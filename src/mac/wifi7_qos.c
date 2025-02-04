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
#include "wifi7_qos.h"
#include "wifi7_mac.h"

/* 
 * Default TID configurations for different traffic types.
 * These settings are based on typical requirements for each traffic class:
 * - Background (BK): Bulk transfers, low priority
 * - Best Effort (BE): Regular data traffic
 * - Video (VI): Streaming media, latency sensitive
 * - Voice (VO): Real-time audio, highest priority
 */
static const struct wifi7_tid_config default_tid_configs[] = {
    /* Background traffic */
    [0] = {
        .tid = 0,
        .ac = WIFI7_AC_BK,
        .queue_limit = 128,
        .ampdu_limit = 64,
        .flags = WIFI7_QOS_FLAG_AMPDU,
        .min_rate = 6,   /* Mbps */
        .max_rate = 0,   /* No limit */
        .target_latency = 100000, /* 100ms */
        .active = true,
    },
    /* Best effort traffic */
    [1] = {
        .tid = 1,
        .ac = WIFI7_AC_BE,
        .queue_limit = 256,
        .ampdu_limit = 128,
        .flags = WIFI7_QOS_FLAG_AMPDU | WIFI7_QOS_FLAG_AMSDU,
        .min_rate = 12,
        .max_rate = 0,
        .target_latency = 50000, /* 50ms */
        .active = true,
    },
    /* Video traffic */
    [2] = {
        .tid = 2,
        .ac = WIFI7_AC_VI,
        .queue_limit = 384,
        .ampdu_limit = 256,
        .flags = WIFI7_QOS_FLAG_AMPDU | WIFI7_QOS_FLAG_BURST,
        .min_rate = 24,
        .max_rate = 0,
        .target_latency = 10000, /* 10ms */
        .active = true,
    },
    /* Voice traffic */
    [3] = {
        .tid = 3,
        .ac = WIFI7_AC_VO,
        .queue_limit = 512,
        .ampdu_limit = 256,
        .flags = WIFI7_QOS_FLAG_AMPDU | WIFI7_QOS_FLAG_LOW_LAT,
        .min_rate = 36,
        .max_rate = 0,
        .target_latency = 5000, /* 5ms */
        .active = true,
    },
    /* Additional TIDs inherit from BE */
    [4] = {
        .tid = 4,
        .ac = WIFI7_AC_BE,
        .queue_limit = 256,
        .ampdu_limit = 128,
        .flags = WIFI7_QOS_FLAG_AMPDU | WIFI7_QOS_FLAG_AMSDU,
        .min_rate = 12,
        .max_rate = 0,
        .target_latency = 50000,
        .active = true,
    },
    [5] = {
        .tid = 5,
        .ac = WIFI7_AC_BE,
        .queue_limit = 256,
        .ampdu_limit = 128,
        .flags = WIFI7_QOS_FLAG_AMPDU | WIFI7_QOS_FLAG_AMSDU,
        .min_rate = 12,
        .max_rate = 0,
        .target_latency = 50000,
        .active = true,
    },
    [6] = {
        .tid = 6,
        .ac = WIFI7_AC_BE,
        .queue_limit = 256,
        .ampdu_limit = 128,
        .flags = WIFI7_QOS_FLAG_AMPDU | WIFI7_QOS_FLAG_AMSDU,
        .min_rate = 12,
        .max_rate = 0,
        .target_latency = 50000,
        .active = true,
    },
    [7] = {
        .tid = 7,
        .ac = WIFI7_AC_BE,
        .queue_limit = 256,
        .ampdu_limit = 128,
        .flags = WIFI7_QOS_FLAG_AMPDU | WIFI7_QOS_FLAG_AMSDU,
        .min_rate = 12,
        .max_rate = 0,
        .target_latency = 50000,
        .active = true,
    },
    /* Management frames */
    [8] = {
        .tid = WIFI7_MGMT_TID,
        .ac = WIFI7_AC_VO,
        .queue_limit = 64,
        .ampdu_limit = 32,
        .flags = WIFI7_QOS_FLAG_NOACK,
        .min_rate = 6,
        .max_rate = 0,
        .target_latency = 10000,
        .active = true,
    },
};

/* Forward declarations for internal functions */
static void wifi7_qos_stats_work(struct work_struct *work);
static int wifi7_qos_update_link_stats(struct wifi7_qos *qos, u8 link_id);
static void wifi7_qos_apply_steering(struct wifi7_qos *qos, struct sk_buff *skb, u8 tid);

/*
 * Helper function to extract TID from an SKB
 * For QoS data frames, uses the priority field
 * For management frames, uses the special management TID
 */
static inline u8 skb_get_tid(struct sk_buff *skb)
{
    struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
    return (le16_to_cpu(hdr->frame_control) & IEEE80211_QOS_CTL) ?
           skb->priority & WIFI7_TID_MASK : WIFI7_MGMT_TID;
}

/*
 * Maps TID to Access Category
 * Used for priority-based scheduling decisions
 */
static inline u8 tid_to_ac(struct wifi7_qos *qos, u8 tid)
{
    return qos->tids[tid].ac;
}

/*
 * Calculates queue ID based on TID and link
 * Each TID has separate queues per link for MLO support
 */
static inline u32 get_queue_id(struct wifi7_qos *qos, u8 tid, u8 link_id)
{
    return qos->queue_mapping[tid] + link_id;
}

/*
 * Queue management functions
 * Handles queue limits and buffer management
 * Maintains statistics for monitoring and optimization
 */
static int wifi7_qos_check_queue_limits(struct wifi7_qos *qos,
                                      u8 tid, u8 link_id)
{
    struct wifi7_tid_config *tid_cfg = &qos->tids[tid];
    struct wifi7_link_qos *link = &qos->links[link_id];
    u32 queue_id = get_queue_id(qos, tid, link_id);
    unsigned long flags;
    int ret = 0;

    spin_lock_irqsave(&link->lock, flags);

    if (skb_queue_len(&qos->queues[queue_id]) >= tid_cfg->queue_limit) {
        link->stats[queue_id].rejected++;
        ret = -ENOSPC;
    }

    if (link->buffer_used >= link->buffer_max) {
        link->drops_overflow++;
        ret = -ENOSPC;
    }

    spin_unlock_irqrestore(&link->lock, flags);
    return ret;
}

/*
 * Updates per-queue and per-link statistics
 * Tracks packet counts, bytes, and latency metrics
 * Used for monitoring QoS performance and steering decisions
 */
static void wifi7_qos_update_stats(struct wifi7_qos *qos,
                                 u8 tid, u8 link_id,
                                 struct sk_buff *skb,
                                 bool is_enqueue)
{
    struct wifi7_link_qos *link = &qos->links[link_id];
    u32 queue_id = get_queue_id(qos, tid, link_id);
    unsigned long flags;
    u32 now = jiffies_to_usecs(jiffies);

    spin_lock_irqsave(&link->lock, flags);

    if (is_enqueue) {
        link->stats[queue_id].enqueued++;
        link->stats[queue_id].bytes_pending += skb->len;
        link->buffer_used += skb->len;
        /* Store enqueue timestamp in skb->tstamp for latency tracking */
        skb->tstamp = ktime_get();
    } else {
        link->stats[queue_id].dequeued++;
        link->stats[queue_id].bytes_pending -= skb->len;
        link->buffer_used -= skb->len;
        /* Calculate sojourn time */
        ktime_t delta = ktime_sub(ktime_get(), skb->tstamp);
        u32 latency = ktime_to_us(delta);
        link->stats[queue_id].avg_latency =
            (link->stats[queue_id].avg_latency + latency) / 2;
        if (latency > link->stats[queue_id].peak_latency)
            link->stats[queue_id].peak_latency = latency;
    }

    spin_unlock_irqrestore(&link->lock, flags);
}

/*
 * Traffic steering implementation
 * Selects best link based on configured policy:
 * - Load balancing
 * - Latency optimization
 * - Airtime fairness
 * - Throughput maximization
 */
static void wifi7_qos_apply_steering(struct wifi7_qos *qos,
                                   struct sk_buff *skb,
                                   u8 tid)
{
    struct wifi7_steer_policy *policy = &qos->steer;
    struct wifi7_tid_config *tid_cfg = &qos->tids[tid];
    u8 best_link = 0;
    u32 best_metric = 0;
    int i;

    if (!policy->active)
        return;

    /* Find best link based on steering policy */
    for (i = 0; i < WIFI7_MAX_LINKS; i++) {
        if (!(tid_cfg->link_mask & BIT(i)))
            continue;

        struct wifi7_link_qos *link = &qos->links[i];
        u32 metric = 0;

        switch (policy->mode) {
        case WIFI7_STEER_LOAD:
            metric = link->buffer_max - link->buffer_used;
            break;
        case WIFI7_STEER_LATENCY:
            metric = policy->latency_threshold -
                    link->stats[get_queue_id(qos, tid, i)].avg_latency;
            break;
        case WIFI7_STEER_AIRTIME:
            metric = policy->airtime_threshold -
                    (link->tx_airtime + link->rx_airtime);
            break;
        case WIFI7_STEER_THROUGHPUT:
            metric = link->current_rate;
            break;
        default:
            return;
        }

        metric *= policy->link_weight[i];

        if (metric > best_metric) {
            best_metric = metric;
            best_link = i;
        }
    }

    /* Update TID-to-link mapping */
    tid_cfg->link_mask = BIT(best_link);
}

/*
 * Multi-TID aggregation helper
 * Checks if a packet can be aggregated based on:
 * - Current aggregation state
 * - TID limits
 * - Buffer availability
 */
static bool wifi7_qos_can_aggregate(struct wifi7_qos *qos,
                                  struct sk_buff *skb,
                                  u8 tid, u8 link_id)
{
    struct wifi7_mtid_state *mtid = &qos->mtid;
    struct wifi7_tid_config *tid_cfg = &qos->tids[tid];
    unsigned long flags;
    bool ret = false;

    if (!mtid->enabled || !(tid_cfg->flags & WIFI7_QOS_FLAG_AMPDU))
        return false;

    spin_lock_irqsave(&mtid->lock, flags);

    if (mtid->active_tids < mtid->max_tids &&
        skb->len <= mtid->max_ampdu &&
        !(mtid->tid_bitmap & BIT(tid))) {
        mtid->tid_bitmap |= BIT(tid);
        mtid->active_tids++;
        ret = true;
    }

    spin_unlock_irqrestore(&mtid->lock, flags);
    return ret;
}

/* Public API Implementation */

/*
 * Initializes QoS subsystem
 * Sets up queues, locks, and default configurations
 * Starts statistics collection
 */
int wifi7_qos_init(struct wifi7_dev *dev)
{
    struct wifi7_qos *qos;
    int i, ret;

    qos = kzalloc(sizeof(*qos), GFP_KERNEL);
    if (!qos)
        return -ENOMEM;

    /* Initialize locks */
    spin_lock_init(&qos->tid_lock);
    spin_lock_init(&qos->mtid.lock);

    for (i = 0; i < WIFI7_MAX_LINKS; i++)
        spin_lock_init(&qos->links[i].lock);

    for (i = 0; i < WIFI7_MAX_QUEUES; i++) {
        spin_lock_init(&qos->queue_locks[i]);
        skb_queue_head_init(&qos->queues[i]);
    }

    /* Initialize TID configurations */
    memcpy(qos->tids, default_tid_configs,
           sizeof(default_tid_configs));

    /* Initialize Multi-TID state */
    qos->mtid.enabled = false;
    qos->mtid.max_tids = WIFI7_MAX_AMPDU_TIDS;
    qos->mtid.max_ampdu = WIFI7_MAX_AMPDU_LEN;
    qos->mtid.timeout_us = 10000; /* 10ms */

    /* Initialize queue mapping */
    for (i = 0; i < WIFI7_NUM_TIDS; i++)
        qos->queue_mapping[i] = i * WIFI7_MAX_LINKS;

    /* Initialize statistics collection */
    INIT_DELAYED_WORK(&qos->stats_work, wifi7_qos_stats_work);
    qos->update_interval = HZ; /* 1 second */
    qos->stats_enabled = true;

    /* Start statistics collection */
    schedule_delayed_work(&qos->stats_work, qos->update_interval);

    dev->qos = qos;
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_qos_init);

void wifi7_qos_deinit(struct wifi7_dev *dev)
{
    struct wifi7_qos *qos = dev->qos;
    int i;

    if (!qos)
        return;

    /* Stop statistics collection */
    cancel_delayed_work_sync(&qos->stats_work);

    /* Flush all queues */
    for (i = 0; i < WIFI7_MAX_QUEUES; i++)
        skb_queue_purge(&qos->queues[i]);

    kfree(qos);
    dev->qos = NULL;
}
EXPORT_SYMBOL_GPL(wifi7_qos_deinit);

int wifi7_qos_setup_tid(struct wifi7_dev *dev,
                       struct wifi7_tid_config *config)
{
    struct wifi7_qos *qos = dev->qos;
    unsigned long flags;

    if (!qos || !config || config->tid >= WIFI7_NUM_TIDS)
        return -EINVAL;

    spin_lock_irqsave(&qos->tid_lock, flags);
    memcpy(&qos->tids[config->tid], config, sizeof(*config));
    spin_unlock_irqrestore(&qos->tid_lock, flags);

    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_qos_setup_tid);

int wifi7_qos_enable_mtid(struct wifi7_dev *dev,
                         u8 max_tids, u32 timeout_us)
{
    struct wifi7_qos *qos = dev->qos;
    unsigned long flags;

    if (!qos || max_tids > WIFI7_MAX_AMPDU_TIDS)
        return -EINVAL;

    spin_lock_irqsave(&qos->mtid.lock, flags);
    qos->mtid.enabled = true;
    qos->mtid.max_tids = max_tids;
    qos->mtid.timeout_us = timeout_us;
    qos->mtid.active_tids = 0;
    qos->mtid.tid_bitmap = 0;
    spin_unlock_irqrestore(&qos->mtid.lock, flags);

    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_qos_enable_mtid);

int wifi7_qos_set_link_mask(struct wifi7_dev *dev,
                           u8 tid, u8 link_mask)
{
    struct wifi7_qos *qos = dev->qos;
    unsigned long flags;

    if (!qos || tid >= WIFI7_NUM_TIDS)
        return -EINVAL;

    spin_lock_irqsave(&qos->tid_lock, flags);
    qos->tids[tid].link_mask = link_mask;
    spin_unlock_irqrestore(&qos->tid_lock, flags);

    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_qos_set_link_mask);

int wifi7_qos_enqueue(struct wifi7_dev *dev,
                     struct sk_buff *skb, u8 tid)
{
    struct wifi7_qos *qos = dev->qos;
    struct wifi7_tid_config *tid_cfg;
    u8 link_id;
    int ret;

    if (!qos || !skb || tid >= WIFI7_NUM_TIDS)
        return -EINVAL;

    tid_cfg = &qos->tids[tid];
    if (!tid_cfg->active)
        return -EINVAL;

    /* Apply traffic steering */
    wifi7_qos_apply_steering(qos, skb, tid);

    /* Get target link */
    link_id = ffs(tid_cfg->link_mask) - 1;
    if (link_id >= WIFI7_MAX_LINKS)
        return -EINVAL;

    /* Check queue limits */
    ret = wifi7_qos_check_queue_limits(qos, tid, link_id);
    if (ret)
        return ret;

    /* Try multi-TID aggregation */
    if (wifi7_qos_can_aggregate(qos, skb, tid, link_id))
        skb->mark |= WIFI7_QOS_FLAG_AMPDU;

    /* Enqueue packet */
    skb_queue_tail(&qos->queues[get_queue_id(qos, tid, link_id)], skb);
    wifi7_qos_update_stats(qos, tid, link_id, skb, true);

    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_qos_enqueue);

struct sk_buff *wifi7_qos_dequeue(struct wifi7_dev *dev, u8 link_id)
{
    struct wifi7_qos *qos = dev->qos;
    struct sk_buff *skb;
    u8 tid, ac;
    int i;

    if (!qos || link_id >= WIFI7_MAX_LINKS)
        return NULL;

    /* Priority-based dequeue */
    for (ac = WIFI7_AC_VO; ac >= WIFI7_AC_BK; ac--) {
        for (i = 0; i < WIFI7_NUM_TIDS; i++) {
            tid = i;
            if (tid_to_ac(qos, tid) != ac)
                continue;

            if (!(qos->tids[tid].link_mask & BIT(link_id)))
                continue;

            skb = skb_dequeue(&qos->queues[get_queue_id(qos, tid, link_id)]);
            if (skb) {
                wifi7_qos_update_stats(qos, tid, link_id, skb, false);
                return skb;
            }
        }
    }

    return NULL;
}
EXPORT_SYMBOL_GPL(wifi7_qos_dequeue);

void wifi7_qos_flush(struct wifi7_dev *dev, u8 tid)
{
    struct wifi7_qos *qos = dev->qos;
    struct wifi7_tid_config *tid_cfg;
    int i;

    if (!qos || tid >= WIFI7_NUM_TIDS)
        return;

    tid_cfg = &qos->tids[tid];

    for (i = 0; i < WIFI7_MAX_LINKS; i++) {
        if (tid_cfg->link_mask & BIT(i))
            skb_queue_purge(&qos->queues[get_queue_id(qos, tid, i)]);
    }
}
EXPORT_SYMBOL_GPL(wifi7_qos_flush);

int wifi7_qos_set_steering(struct wifi7_dev *dev,
                          struct wifi7_steer_policy *policy)
{
    struct wifi7_qos *qos = dev->qos;

    if (!qos || !policy)
        return -EINVAL;

    memcpy(&qos->steer, policy, sizeof(*policy));
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_qos_set_steering);

int wifi7_qos_get_stats(struct wifi7_dev *dev, u8 link_id,
                       struct wifi7_queue_stats *stats)
{
    struct wifi7_qos *qos = dev->qos;
    struct wifi7_link_qos *link;
    unsigned long flags;

    if (!qos || !stats || link_id >= WIFI7_MAX_LINKS)
        return -EINVAL;

    link = &qos->links[link_id];

    spin_lock_irqsave(&link->lock, flags);
    memcpy(stats, link->stats, sizeof(link->stats));
    spin_unlock_irqrestore(&link->lock, flags);

    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_qos_get_stats);

/* Statistics worker */
static void wifi7_qos_stats_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct wifi7_qos *qos = container_of(dwork, struct wifi7_qos, stats_work);
    int i;

    if (!qos->stats_enabled)
        return;

    for (i = 0; i < WIFI7_MAX_LINKS; i++)
        wifi7_qos_update_link_stats(qos, i);

    schedule_delayed_work(&qos->stats_work, qos->update_interval);
}

static int wifi7_qos_update_link_stats(struct wifi7_qos *qos, u8 link_id)
{
    struct wifi7_link_qos *link = &qos->links[link_id];
    unsigned long flags;
    int i;

    spin_lock_irqsave(&link->lock, flags);

    /* Update queue statistics */
    for (i = 0; i < WIFI7_MAX_QUEUES; i++) {
        struct wifi7_queue_stats *stats = &link->stats[i];
        stats->avg_sojourn = stats->avg_latency;
    }

    /* Update link statistics */
    link->tx_bytes = 0;
    link->rx_bytes = 0;
    link->tx_mpdu = 0;
    link->rx_mpdu = 0;
    link->tx_ampdu = 0;
    link->rx_ampdu = 0;

    spin_unlock_irqrestore(&link->lock, flags);

    return 0;
}

/*
 * Module initialization and cleanup
 */
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