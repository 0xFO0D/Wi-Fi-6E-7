/*
 * WiFi 7 Spatial Reuse
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
#include "wifi7_spatial.h"
#include "wifi7_mac.h"

/* Helper functions */
static inline bool is_bss_color_valid(u8 color)
{
    return color < WIFI7_SR_MAX_BSS_COLOR;
}

static inline bool is_obss_pd_valid(u8 pd_level)
{
    return pd_level <= WIFI7_SR_MAX_OBSS_PD;
}

static inline bool is_power_valid(s8 power)
{
    return power >= -WIFI7_SR_MAX_TX_POWER &&
           power <= WIFI7_SR_MAX_TX_POWER;
}

/* BSS color management */
static void wifi7_sr_color_work(struct work_struct *work)
{
    struct wifi7_sr *sr = container_of(to_delayed_work(work),
                                     struct wifi7_sr, color_work);
    unsigned long flags;
    bool collision = false;
    
    spin_lock_irqsave(&sr->color_lock, flags);
    
    if (!(sr->flags & WIFI7_SR_FLAG_BSS_COLOR))
        goto out_unlock;
        
    /* Check for collisions */
    if (sr->bss_color.collision_count > 0) {
        ktime_t now = ktime_get();
        s64 delta = ktime_ms_delta(now, sr->bss_color.last_collision);
        
        if (delta < 1000) { /* Within 1 second */
            collision = true;
            sr->stats.color_collisions++;
        }
    }
    
    /* Handle collision */
    if (collision) {
        u8 new_color;
        
        /* Generate new color */
        do {
            get_random_bytes(&new_color, sizeof(new_color));
            new_color %= WIFI7_SR_MAX_BSS_COLOR;
        } while (new_color == sr->bss_color.color);
        
        sr->bss_color.color = new_color;
        sr->bss_color.collision_count = 0;
        sr->stats.color_changes++;
    }
    
out_unlock:
    spin_unlock_irqrestore(&sr->color_lock, flags);
    
    /* Schedule next check */
    schedule_delayed_work(&sr->color_work, HZ);
}

/* SRG management */
static void wifi7_sr_srg_work(struct work_struct *work)
{
    struct wifi7_sr *sr = container_of(to_delayed_work(work),
                                     struct wifi7_sr, srg_work);
    unsigned long flags;
    
    spin_lock_irqsave(&sr->srg_lock, flags);
    
    if (!(sr->flags & WIFI7_SR_FLAG_SRG))
        goto out_unlock;
        
    /* Update SRG parameters based on conditions */
    if (sr->stats.srg_opportunities > 0) {
        u32 success_rate = sr->stats.srg_successes * 100 /
                          sr->stats.srg_opportunities;
                          
        /* Adjust OBSS PD thresholds */
        if (success_rate < 50 && sr->srg.obss_pd_min > 0)
            sr->srg.obss_pd_min--;
        else if (success_rate > 90 &&
                 sr->srg.obss_pd_min < WIFI7_SR_MAX_OBSS_PD)
            sr->srg.obss_pd_min++;
    }
    
out_unlock:
    spin_unlock_irqrestore(&sr->srg_lock, flags);
    
    /* Schedule next update */
    schedule_delayed_work(&sr->srg_work, HZ/2);
}

/* PSR management */
static void wifi7_sr_psr_work(struct work_struct *work)
{
    struct wifi7_sr *sr = container_of(to_delayed_work(work),
                                     struct wifi7_sr, psr_work);
    unsigned long flags;
    
    spin_lock_irqsave(&sr->psr_lock, flags);
    
    if (!(sr->flags & WIFI7_SR_FLAG_PSR))
        goto out_unlock;
        
    /* Update PSR parameters based on performance */
    if (sr->stats.psr_opportunities > 0) {
        u32 success_rate = sr->stats.psr_successes * 100 /
                          sr->stats.psr_opportunities;
                          
        /* Adjust threshold and margin */
        if (success_rate < 60) {
            if (sr->psr.threshold > 0)
                sr->psr.threshold--;
            if (sr->psr.margin < WIFI7_SR_MAX_PSR_THRESH)
                sr->psr.margin++;
        } else if (success_rate > 90) {
            if (sr->psr.threshold < WIFI7_SR_MAX_PSR_THRESH)
                sr->psr.threshold++;
            if (sr->psr.margin > 0)
                sr->psr.margin--;
        }
    }
    
out_unlock:
    spin_unlock_irqrestore(&sr->psr_lock, flags);
    
    /* Schedule next update */
    schedule_delayed_work(&sr->psr_work, HZ/2);
}

/* Power control management */
static void wifi7_sr_power_work(struct work_struct *work)
{
    struct wifi7_sr *sr = container_of(to_delayed_work(work),
                                     struct wifi7_sr, power_work);
    unsigned long flags;
    
    spin_lock_irqsave(&sr->power_lock, flags);
    
    if (!(sr->flags & WIFI7_SR_FLAG_POWER))
        goto out_unlock;
        
    /* Check power boost timeout */
    if (sr->power.power_boost) {
        if (sr->power.boost_timeout > 0) {
            sr->power.boost_timeout--;
            if (sr->power.boost_timeout == 0) {
                sr->power.power_boost = false;
                sr->power.tx_power = sr->power.rx_power;
                sr->stats.power_adjustments++;
            }
        }
    }
    
    /* Adjust PD threshold based on interference */
    if (sr->interference.level > 0) {
        if (sr->power.pd_threshold < WIFI7_SR_MAX_OBSS_PD) {
            sr->power.pd_threshold++;
            sr->stats.pd_adjustments++;
        }
    } else if (sr->power.pd_threshold > 0) {
        sr->power.pd_threshold--;
        sr->stats.pd_adjustments++;
    }
    
out_unlock:
    spin_unlock_irqrestore(&sr->power_lock, flags);
    
    /* Schedule next update */
    schedule_delayed_work(&sr->power_work, HZ/10);
}

/* Public API Implementation */
int wifi7_sr_init(struct wifi7_dev *dev)
{
    struct wifi7_sr *sr;
    int ret;
    
    sr = kzalloc(sizeof(*sr), GFP_KERNEL);
    if (!sr)
        return -ENOMEM;
        
    /* Set capabilities */
    sr->capabilities = WIFI7_SR_CAP_BSS_COLOR |
                      WIFI7_SR_CAP_NON_SRG_OBSS |
                      WIFI7_SR_CAP_SRG_OBSS |
                      WIFI7_SR_CAP_PSR |
                      WIFI7_SR_CAP_HESIGA |
                      WIFI7_SR_CAP_EHTVECTOR |
                      WIFI7_SR_CAP_DYNAMIC |
                      WIFI7_SR_CAP_ADAPTIVE |
                      WIFI7_SR_CAP_MULTI_BSS |
                      WIFI7_SR_CAP_SPATIAL_REUSE |
                      WIFI7_SR_CAP_POWER_CONTROL |
                      WIFI7_SR_CAP_INTERFERENCE;
                      
    /* Initialize locks */
    spin_lock_init(&sr->color_lock);
    spin_lock_init(&sr->srg_lock);
    spin_lock_init(&sr->psr_lock);
    spin_lock_init(&sr->power_lock);
    spin_lock_init(&sr->interference_lock);
    
    /* Create workqueue */
    sr->wq = create_singlethread_workqueue("wifi7_sr");
    if (!sr->wq) {
        ret = -ENOMEM;
        goto err_free_sr;
    }
    
    /* Initialize work items */
    INIT_DELAYED_WORK(&sr->color_work, wifi7_sr_color_work);
    INIT_DELAYED_WORK(&sr->srg_work, wifi7_sr_srg_work);
    INIT_DELAYED_WORK(&sr->psr_work, wifi7_sr_psr_work);
    INIT_DELAYED_WORK(&sr->power_work, wifi7_sr_power_work);
    
    /* Set default BSS color */
    get_random_bytes(&sr->bss_color.color, sizeof(sr->bss_color.color));
    sr->bss_color.color %= WIFI7_SR_MAX_BSS_COLOR;
    
    /* Set default SRG parameters */
    sr->srg.obss_pd_min = 10;
    sr->srg.obss_pd_max = WIFI7_SR_MAX_OBSS_PD;
    
    /* Set default PSR parameters */
    sr->psr.threshold = WIFI7_SR_MAX_PSR_THRESH / 2;
    sr->psr.margin = 2;
    sr->psr.psr_reset_timeout = 100;
    
    /* Set default power parameters */
    sr->power.tx_power = 20;
    sr->power.rx_power = 20;
    sr->power.pd_threshold = 10;
    sr->power.margin = 3;
    
    dev->sr = sr;
    return 0;
    
err_free_sr:
    kfree(sr);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_sr_init);

void wifi7_sr_deinit(struct wifi7_dev *dev)
{
    struct wifi7_sr *sr = dev->sr;
    
    if (!sr)
        return;
        
    /* Cancel work items */
    cancel_delayed_work_sync(&sr->color_work);
    cancel_delayed_work_sync(&sr->srg_work);
    cancel_delayed_work_sync(&sr->psr_work);
    cancel_delayed_work_sync(&sr->power_work);
    
    /* Destroy workqueue */
    destroy_workqueue(sr->wq);
    
    kfree(sr);
    dev->sr = NULL;
}
EXPORT_SYMBOL_GPL(wifi7_sr_deinit);

int wifi7_sr_start(struct wifi7_dev *dev)
{
    struct wifi7_sr *sr = dev->sr;
    
    if (!sr)
        return -EINVAL;
        
    /* Enable features */
    sr->flags |= WIFI7_SR_FLAG_BSS_COLOR |
                 WIFI7_SR_FLAG_NON_SRG |
                 WIFI7_SR_FLAG_SRG |
                 WIFI7_SR_FLAG_PSR |
                 WIFI7_SR_FLAG_POWER;
                 
    /* Schedule work */
    schedule_delayed_work(&sr->color_work, 0);
    schedule_delayed_work(&sr->srg_work, 0);
    schedule_delayed_work(&sr->psr_work, 0);
    schedule_delayed_work(&sr->power_work, 0);
    
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_sr_start);

void wifi7_sr_stop(struct wifi7_dev *dev)
{
    struct wifi7_sr *sr = dev->sr;
    
    if (!sr)
        return;
        
    /* Disable features */
    sr->flags = 0;
    
    /* Cancel work */
    cancel_delayed_work_sync(&sr->color_work);
    cancel_delayed_work_sync(&sr->srg_work);
    cancel_delayed_work_sync(&sr->psr_work);
    cancel_delayed_work_sync(&sr->power_work);
}
EXPORT_SYMBOL_GPL(wifi7_sr_stop);

int wifi7_sr_set_bss_color(struct wifi7_dev *dev,
                          struct wifi7_sr_bss_color *color)
{
    struct wifi7_sr *sr = dev->sr;
    unsigned long flags;
    int ret = 0;
    
    if (!sr || !color)
        return -EINVAL;
        
    if (!is_bss_color_valid(color->color))
        return -EINVAL;
        
    spin_lock_irqsave(&sr->color_lock, flags);
    memcpy(&sr->bss_color, color, sizeof(*color));
    spin_unlock_irqrestore(&sr->color_lock, flags);
    
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_sr_set_bss_color);

int wifi7_sr_get_bss_color(struct wifi7_dev *dev,
                          struct wifi7_sr_bss_color *color)
{
    struct wifi7_sr *sr = dev->sr;
    unsigned long flags;
    
    if (!sr || !color)
        return -EINVAL;
        
    spin_lock_irqsave(&sr->color_lock, flags);
    memcpy(color, &sr->bss_color, sizeof(*color));
    spin_unlock_irqrestore(&sr->color_lock, flags);
    
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_sr_get_bss_color);

int wifi7_sr_set_srg(struct wifi7_dev *dev,
                     struct wifi7_sr_srg *srg)
{
    struct wifi7_sr *sr = dev->sr;
    unsigned long flags;
    int ret = 0;
    
    if (!sr || !srg)
        return -EINVAL;
        
    if (!is_obss_pd_valid(srg->obss_pd_min) ||
        !is_obss_pd_valid(srg->obss_pd_max))
        return -EINVAL;
        
    spin_lock_irqsave(&sr->srg_lock, flags);
    memcpy(&sr->srg, srg, sizeof(*srg));
    spin_unlock_irqrestore(&sr->srg_lock, flags);
    
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_sr_set_srg);

int wifi7_sr_get_srg(struct wifi7_dev *dev,
                     struct wifi7_sr_srg *srg)
{
    struct wifi7_sr *sr = dev->sr;
    unsigned long flags;
    
    if (!sr || !srg)
        return -EINVAL;
        
    spin_lock_irqsave(&sr->srg_lock, flags);
    memcpy(srg, &sr->srg, sizeof(*srg));
    spin_unlock_irqrestore(&sr->srg_lock, flags);
    
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_sr_get_srg);

int wifi7_sr_set_psr(struct wifi7_dev *dev,
                     struct wifi7_sr_psr *psr)
{
    struct wifi7_sr *sr = dev->sr;
    unsigned long flags;
    int ret = 0;
    
    if (!sr || !psr)
        return -EINVAL;
        
    spin_lock_irqsave(&sr->psr_lock, flags);
    memcpy(&sr->psr, psr, sizeof(*psr));
    spin_unlock_irqrestore(&sr->psr_lock, flags);
    
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_sr_set_psr);

int wifi7_sr_get_psr(struct wifi7_dev *dev,
                     struct wifi7_sr_psr *psr)
{
    struct wifi7_sr *sr = dev->sr;
    unsigned long flags;
    
    if (!sr || !psr)
        return -EINVAL;
        
    spin_lock_irqsave(&sr->psr_lock, flags);
    memcpy(psr, &sr->psr, sizeof(*psr));
    spin_unlock_irqrestore(&sr->psr_lock, flags);
    
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_sr_get_psr);

int wifi7_sr_set_power(struct wifi7_dev *dev,
                      struct wifi7_sr_power *power)
{
    struct wifi7_sr *sr = dev->sr;
    unsigned long flags;
    int ret = 0;
    
    if (!sr || !power)
        return -EINVAL;
        
    if (!is_power_valid(power->tx_power) ||
        !is_power_valid(power->rx_power))
        return -EINVAL;
        
    spin_lock_irqsave(&sr->power_lock, flags);
    memcpy(&sr->power, power, sizeof(*power));
    spin_unlock_irqrestore(&sr->power_lock, flags);
    
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_sr_set_power);

int wifi7_sr_get_power(struct wifi7_dev *dev,
                      struct wifi7_sr_power *power)
{
    struct wifi7_sr *sr = dev->sr;
    unsigned long flags;
    
    if (!sr || !power)
        return -EINVAL;
        
    spin_lock_irqsave(&sr->power_lock, flags);
    memcpy(power, &sr->power, sizeof(*power));
    spin_unlock_irqrestore(&sr->power_lock, flags);
    
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_sr_get_power);

int wifi7_sr_report_interference(struct wifi7_dev *dev,
                               struct wifi7_sr_interference *interference)
{
    struct wifi7_sr *sr = dev->sr;
    unsigned long flags;
    
    if (!sr || !interference)
        return -EINVAL;
        
    spin_lock_irqsave(&sr->interference_lock, flags);
    
    /* Update interference info */
    memcpy(&sr->interference, interference, sizeof(*interference));
    sr->stats.interference_events++;
    sr->stats.interference_duration += interference->duration;
    
    /* Trigger mitigation if needed */
    if (interference->level >= WIFI7_SR_MAX_INTERFERENCE / 2) {
        sr->power.power_boost = true;
        sr->power.boost_timeout = 100;
        sr->stats.interference_mitigations++;
    }
    
    spin_unlock_irqrestore(&sr->interference_lock, flags);
    
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_sr_report_interference);

int wifi7_sr_get_stats(struct wifi7_dev *dev,
                      struct wifi7_sr_stats *stats)
{
    struct wifi7_sr *sr = dev->sr;
    
    if (!sr || !stats)
        return -EINVAL;
        
    memcpy(stats, &sr->stats, sizeof(*stats));
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_sr_get_stats);

int wifi7_sr_clear_stats(struct wifi7_dev *dev)
{
    struct wifi7_sr *sr = dev->sr;
    
    if (!sr)
        return -EINVAL;
        
    memset(&sr->stats, 0, sizeof(sr->stats));
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_sr_clear_stats);

/* Module initialization */
static int __init wifi7_sr_init_module(void)
{
    pr_info("WiFi 7 Spatial Reuse initialized\n");
    return 0;
}

static void __exit wifi7_sr_exit_module(void)
{
    pr_info("WiFi 7 Spatial Reuse unloaded\n");
}

module_init(wifi7_sr_init_module);
module_exit(wifi7_sr_exit_module);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Spatial Reuse");
MODULE_VERSION("1.0"); 