/*
 * WiFi 7 Multi-Link Operation Management
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include <linux/random.h>
#include <linux/crc32.h>
#include <linux/completion.h>
#include <linux/ktime.h>
#include <net/mac80211.h>
#include "wifi7_mlo.h"
#include "wifi7_mac.h"
#include "../hal/wifi7_rf.h"
#include "../../include/core/wifi67.h"
#include "../../include/core/mlo.h"

/* MLO device state */
struct wifi7_mlo {
    /* Configuration */
    struct wifi7_mlo_config config;
    struct wifi7_mlo_stats stats;
    
    /* Link management */
    struct {
        struct wifi7_mlo_link_config links[WIFI7_MAX_LINKS];
        struct wifi7_mlo_metrics metrics[WIFI7_MAX_LINKS];
        u8 active_link;
        spinlock_t lock;
    } link;
    
    /* TID mapping */
    struct {
        struct wifi7_mlo_tid_map maps[WIFI7_NUM_TIDS];
        spinlock_t lock;
    } tid;
    
    /* Frame handling */
    struct {
        struct sk_buff_head tx_queue;
        struct sk_buff_head rx_queue;
        spinlock_t tx_lock;
        spinlock_t rx_lock;
        struct delayed_work tx_work;
        struct delayed_work rx_work;
    } frames;
    
    /* Link selection */
    struct {
        u8 policy;
        u32 interval;
        struct delayed_work work;
    } select;
    
    /* Metrics collection */
    struct {
        u32 interval;
        struct delayed_work work;
    } metrics;
    
    /* Power management */
    struct {
        bool enabled;
        u32 timeout;
        struct delayed_work work;
    } power;
    
    /* Debugging */
    struct dentry *debugfs_dir;
    bool debug_enabled;
};

/* Link selection algorithms */
static u8 wifi7_mlo_select_rssi(struct wifi7_mlo *mlo)
{
    struct wifi7_mlo_metrics *metrics;
    u8 best_link = mlo->link.active_link;
    u32 best_rssi = 0;
    int i;
    
    for (i = 0; i < mlo->config.num_links; i++) {
        if (!mlo->config.links[i].enabled)
            continue;
            
        metrics = &mlo->link.metrics[i];
        if (metrics->rssi > best_rssi) {
            best_rssi = metrics->rssi;
            best_link = i;
        }
    }
    
    return best_link;
}

static u8 wifi7_mlo_select_load(struct wifi7_mlo *mlo)
{
    struct wifi7_mlo_metrics *metrics;
    u8 best_link = mlo->link.active_link;
    u32 best_load = UINT_MAX;
    int i;
    
    for (i = 0; i < mlo->config.num_links; i++) {
        if (!mlo->config.links[i].enabled)
            continue;
            
        metrics = &mlo->link.metrics[i];
        u32 load = metrics->airtime;
        
        if (load < best_load) {
            best_load = load;
            best_link = i;
        }
    }
    
    return best_link;
}

static u8 wifi7_mlo_select_latency(struct wifi7_mlo *mlo)
{
    struct wifi7_mlo_metrics *metrics;
    u8 best_link = mlo->link.active_link;
    u32 best_latency = UINT_MAX;
    int i;
    
    for (i = 0; i < mlo->config.num_links; i++) {
        if (!mlo->config.links[i].enabled)
            continue;
            
        metrics = &mlo->link.metrics[i];
        if (metrics->latency < best_latency) {
            best_latency = metrics->latency;
            best_link = i;
        }
    }
    
    return best_link;
}

static u8 wifi7_mlo_select_ml(struct wifi7_mlo *mlo)
{
    struct wifi7_mlo_metrics *metrics;
    u8 best_link = mlo->link.active_link;
    u32 best_score = 0;
    int i;
    
    for (i = 0; i < mlo->config.num_links; i++) {
        if (!mlo->config.links[i].enabled)
            continue;
            
        metrics = &mlo->link.metrics[i];
        
        /* Calculate ML score using various metrics */
        u32 score = 0;
        score += metrics->rssi * 2;
        score += (1000 - metrics->latency) * 3;
        score += (100 - metrics->loss) * 4;
        score += metrics->tx_rate * 2;
        score += (100 - metrics->airtime) * 3;
        
        if (score > best_score) {
            best_score = score;
            best_link = i;
        }
    }
    
    return best_link;
}

/* Link selection work handler */
static void wifi7_mlo_select_work(struct work_struct *work)
{
    struct wifi7_mlo *mlo = container_of(to_delayed_work(work),
                                       struct wifi7_mlo, select.work);
    u8 new_link;
    
    /* Select best link based on policy */
    switch (mlo->select.policy) {
    case WIFI7_MLO_SELECT_RSSI:
        new_link = wifi7_mlo_select_rssi(mlo);
        break;
    case WIFI7_MLO_SELECT_LOAD:
        new_link = wifi7_mlo_select_load(mlo);
        break;
    case WIFI7_MLO_SELECT_LAT:
        new_link = wifi7_mlo_select_latency(mlo);
        break;
    case WIFI7_MLO_SELECT_ML:
        new_link = wifi7_mlo_select_ml(mlo);
        break;
    default:
        new_link = mlo->link.active_link;
        break;
    }
    
    /* Switch link if needed */
    if (new_link != mlo->link.active_link) {
        ktime_t start = ktime_get();
        
        if (wifi7_mlo_switch_link(mlo->dev, new_link) == 0) {
            mlo->link.active_link = new_link;
            mlo->stats.link_switches++;
            mlo->stats.switch_latency = ktime_us_delta(ktime_get(), start);
        } else {
            mlo->stats.link_failures++;
        }
    }
    
    /* Schedule next selection */
    if (mlo->config.mode != WIFI7_MLO_MODE_DISABLED)
        schedule_delayed_work(&mlo->select.work,
                            msecs_to_jiffies(mlo->select.interval));
}

/* Metrics collection work handler */
static void wifi7_mlo_metrics_work(struct work_struct *work)
{
    struct wifi7_mlo *mlo = container_of(to_delayed_work(work),
                                       struct wifi7_mlo, metrics.work);
    int i;
    
    /* Update metrics for all links */
    for (i = 0; i < mlo->config.num_links; i++) {
        if (!mlo->config.links[i].enabled)
            continue;
            
        /* Collect hardware metrics */
        // TODO: Implement hardware metrics collection
        
        /* Update moving averages */
        struct wifi7_mlo_metrics *metrics = &mlo->link.metrics[i];
        metrics->rssi = (metrics->rssi * 7 + new_rssi) / 8;
        metrics->noise = (metrics->noise * 7 + new_noise) / 8;
        metrics->latency = (metrics->latency * 7 + new_latency) / 8;
        metrics->jitter = (metrics->jitter * 7 + new_jitter) / 8;
        metrics->loss = (metrics->loss * 7 + new_loss) / 8;
    }
    
    /* Schedule next collection */
    if (mlo->config.mode != WIFI7_MLO_MODE_DISABLED)
        schedule_delayed_work(&mlo->metrics.work,
                            msecs_to_jiffies(mlo->metrics.interval));
}

/* Power management work handler */
static void wifi7_mlo_power_work(struct work_struct *work)
{
    struct wifi7_mlo *mlo = container_of(to_delayed_work(work),
                                       struct wifi7_mlo, power.work);
    int i;
    
    if (!mlo->power.enabled)
        return;
        
    /* Check link activity */
    for (i = 0; i < mlo->config.num_links; i++) {
        if (!mlo->config.links[i].enabled)
            continue;
            
        struct wifi7_mlo_metrics *metrics = &mlo->link.metrics[i];
        
        /* Put idle links to sleep */
        if (metrics->tx_packets == 0 && metrics->rx_packets == 0) {
            // TODO: Implement link power save
        }
    }
    
    /* Schedule next check */
    schedule_delayed_work(&mlo->power.work,
                         msecs_to_jiffies(mlo->power.timeout));
}

/* Initialization */
int wifi7_mlo_init(struct wifi7_dev *dev)
{
    struct wifi7_mlo *mlo;
    int ret;
    
    mlo = kzalloc(sizeof(*mlo), GFP_KERNEL);
    if (!mlo)
        return -ENOMEM;
        
    /* Initialize locks */
    spin_lock_init(&mlo->link.lock);
    spin_lock_init(&mlo->tid.lock);
    spin_lock_init(&mlo->frames.tx_lock);
    spin_lock_init(&mlo->frames.rx_lock);
    
    /* Initialize queues */
    skb_queue_head_init(&mlo->frames.tx_queue);
    skb_queue_head_init(&mlo->frames.rx_queue);
    
    /* Initialize work items */
    INIT_DELAYED_WORK(&mlo->select.work, wifi7_mlo_select_work);
    INIT_DELAYED_WORK(&mlo->metrics.work, wifi7_mlo_metrics_work);
    INIT_DELAYED_WORK(&mlo->power.work, wifi7_mlo_power_work);
    
    /* Set default configuration */
    mlo->config.mode = WIFI7_MLO_MODE_EMLSR;
    mlo->config.capabilities = WIFI7_MLO_CAP_EMLSR |
                             WIFI7_MLO_CAP_TID_8 |
                             WIFI7_MLO_CAP_LINK_4 |
                             WIFI7_MLO_CAP_SWITCH;
    mlo->config.num_links = 4;
    mlo->config.active_links = 1;
    mlo->config.selection_policy = WIFI7_MLO_SELECT_ML;
    mlo->config.power_save = true;
    mlo->config.spatial_reuse = true;
    
    /* Set work intervals */
    mlo->select.interval = 100;  /* 100ms */
    mlo->metrics.interval = 50;  /* 50ms */
    mlo->power.timeout = 1000;   /* 1s */
    
    dev->mlo = mlo;
    mlo->dev = dev;
    
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_mlo_init);

void wifi7_mlo_deinit(struct wifi7_dev *dev)
{
    struct wifi7_mlo *mlo = dev->mlo;
    
    if (!mlo)
        return;
        
    /* Cancel work items */
    cancel_delayed_work_sync(&mlo->select.work);
    cancel_delayed_work_sync(&mlo->metrics.work);
    cancel_delayed_work_sync(&mlo->power.work);
    
    /* Flush queues */
    skb_queue_purge(&mlo->frames.tx_queue);
    skb_queue_purge(&mlo->frames.rx_queue);
    
    kfree(mlo);
    dev->mlo = NULL;
}
EXPORT_SYMBOL_GPL(wifi7_mlo_deinit);

/* Module initialization */
static int __init wifi7_mlo_init_module(void)
{
    pr_info("WiFi 7 MLO Management initialized\n");
    return 0;
}

static void __exit wifi7_mlo_exit_module(void)
{
    pr_info("WiFi 7 MLO Management unloaded\n");
}

module_init(wifi7_mlo_init_module);
module_exit(wifi7_mlo_exit_module);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 MLO Management");
MODULE_VERSION("1.0");

/* EMLMR state machine states */
#define EMLMR_STATE_IDLE      0
#define EMLMR_STATE_SETUP     1
#define EMLMR_STATE_ACTIVE    2
#define EMLMR_STATE_TEARDOWN  3
#define EMLMR_STATE_ERROR     4

struct wifi7_emlmr_state {
    u8 state;
    u8 num_active_links;
    u32 active_link_mask;
    u8 primary_link;
    u8 transition_delay;
    bool pad_present;
    u32 capabilities;
    spinlock_t lock;
};

static struct wifi7_emlmr_state emlmr_state;

static int wifi7_emlmr_setup_link(struct wifi7_dev *dev, u8 link_id)
{
    struct wifi7_mlo_link_config *link;
    int ret;

    link = &dev->mlo.config.links[link_id];
    if (!link->enabled)
        return -EINVAL;

    ret = wifi7_rf_setup_link(dev, link_id, link->band,
                             link->center_freq, link->width,
                             link->primary_chan, link->nss);
    if (ret)
        return ret;

    emlmr_state.active_link_mask |= BIT(link_id);
    emlmr_state.num_active_links++;

    return 0;
}

static void wifi7_emlmr_teardown_link(struct wifi7_dev *dev, u8 link_id)
{
    wifi7_rf_teardown_link(dev, link_id);
    emlmr_state.active_link_mask &= ~BIT(link_id);
    emlmr_state.num_active_links--;
}

int wifi7_emlmr_init(struct wifi7_dev *dev)
{
    spin_lock_init(&emlmr_state.lock);
    emlmr_state.state = EMLMR_STATE_IDLE;
    emlmr_state.num_active_links = 0;
    emlmr_state.active_link_mask = 0;
    emlmr_state.primary_link = 0;
    emlmr_state.transition_delay = 0;
    emlmr_state.pad_present = false;
    emlmr_state.capabilities = dev->mlo.config.capabilities;

    return 0;
}

void wifi7_emlmr_deinit(struct wifi7_dev *dev)
{
    unsigned long flags;
    u8 link_id;

    spin_lock_irqsave(&emlmr_state.lock, flags);

    if (emlmr_state.state == EMLMR_STATE_ACTIVE) {
        for_each_set_bit(link_id, (unsigned long *)&emlmr_state.active_link_mask,
                        WIFI7_MAX_LINKS) {
            wifi7_emlmr_teardown_link(dev, link_id);
        }
    }

    emlmr_state.state = EMLMR_STATE_IDLE;
    spin_unlock_irqrestore(&emlmr_state.lock, flags);
}

int wifi7_emlmr_start(struct wifi7_dev *dev)
{
    unsigned long flags;
    int ret = 0;
    u8 link_id;

    spin_lock_irqsave(&emlmr_state.lock, flags);

    if (!(emlmr_state.capabilities & WIFI7_MLO_CAP_EMLMR)) {
        ret = -ENOTSUP;
        goto out;
    }

    if (emlmr_state.state != EMLMR_STATE_IDLE) {
        ret = -EBUSY;
        goto out;
    }

    emlmr_state.state = EMLMR_STATE_SETUP;

    for (link_id = 0; link_id < dev->mlo.config.num_links; link_id++) {
        if (dev->mlo.config.links[link_id].enabled) {
            ret = wifi7_emlmr_setup_link(dev, link_id);
            if (ret) {
                emlmr_state.state = EMLMR_STATE_ERROR;
                goto out;
            }
        }
    }

    emlmr_state.state = EMLMR_STATE_ACTIVE;

out:
    spin_unlock_irqrestore(&emlmr_state.lock, flags);
    return ret;
}

void wifi7_emlmr_stop(struct wifi7_dev *dev)
{
    unsigned long flags;
    u8 link_id;

    spin_lock_irqsave(&emlmr_state.lock, flags);

    if (emlmr_state.state == EMLMR_STATE_ACTIVE) {
        emlmr_state.state = EMLMR_STATE_TEARDOWN;

        for_each_set_bit(link_id, (unsigned long *)&emlmr_state.active_link_mask,
                        WIFI7_MAX_LINKS) {
            wifi7_emlmr_teardown_link(dev, link_id);
        }

        emlmr_state.state = EMLMR_STATE_IDLE;
    }

    spin_unlock_irqrestore(&emlmr_state.lock, flags);
}

int wifi7_emlmr_switch_link(struct wifi7_dev *dev, u8 new_link)
{
    unsigned long flags;
    int ret = 0;

    spin_lock_irqsave(&emlmr_state.lock, flags);

    if (emlmr_state.state != EMLMR_STATE_ACTIVE) {
        ret = -EINVAL;
        goto out;
    }

    if (!(emlmr_state.active_link_mask & BIT(new_link))) {
        ret = -EINVAL;
        goto out;
    }

    emlmr_state.primary_link = new_link;
    wifi7_rf_switch_primary(dev, new_link);

out:
    spin_unlock_irqrestore(&emlmr_state.lock, flags);
    return ret;
}

int wifi7_emlmr_get_status(struct wifi7_dev *dev, 
                          struct wifi7_mlo_metrics *metrics)
{
    unsigned long flags;
    u8 link_id;

    spin_lock_irqsave(&emlmr_state.lock, flags);

    for_each_set_bit(link_id, (unsigned long *)&emlmr_state.active_link_mask,
                     WIFI7_MAX_LINKS) {
        wifi7_rf_get_link_metrics(dev, link_id, &metrics[link_id]);
    }

    spin_unlock_irqrestore(&emlmr_state.lock, flags);
    return 0;
}

EXPORT_SYMBOL(wifi7_emlmr_init);
EXPORT_SYMBOL(wifi7_emlmr_deinit);
EXPORT_SYMBOL(wifi7_emlmr_start);
EXPORT_SYMBOL(wifi7_emlmr_stop);
EXPORT_SYMBOL(wifi7_emlmr_switch_link);
EXPORT_SYMBOL(wifi7_emlmr_get_status);

void wifi7_mlo_link_state_change(struct wifi67_priv *priv, u8 link_id,
                               enum wifi67_mlo_link_state new_state)
{
    struct wifi67_mlo_link *link;
    unsigned long flags;

    spin_lock_irqsave(&priv->mlo_lock, flags);
    link = wifi67_mlo_get_link_by_id(priv, link_id);
    if (!link)
        goto out;

    switch (new_state) {
    case WIFI67_MLO_LINK_ACTIVE:
        wifi67_mlo_activate_link(link);
        break;
    case WIFI67_MLO_LINK_IDLE:
        wifi67_mlo_deactivate_link(link);
        break;
    case WIFI67_MLO_LINK_ERROR:
        wifi67_mlo_handle_link_error(link);
        break;
    default:
        break;
    }

out:
    spin_unlock_irqrestore(&priv->mlo_lock, flags);
}

int wifi7_mlo_setup_link_params(struct wifi67_priv *priv, u8 link_id,
                              struct ieee80211_link_data *link_data)
{
    struct wifi67_mlo_link *link;
    int ret = 0;

    link = wifi67_mlo_get_link_by_id(priv, link_id);
    if (!link)
        return -ENODEV;

    if (link->state != WIFI67_MLO_LINK_SETUP)
        return -EINVAL;

    link_data->hw_link_id = link_id;
    link_data->valid = true;

    return ret;
} 