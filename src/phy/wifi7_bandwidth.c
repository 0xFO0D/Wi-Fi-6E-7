/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ieee80211.h>
#include "wifi7_phy.h"
#include "wifi7_bandwidth.h"

/* Forward declarations */
static void wifi7_bw_monitor_work(struct work_struct *work);
static int wifi7_bw_check_conditions(struct wifi7_bw_context *bw,
                                   u16 target_width);
static int wifi7_bw_adapt_width(struct wifi7_bw_context *bw);
static void wifi7_bw_update_stats(struct wifi7_bw_context *bw,
                                 u16 old_width,
                                 u16 new_width);

/* Allocate bandwidth context */
struct wifi7_bw_context *wifi7_bw_alloc(struct wifi7_phy_dev *phy)
{
    struct wifi7_bw_context *bw;

    if (!phy)
        return NULL;

    bw = kzalloc(sizeof(*bw), GFP_KERNEL);
    if (!bw)
        return NULL;

    bw->phy = phy;
    bw->state = WIFI7_BW_STATE_INIT;
    spin_lock_init(&bw->bw_lock);

    /* Set default configuration */
    bw->config.primary_width = WIFI7_BW_20_MHZ;
    bw->config.punct_pattern = WIFI7_PUNCT_PATTERN_NONE;
    bw->config.dynamic_enabled = true;
    bw->config.params.min_duration = WIFI7_BW_MIN_ADAPT_INTERVAL_MS;
    bw->config.params.max_duration = WIFI7_BW_MAX_ADAPT_INTERVAL_MS;
    bw->config.params.up_threshold = 80;   /* 80% utilization */
    bw->config.params.down_threshold = 40;  /* 40% utilization */

    /* Create workqueue for monitoring */
    bw->bw_wq = alloc_workqueue("wifi7_bw_wq",
                               WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
    if (!bw->bw_wq)
        goto err_free_bw;

    INIT_DELAYED_WORK(&bw->monitor_work, wifi7_bw_monitor_work);

    return bw;

err_free_bw:
    kfree(bw);
    return NULL;
}
EXPORT_SYMBOL_GPL(wifi7_bw_alloc);

void wifi7_bw_free(struct wifi7_bw_context *bw)
{
    if (!bw)
        return;

    if (bw->bw_wq) {
        cancel_delayed_work_sync(&bw->monitor_work);
        destroy_workqueue(bw->bw_wq);
    }

    kfree(bw);
}
EXPORT_SYMBOL_GPL(wifi7_bw_free);

/* Configuration management */
int wifi7_bw_set_config(struct wifi7_bw_context *bw,
                       const struct wifi7_bw_config *config)
{
    unsigned long flags;
    int ret = 0;

    if (!bw || !config)
        return -EINVAL;

    /* Validate configuration */
    if (config->primary_width > WIFI7_BW_320_MHZ ||
        config->secondary_width > WIFI7_BW_320_MHZ)
        return -EINVAL;

    spin_lock_irqsave(&bw->bw_lock, flags);

    /* Update configuration */
    memcpy(&bw->config, config, sizeof(*config));

    /* Apply new configuration if active */
    if (bw->state != WIFI7_BW_STATE_INIT) {
        ret = wifi7_bw_adapt_width(bw);
        if (ret)
            goto out_unlock;
    }

out_unlock:
    spin_unlock_irqrestore(&bw->bw_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_bw_set_config);

/* Channel quality assessment */
static int wifi7_bw_check_conditions(struct wifi7_bw_context *bw,
                                   u16 target_width)
{
    int min_snr;

    /* Determine minimum SNR requirement */
    switch (target_width) {
    case WIFI7_BW_320_MHZ:
        min_snr = WIFI7_MIN_SNR_320MHZ;
        break;
    case WIFI7_BW_160_MHZ:
        min_snr = WIFI7_MIN_SNR_160MHZ;
        break;
    case WIFI7_BW_80_MHZ:
        min_snr = WIFI7_MIN_SNR_80MHZ;
        break;
    case WIFI7_BW_40_MHZ:
        min_snr = WIFI7_MIN_SNR_40MHZ;
        break;
    case WIFI7_BW_20_MHZ:
        min_snr = WIFI7_MIN_SNR_20MHZ;
        break;
    default:
        return -EINVAL;
    }

    /* Check signal quality */
    if (bw->quality.snr < min_snr)
        return -EAGAIN;

    /* Check interference level */
    if (bw->quality.interference_level > WIFI7_MAX_INTERFERENCE)
        return -EAGAIN;

    return 0;
}

static int wifi7_bw_adapt_width(struct wifi7_bw_context *bw)
{
    u16 old_width = bw->config.primary_width;
    u16 new_width = old_width;
    int ret = 0;

    /* Check if adaptation is needed */
    if (bw->quality.channel_utilization > bw->config.params.up_threshold) {
        /* Try to increase bandwidth */
        switch (old_width) {
        case WIFI7_BW_20_MHZ:
            new_width = WIFI7_BW_40_MHZ;
            break;
        case WIFI7_BW_40_MHZ:
            new_width = WIFI7_BW_80_MHZ;
            break;
        case WIFI7_BW_80_MHZ:
            new_width = WIFI7_BW_160_MHZ;
            break;
        case WIFI7_BW_160_MHZ:
            new_width = WIFI7_BW_320_MHZ;
            break;
        }
    } else if (bw->quality.channel_utilization < bw->config.params.down_threshold) {
        /* Try to decrease bandwidth */
        switch (old_width) {
        case WIFI7_BW_320_MHZ:
            new_width = WIFI7_BW_160_MHZ;
            break;
        case WIFI7_BW_160_MHZ:
            new_width = WIFI7_BW_80_MHZ;
            break;
        case WIFI7_BW_80_MHZ:
            new_width = WIFI7_BW_40_MHZ;
            break;
        case WIFI7_BW_40_MHZ:
            new_width = WIFI7_BW_20_MHZ;
            break;
        }
    }

    /* Check conditions for new width */
    if (new_width != old_width) {
        ret = wifi7_bw_check_conditions(bw, new_width);
        if (ret == 0) {
            bw->config.primary_width = new_width;
            wifi7_bw_update_stats(bw, old_width, new_width);
            
            /* Notify hardware */
            if (bw->phy->ops && bw->phy->ops->set_bandwidth)
                ret = bw->phy->ops->set_bandwidth(bw->phy, new_width);
        }
    }

    return ret;
}

/* Monitoring work */
static void wifi7_bw_monitor_work(struct work_struct *work)
{
    struct wifi7_bw_context *bw = container_of(work, struct wifi7_bw_context,
                                             monitor_work.work);
    unsigned long flags;
    bool reschedule = true;

    spin_lock_irqsave(&bw->bw_lock, flags);

    if (bw->state != WIFI7_BW_STATE_STABLE) {
        /* Update channel quality metrics */
        if (bw->phy->ops && bw->phy->ops->get_channel_quality) {
            struct wifi7_channel_quality quality;
            if (bw->phy->ops->get_channel_quality(bw->phy, &quality) == 0)
                memcpy(&bw->quality, &quality, sizeof(quality));
        }

        /* Perform bandwidth adaptation if needed */
        if (bw->config.dynamic_enabled)
            wifi7_bw_adapt_width(bw);

        /* Check for interference */
        if (bw->quality.interference_level > WIFI7_MAX_INTERFERENCE) {
            if (bw->state != WIFI7_BW_STATE_PUNCTURED) {
                /* Try to enable puncturing */
                if (wifi7_bw_set_puncturing(bw, WIFI7_PUNCT_PATTERN_LOW) == 0) {
                    bw->state = WIFI7_BW_STATE_PUNCTURED;
                    bw->stats.interference_events++;
                }
            }
        } else if (bw->quality.interference_level < WIFI7_CLEAR_INTERFERENCE) {
            if (bw->state == WIFI7_BW_STATE_PUNCTURED) {
                /* Try to disable puncturing */
                if (wifi7_bw_set_puncturing(bw, WIFI7_PUNCT_PATTERN_NONE) == 0) {
                    bw->state = WIFI7_BW_STATE_STABLE;
                    bw->stats.recovery_events++;
                }
            }
        }
    }

    spin_unlock_irqrestore(&bw->bw_lock, flags);

    /* Reschedule monitoring */
    if (reschedule) {
        queue_delayed_work(bw->bw_wq, &bw->monitor_work,
                          msecs_to_jiffies(WIFI7_BW_MEASURE_INTERVAL_MS));
    }
}

/* Statistics update */
static void wifi7_bw_update_stats(struct wifi7_bw_context *bw,
                                 u16 old_width,
                                 u16 new_width)
{
    if (new_width > old_width)
        bw->stats.width_increases++;
    else if (new_width < old_width)
        bw->stats.width_decreases++;

    /* Update throughput samples */
    switch (new_width) {
    case WIFI7_BW_320_MHZ:
        bw->stats.throughput_samples[4]++;
        break;
    case WIFI7_BW_160_MHZ:
        bw->stats.throughput_samples[3]++;
        break;
    case WIFI7_BW_80_MHZ:
        bw->stats.throughput_samples[2]++;
        break;
    case WIFI7_BW_40_MHZ:
        bw->stats.throughput_samples[1]++;
        break;
    case WIFI7_BW_20_MHZ:
        bw->stats.throughput_samples[0]++;
        break;
    }
}

/* Operation control */
int wifi7_bw_start(struct wifi7_bw_context *bw)
{
    unsigned long flags;
    int ret = 0;

    if (!bw)
        return -EINVAL;

    spin_lock_irqsave(&bw->bw_lock, flags);

    if (bw->state != WIFI7_BW_STATE_INIT) {
        ret = -EBUSY;
        goto out_unlock;
    }

    /* Initialize monitoring */
    bw->state = WIFI7_BW_STATE_MEASURING;
    queue_delayed_work(bw->bw_wq, &bw->monitor_work,
                      msecs_to_jiffies(WIFI7_BW_MEASURE_INTERVAL_MS));

out_unlock:
    spin_unlock_irqrestore(&bw->bw_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_bw_start);

void wifi7_bw_stop(struct wifi7_bw_context *bw)
{
    unsigned long flags;

    if (!bw)
        return;

    spin_lock_irqsave(&bw->bw_lock, flags);
    bw->state = WIFI7_BW_STATE_INIT;
    spin_unlock_irqrestore(&bw->bw_lock, flags);

    cancel_delayed_work_sync(&bw->monitor_work);
}
EXPORT_SYMBOL_GPL(wifi7_bw_stop);

/* Puncturing control */
int wifi7_bw_set_puncturing(struct wifi7_bw_context *bw,
                           u8 pattern)
{
    unsigned long flags;
    int ret = 0;

    if (!bw)
        return -EINVAL;

    spin_lock_irqsave(&bw->bw_lock, flags);

    if (bw->config.punct_pattern == pattern)
        goto out_unlock;

    /* Update puncturing pattern */
    bw->config.punct_pattern = pattern;
    bw->stats.punct_pattern_changes++;

    /* Notify hardware */
    if (bw->phy->ops && bw->phy->ops->set_puncturing)
        ret = bw->phy->ops->set_puncturing(bw->phy, pattern);

out_unlock:
    spin_unlock_irqrestore(&bw->bw_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_bw_set_puncturing);

/* MLO coordination */
int wifi7_bw_mlo_register_link(struct wifi7_bw_context *bw,
                              u8 link_id,
                              struct wifi7_bw_context *peer)
{
    unsigned long flags;
    int ret = 0;

    if (!bw || !peer || link_id >= 2)
        return -EINVAL;

    spin_lock_irqsave(&bw->bw_lock, flags);

    if (bw->peer_links[link_id]) {
        ret = -EBUSY;
        goto out_unlock;
    }

    bw->peer_links[link_id] = peer;
    bw->mlo_active = true;
    bw->link_id = link_id;

out_unlock:
    spin_unlock_irqrestore(&bw->bw_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_bw_mlo_register_link);

/* Debug interface */
void wifi7_bw_dump_stats(struct wifi7_bw_context *bw)
{
    if (!bw)
        return;

    pr_info("WiFi 7 Bandwidth Statistics:\n");
    pr_info("Width increases: %u\n", bw->stats.width_increases);
    pr_info("Width decreases: %u\n", bw->stats.width_decreases);
    pr_info("Puncturing changes: %u\n", bw->stats.punct_pattern_changes);
    pr_info("Interference events: %u\n", bw->stats.interference_events);
    pr_info("Recovery events: %u\n", bw->stats.recovery_events);

    pr_info("\nBandwidth distribution:\n");
    pr_info("20MHz: %u\n", bw->stats.throughput_samples[0]);
    pr_info("40MHz: %u\n", bw->stats.throughput_samples[1]);
    pr_info("80MHz: %u\n", bw->stats.throughput_samples[2]);
    pr_info("160MHz: %u\n", bw->stats.throughput_samples[3]);
    pr_info("320MHz: %u\n", bw->stats.throughput_samples[4]);
}
EXPORT_SYMBOL_GPL(wifi7_bw_dump_stats);

/* Module initialization */
static int __init wifi7_bw_init(void)
{
    pr_info("WiFi 7 Bandwidth Management initialized\n");
    return 0;
}

static void __exit wifi7_bw_exit(void)
{
    pr_info("WiFi 7 Bandwidth Management unloaded\n");
}

module_init(wifi7_bw_init);
module_exit(wifi7_bw_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Bandwidth Management");
MODULE_VERSION("1.0"); 