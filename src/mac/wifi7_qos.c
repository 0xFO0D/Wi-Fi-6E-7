/* 
 * WiFi 7 QoS Management
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 *
 * This module implements Quality of Service (QoS) and traffic management
 * for the WiFi 7 driver, including:
 * - Traffic classification and prioritization
 * - Multi-TID aggregation
 * - Dynamic traffic steering
 * - Queue management and statistics
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/math64.h>
#include <net/dsfield.h>
#include "wifi7_qos.h"
#include "wifi7_mac.h"

/* Token bucket parameters */
#define WIFI7_TOKEN_SHIFT      20
#define WIFI7_TOKEN_SCALE      (1 << WIFI7_TOKEN_SHIFT)
#define WIFI7_MIN_BURST        1500
#define WIFI7_MAX_BURST        32768
#define WIFI7_MIN_RATE_BPS     64000
#define WIFI7_MAX_RATE_BPS     1000000000

/* Traffic shaping state */
struct wifi7_shaper {
    u64 tokens;
    u32 rate;
    u32 burst;
    u32 mpu;
    u32 overhead;
    ktime_t last_update;
    spinlock_t lock;
};

/* Rate control state */
struct wifi7_rate_ctrl {
    u32 target_rate;
    u32 current_rate;
    u32 max_rate;
    u32 min_rate;
    u8 mcs_idx;
    u8 nss;
    u8 bw;
    u8 gi;
    u32 pkt_count;
    u32 err_count;
    u32 retry_count;
    ktime_t last_update;
    bool ml_active;
};

/* Enhanced link state */
struct wifi7_link_state {
    struct wifi7_shaper shaper;
    struct wifi7_rate_ctrl rate;
    u32 airtime_used;
    u32 airtime_total;
    u32 tx_bytes;
    u32 rx_bytes;
    u32 tx_packets;
    u32 rx_packets;
    u32 dropped;
    u32 retries;
    u32 avg_q_depth;
    u32 peak_q_depth;
    u32 buffer_used;
    u32 buffer_max;
    ktime_t last_active;
    bool congested;
};

/* MLO link prediction state */
struct wifi7_mlo_predict {
    u32 success_hist[WIFI7_MAX_LINKS];
    u32 fail_hist[WIFI7_MAX_LINKS];
    u32 latency_hist[WIFI7_MAX_LINKS];
    u32 tput_hist[WIFI7_MAX_LINKS];
    u32 pred_weights[WIFI7_MAX_LINKS];
    ktime_t last_update;
    spinlock_t lock;
};

/* Per-TID state tracking */
struct wifi7_tid_state {
    struct wifi7_shaper shaper;
    struct wifi7_rate_ctrl rate;
    u32 packets_in_flight;
    u32 bytes_in_flight;
    u32 queue_len;
    u32 dropped;
    u32 completed;
    u32 retried;
    u32 avg_sojourn;
    u32 peak_sojourn;
    ktime_t last_pkt_ts;
    bool active;
};

/* Main QoS structure */
struct wifi7_qos {
    /* Enhanced state tracking */
    struct wifi7_link_state links[WIFI7_MAX_LINKS];
    struct wifi7_tid_state tids[WIFI7_NUM_TIDS];
    struct wifi7_mlo_predict mlo;
    
    /* DRR scheduling */
    u32 quantum[WIFI7_NUM_TIDS];
    u32 deficit[WIFI7_NUM_TIDS];
    
    /* Power management */
    bool power_save;
    u32 ps_timeout;
    u32 ps_threshold;
    
    /* Statistics */
    struct {
        u64 bytes_tx;
        u64 bytes_rx;
        u32 pkts_tx;
        u32 pkts_rx;
        u32 dropped;
        u32 retried;
        u32 avg_lat;
        u32 peak_lat;
    } stats;
    
    /* Locks and synchronization */
    spinlock_t lock;
    struct mutex conf_lock;
    
    /* Work items */
    struct delayed_work stats_work;
    struct delayed_work tune_work;
    
    /* Configuration */
    bool active;
    bool ml_enabled;
    u32 update_interval;
};

/* Token bucket implementation */
static void wifi7_shaper_update_tokens(struct wifi7_shaper *sh)
{
    ktime_t now = ktime_get();
    u64 elapsed = ktime_us_delta(now, sh->last_update);
    u64 tokens;
    
    tokens = elapsed * sh->rate >> WIFI7_TOKEN_SHIFT;
    sh->tokens = min_t(u64, sh->tokens + tokens, sh->burst << WIFI7_TOKEN_SHIFT);
    sh->last_update = now;
}

static bool wifi7_shaper_allow(struct wifi7_shaper *sh, u32 size)
{
    unsigned long flags;
    bool allow;
    
    size = max_t(u32, size + sh->overhead, sh->mpu);
    
    spin_lock_irqsave(&sh->lock, flags);
    wifi7_shaper_update_tokens(sh);
    
    allow = size <= (sh->tokens >> WIFI7_TOKEN_SHIFT);
    if (allow)
        sh->tokens -= (u64)size << WIFI7_TOKEN_SHIFT;
        
    spin_unlock_irqrestore(&sh->lock, flags);
    return allow;
}

/* MLO link prediction */
static void wifi7_mlo_update_prediction(struct wifi7_qos *qos)
{
    struct wifi7_mlo_predict *mlo = &qos->mlo;
    unsigned long flags;
    int i;
    
    spin_lock_irqsave(&mlo->lock, flags);
    
    /* Update prediction weights based on history */
    for (i = 0; i < WIFI7_MAX_LINKS; i++) {
        u32 success_rate = mlo->success_hist[i] * 100 /
                          (mlo->success_hist[i] + mlo->fail_hist[i] + 1);
        u32 latency_score = 100 - (mlo->latency_hist[i] * 100 / 
                           (qos->stats.peak_lat + 1));
        u32 tput_score = mlo->tput_hist[i] * 100 /
                        (qos->stats.bytes_tx / 1024 + 1);
                        
        mlo->pred_weights[i] = (success_rate + latency_score + tput_score) / 3;
    }
    
    spin_unlock_irqrestore(&mlo->lock, flags);
}

/* Rate control */
static void wifi7_rate_update(struct wifi7_rate_ctrl *rc, 
                            bool success, u32 rate, u8 retries)
{
    ktime_t now = ktime_get();
    u32 interval = ktime_ms_delta(now, rc->last_update);
    
    rc->pkt_count++;
    if (!success) {
        rc->err_count++;
        rc->retry_count += retries;
    }
    
    if (interval < 100)
        return;
        
    /* Adjust MCS based on error rate and retries */
    if (rc->err_count * 100 / rc->pkt_count > 10 ||
        rc->retry_count * 100 / rc->pkt_count > 20) {
        if (rc->mcs_idx > 0)
            rc->mcs_idx--;
    } else if (rc->err_count == 0 && rc->retry_count == 0) {
        if (rc->mcs_idx < 11)  /* MCS11 for WiFi 7 */
            rc->mcs_idx++;
    }
    
    /* Update rate targets */
    rc->current_rate = min_t(u32, rate, rc->max_rate);
    if (rc->ml_active)
        rc->target_rate = rc->current_rate * 3 / 2;
    else
        rc->target_rate = rc->current_rate;
        
    rc->pkt_count = 0;
    rc->err_count = 0;
    rc->retry_count = 0;
    rc->last_update = now;
}

/* Queue management with DRR */
static struct sk_buff *wifi7_drr_dequeue(struct wifi7_qos *qos, u8 link_id)
{
    struct sk_buff *skb = NULL;
    int i;
    
    for (i = 0; i < WIFI7_NUM_TIDS; i++) {
        struct wifi7_tid_state *ts = &qos->tids[i];
        
        if (!ts->active || ts->queue_len == 0)
            continue;
            
        qos->deficit[i] += qos->quantum[i];
        while (qos->deficit[i] > 0 && ts->queue_len > 0) {
            skb = skb_dequeue(&qos->links[link_id].queues[i]);
            if (!skb)
                break;
                
            qos->deficit[i] -= skb->len;
            ts->queue_len--;
            ts->bytes_in_flight += skb->len;
            ts->packets_in_flight++;
            
            /* Apply traffic shaping */
            if (!wifi7_shaper_allow(&ts->shaper, skb->len)) {
                skb_queue_head(&qos->links[link_id].queues[i], skb);
                ts->queue_len++;
                ts->bytes_in_flight -= skb->len;
                ts->packets_in_flight--;
                goto next_tid;
            }
            
            return skb;
        }
        
next_tid:
        if (qos->deficit[i] <= 0)
            qos->deficit[i] = 0;
    }
    
    return NULL;
}

/* Power management */
static void wifi7_power_update(struct wifi7_qos *qos)
{
    bool active = false;
    int i;
    
    if (!qos->power_save)
        return;
        
    for (i = 0; i < WIFI7_NUM_TIDS; i++) {
        struct wifi7_tid_state *ts = &qos->tids[i];
        if (ts->active && ts->queue_len > qos->ps_threshold) {
            active = true;
            break;
        }
    }
    
    if (active) {
        /* Wake up radio */
        // TODO: Implement radio power state control
    } else {
        /* Schedule sleep after timeout */
        // TODO: Implement delayed sleep
    }
}

/* Statistics collection */
static void wifi7_update_stats(struct wifi7_qos *qos)
{
    int i;
    
    for (i = 0; i < WIFI7_MAX_LINKS; i++) {
        struct wifi7_link_state *ls = &qos->links[i];
        
        qos->stats.bytes_tx += ls->tx_bytes;
        qos->stats.bytes_rx += ls->rx_bytes;
        qos->stats.pkts_tx += ls->tx_packets;
        qos->stats.pkts_rx += ls->rx_packets;
        qos->stats.dropped += ls->dropped;
        qos->stats.retried += ls->retries;
        
        ls->tx_bytes = 0;
        ls->rx_bytes = 0;
        ls->tx_packets = 0;
        ls->rx_packets = 0;
        ls->dropped = 0;
        ls->retries = 0;
    }
}

/* Periodic work handlers */
static void wifi7_stats_work(struct work_struct *work)
{
    struct wifi7_qos *qos = container_of(to_delayed_work(work),
                                       struct wifi7_qos, stats_work);
                                       
    wifi7_update_stats(qos);
    wifi7_mlo_update_prediction(qos);
    
    if (qos->active)
        schedule_delayed_work(&qos->stats_work, 
                            msecs_to_jiffies(qos->update_interval));
}

static void wifi7_tune_work(struct work_struct *work)
{
    struct wifi7_qos *qos = container_of(to_delayed_work(work),
                                       struct wifi7_qos, tune_work);
    int i;
    
    /* Tune shapers based on link conditions */
    for (i = 0; i < WIFI7_NUM_TIDS; i++) {
        struct wifi7_tid_state *ts = &qos->tids[i];
        if (ts->active) {
            ts->shaper.rate = ts->rate.target_rate;
            ts->shaper.burst = min_t(u32, ts->rate.target_rate / 4,
                                   WIFI7_MAX_BURST);
        }
    }
    
    if (qos->active)
        schedule_delayed_work(&qos->tune_work, HZ);
}

/* Initialization */
int wifi7_qos_init(struct wifi7_dev *dev)
{
    struct wifi7_qos *qos;
    int i;
    
    qos = kzalloc(sizeof(*qos), GFP_KERNEL);
    if (!qos)
        return -ENOMEM;
        
    spin_lock_init(&qos->lock);
    mutex_init(&qos->conf_lock);
    spin_lock_init(&qos->mlo.lock);
    
    /* Initialize shapers */
    for (i = 0; i < WIFI7_NUM_TIDS; i++) {
        struct wifi7_tid_state *ts = &qos->tids[i];
        spin_lock_init(&ts->shaper.lock);
        ts->shaper.rate = WIFI7_MIN_RATE_BPS;
        ts->shaper.burst = WIFI7_MIN_BURST;
        ts->shaper.mpu = 256;
        ts->shaper.overhead = 24;  /* MAC header */
        ts->shaper.last_update = ktime_get();
    }
    
    /* Initialize DRR */
    for (i = 0; i < WIFI7_NUM_TIDS; i++) {
        qos->quantum[i] = 256 << (i / 2);  /* Exponential quantum */
        qos->deficit[i] = 0;
    }
    
    /* Initialize work items */
    INIT_DELAYED_WORK(&qos->stats_work, wifi7_stats_work);
    INIT_DELAYED_WORK(&qos->tune_work, wifi7_tune_work);
    
    qos->update_interval = 100;  /* 100ms */
    qos->active = true;
    qos->ml_enabled = true;
    
    schedule_delayed_work(&qos->stats_work, 
                         msecs_to_jiffies(qos->update_interval));
    schedule_delayed_work(&qos->tune_work, HZ);
    
    dev->qos = qos;
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_qos_init);

void wifi7_qos_deinit(struct wifi7_dev *dev)
{
    struct wifi7_qos *qos = dev->qos;
    
    if (!qos)
        return;
        
    qos->active = false;
    cancel_delayed_work_sync(&qos->stats_work);
    cancel_delayed_work_sync(&qos->tune_work);
    
    mutex_destroy(&qos->conf_lock);
    kfree(qos);
    dev->qos = NULL;
}
EXPORT_SYMBOL_GPL(wifi7_qos_deinit);

/* Module init/exit */
static int __init wifi7_qos_init_module(void)
{
    pr_info("WiFi 7 QoS Management initialized\n");
    return 0;
}

static void __exit wifi7_qos_exit_module(void)
{
    pr_info("WiFi 7 QoS Management unloaded\n");
}

module_init(wifi7_qos_init_module);
module_exit(wifi7_qos_exit_module);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 QoS Management");
MODULE_VERSION("1.0"); 