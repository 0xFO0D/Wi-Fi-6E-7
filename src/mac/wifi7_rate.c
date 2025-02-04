/*
 * WiFi 7 Rate Control
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include <linux/random.h>
#include <linux/math64.h>
#include <linux/jiffies.h>
#include "wifi7_rate.h"
#include "wifi7_mac.h"

/* Rate table initialization */
static void wifi7_rate_init_table(struct wifi7_rate_table *table)
{
    int i, j, k, l, idx = 0;
    
    /* Initialize masks */
    memset(table->mcs_mask, 0xFF, sizeof(table->mcs_mask));
    memset(table->nss_mask, 0xFF, sizeof(table->nss_mask));
    memset(table->bw_mask, 0xFF, sizeof(table->bw_mask));
    memset(table->gi_mask, 0xFF, sizeof(table->gi_mask));
    table->flags_mask = 0xFFFFFFFF;
    
    /* Generate rate table */
    for (i = 0; i < WIFI7_RATE_MAX_MCS; i++) {
        for (j = 0; j < WIFI7_RATE_MAX_NSS; j++) {
            for (k = 0; k < WIFI7_RATE_MAX_BW; k++) {
                for (l = 0; l < WIFI7_RATE_MAX_GI; l++) {
                    struct wifi7_rate_info *rate = &table->rates[idx];
                    
                    rate->mcs = i;
                    rate->nss = j + 1;
                    rate->bw = k;
                    rate->gi = l;
                    rate->flags = WIFI7_RATE_FLAG_EHT;
                    
                    /* Calculate bitrate */
                    u32 base_rate = 6500; /* Base rate in 100 kbps */
                    u32 mcs_factor = 1 << i;
                    u32 nss_factor = j + 1;
                    u32 bw_factor = 1 << k;
                    
                    rate->bitrate = base_rate * mcs_factor * nss_factor *
                                  bw_factor;
                                  
                    /* Set duration based on guard interval */
                    switch (l) {
                    case 0: /* 0.8 us */
                        rate->duration = 800;
                        break;
                    case 1: /* 1.6 us */
                        rate->duration = 1600;
                        break;
                    case 2: /* 3.2 us */
                        rate->duration = 3200;
                        break;
                    }
                    
                    /* Set retry count based on MCS */
                    rate->retry_count = WIFI7_RATE_MAX_RETRIES - (i / 2);
                    
                    /* Set power level */
                    rate->power_level = 100 - (i * 5);
                    
                    idx++;
                }
            }
        }
    }
    
    table->n_rates = idx;
    table->max_rate = idx - 1;
    table->min_rate = 0;
    table->probe_rate = idx / 2;
    table->fallback_rate = 0;
}

/* Rate selection algorithms */
static u16 wifi7_rate_select_minstrel(struct wifi7_rate *rate,
                                    struct sk_buff *skb)
{
    struct wifi7_rate_table *table = &rate->tables[0];
    struct wifi7_rate_stats *stats;
    u16 best_rate = rate->current_rate;
    u32 best_tp = 0;
    int i;
    
    /* Find rate with best throughput */
    for (i = 0; i < table->n_rates; i++) {
        stats = &table->stats[i];
        
        if (stats->attempts == 0)
            continue;
            
        u32 tp = stats->success * table->rates[i].bitrate /
                 stats->attempts;
                 
        if (tp > best_tp) {
            best_tp = tp;
            best_rate = i;
        }
    }
    
    /* Occasionally probe other rates */
    if (prandom_u32() % 10 == 0) {
        u16 probe_rate = prandom_u32() % table->n_rates;
        if (probe_rate != best_rate) {
            rate->stats.probes++;
            return probe_rate;
        }
    }
    
    return best_rate;
}

static u16 wifi7_rate_select_pid(struct wifi7_rate *rate,
                               struct sk_buff *skb)
{
    struct wifi7_rate_table *table = &rate->tables[0];
    struct wifi7_rate_stats *stats = &table->stats[rate->current_rate];
    s32 error, delta;
    u16 new_rate;
    
    /* Calculate error */
    error = rate->config.success_threshold -
            (stats->success * 100 / stats->attempts);
            
    /* Apply PID control */
    delta = error / 2 + (error - stats->last_error) / 4;
    stats->last_error = error;
    
    /* Update rate */
    if (delta > 0 && rate->current_rate > table->min_rate)
        new_rate = rate->current_rate - 1;
    else if (delta < 0 && rate->current_rate < table->max_rate)
        new_rate = rate->current_rate + 1;
    else
        new_rate = rate->current_rate;
        
    return new_rate;
}

static u16 wifi7_rate_select_ml(struct wifi7_rate *rate,
                              struct sk_buff *skb)
{
    struct wifi7_rate_table *table = &rate->tables[0];
    struct wifi7_rate_stats *stats;
    u16 best_rate = rate->current_rate;
    u32 best_score = 0;
    int i;
    
    /* Calculate ML score for each rate */
    for (i = 0; i < table->n_rates; i++) {
        stats = &table->stats[i];
        
        if (stats->attempts < 10)
            continue;
            
        /* Features for ML model */
        u32 success_ratio = stats->success * 100 / stats->attempts;
        u32 retry_ratio = stats->retries * 100 / stats->attempts;
        u32 perfect_ratio = stats->perfect * 100 / stats->success;
        u32 throughput = stats->throughput;
        u32 airtime = stats->airtime;
        
        /* Simple linear model */
        u32 score = success_ratio * 2 +
                   (100 - retry_ratio) * 3 +
                   perfect_ratio * 2 +
                   throughput / 1000 +
                   (100 - airtime);
                   
        if (score > best_score) {
            best_score = score;
            best_rate = i;
        }
    }
    
    return best_rate;
}

/* Rate update work handler */
static void wifi7_rate_update_work(struct work_struct *work)
{
    struct wifi7_rate *rate = container_of(to_delayed_work(work),
                                         struct wifi7_rate, update_work);
    struct wifi7_rate_table *table = &rate->tables[0];
    struct wifi7_rate_stats *stats;
    unsigned long flags;
    u16 new_rate;
    
    spin_lock_irqsave(&rate->lock, flags);
    
    /* Select new rate */
    switch (rate->config.algorithm) {
    case WIFI7_RATE_ALGO_MINSTREL:
        new_rate = wifi7_rate_select_minstrel(rate, NULL);
        break;
    case WIFI7_RATE_ALGO_PID:
        new_rate = wifi7_rate_select_pid(rate, NULL);
        break;
    case WIFI7_RATE_ALGO_ML:
        new_rate = wifi7_rate_select_ml(rate, NULL);
        break;
    default:
        new_rate = rate->current_rate;
        break;
    }
    
    /* Update rate if changed */
    if (new_rate != rate->current_rate) {
        rate->last_rate = rate->current_rate;
        rate->current_rate = new_rate;
        rate->stats.rate_changes++;
    }
    
    /* Update statistics */
    stats = &table->stats[rate->current_rate];
    if (stats->throughput > rate->stats.max_tp)
        rate->stats.max_tp = stats->throughput;
    if (stats->throughput < rate->stats.min_tp)
        rate->stats.min_tp = stats->throughput;
    rate->stats.avg_tp = (rate->stats.avg_tp * 7 +
                         stats->throughput) / 8;
                         
    rate->update_count++;
    rate->last_update = ktime_get();
    
    spin_unlock_irqrestore(&rate->lock, flags);
    
    /* Schedule next update */
    if (rate->config.update_interval > 0)
        schedule_delayed_work(&rate->update_work,
                            msecs_to_jiffies(rate->config.update_interval));
}

/* Probe work handler */
static void wifi7_rate_probe_work(struct work_struct *work)
{
    struct wifi7_rate *rate = container_of(to_delayed_work(work),
                                         struct wifi7_rate, probe_work);
    struct wifi7_rate_table *table = &rate->tables[0];
    unsigned long flags;
    
    spin_lock_irqsave(&rate->lock, flags);
    
    /* Select probe rate */
    if (rate->config.algorithm == WIFI7_RATE_ALGO_MINSTREL) {
        u16 probe_rate = prandom_u32() % table->n_rates;
        if (probe_rate != rate->current_rate) {
            rate->last_rate = rate->current_rate;
            rate->current_rate = probe_rate;
            rate->stats.probes++;
        }
    }
    
    spin_unlock_irqrestore(&rate->lock, flags);
    
    /* Schedule next probe */
    if (rate->config.probe_interval > 0)
        schedule_delayed_work(&rate->probe_work,
                            msecs_to_jiffies(rate->config.probe_interval));
}

/* Public API Implementation */
int wifi7_rate_init(struct wifi7_dev *dev)
{
    struct wifi7_rate *rate;
    int ret;
    
    rate = kzalloc(sizeof(*rate), GFP_KERNEL);
    if (!rate)
        return -ENOMEM;
        
    /* Initialize lock */
    spin_lock_init(&rate->lock);
    
    /* Initialize rate tables */
    rate->tables = kzalloc(sizeof(struct wifi7_rate_table),
                          GFP_KERNEL);
    if (!rate->tables) {
        ret = -ENOMEM;
        goto err_free_rate;
    }
    
    wifi7_rate_init_table(&rate->tables[0]);
    rate->n_tables = 1;
    
    /* Initialize work items */
    INIT_DELAYED_WORK(&rate->update_work, wifi7_rate_update_work);
    INIT_DELAYED_WORK(&rate->probe_work, wifi7_rate_probe_work);
    
    /* Set default configuration */
    rate->config.algorithm = WIFI7_RATE_ALGO_MINSTREL;
    rate->config.max_retry = WIFI7_RATE_MAX_RETRIES;
    rate->config.update_interval = 100; /* 100ms */
    rate->config.probe_interval = 1000; /* 1s */
    rate->config.ewma_level = 75;
    rate->config.success_threshold = 85;
    rate->config.tx_power_max = 100;
    rate->config.tx_power_min = 0;
    rate->config.ampdu_enabled = true;
    rate->config.amsdu_enabled = true;
    rate->config.mu_enabled = true;
    rate->config.ofdma_enabled = true;
    rate->config.mlo_enabled = true;
    
    dev->rate = rate;
    return 0;
    
err_free_rate:
    kfree(rate);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_rate_init);

void wifi7_rate_deinit(struct wifi7_dev *dev)
{
    struct wifi7_rate *rate = dev->rate;
    
    if (!rate)
        return;
        
    /* Cancel work items */
    cancel_delayed_work_sync(&rate->update_work);
    cancel_delayed_work_sync(&rate->probe_work);
    
    kfree(rate->tables);
    kfree(rate);
    dev->rate = NULL;
}
EXPORT_SYMBOL_GPL(wifi7_rate_deinit);

int wifi7_rate_start(struct wifi7_dev *dev)
{
    struct wifi7_rate *rate = dev->rate;
    
    if (!rate)
        return -EINVAL;
        
    /* Start rate control */
    rate->current_rate = rate->tables[0].min_rate;
    rate->last_rate = rate->current_rate;
    rate->update_count = 0;
    rate->last_update = ktime_get();
    
    /* Schedule work */
    if (rate->config.update_interval > 0)
        schedule_delayed_work(&rate->update_work,
                            msecs_to_jiffies(rate->config.update_interval));
                            
    if (rate->config.probe_interval > 0)
        schedule_delayed_work(&rate->probe_work,
                            msecs_to_jiffies(rate->config.probe_interval));
                            
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_rate_start);

void wifi7_rate_stop(struct wifi7_dev *dev)
{
    struct wifi7_rate *rate = dev->rate;
    
    if (!rate)
        return;
        
    /* Stop rate control */
    cancel_delayed_work_sync(&rate->update_work);
    cancel_delayed_work_sync(&rate->probe_work);
}
EXPORT_SYMBOL_GPL(wifi7_rate_stop);

int wifi7_rate_update(struct wifi7_dev *dev,
                     struct sk_buff *skb,
                     bool success, u8 retries)
{
    struct wifi7_rate *rate = dev->rate;
    struct wifi7_rate_table *table;
    struct wifi7_rate_stats *stats;
    unsigned long flags;
    
    if (!rate)
        return -EINVAL;
        
    spin_lock_irqsave(&rate->lock, flags);
    
    table = &rate->tables[0];
    stats = &table->stats[rate->current_rate];
    
    /* Update statistics */
    stats->attempts++;
    if (success) {
        stats->success++;
        if (retries == 0)
            stats->perfect++;
        else
            stats->imperfect++;
    } else {
        stats->failures++;
    }
    stats->retries += retries;
    
    /* Update throughput */
    if (success) {
        u32 airtime = table->rates[rate->current_rate].duration *
                     (retries + 1);
        stats->airtime += airtime;
        stats->throughput = (stats->throughput * 7 +
                           table->rates[rate->current_rate].bitrate) / 8;
    }
    
    spin_unlock_irqrestore(&rate->lock, flags);
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_rate_update);

struct wifi7_rate_info *wifi7_rate_get_next(struct wifi7_dev *dev,
                                          struct sk_buff *skb)
{
    struct wifi7_rate *rate = dev->rate;
    struct wifi7_rate_table *table;
    struct wifi7_rate_info *info;
    unsigned long flags;
    
    if (!rate)
        return NULL;
        
    spin_lock_irqsave(&rate->lock, flags);
    
    table = &rate->tables[0];
    info = &table->rates[rate->current_rate];
    
    spin_unlock_irqrestore(&rate->lock, flags);
    return info;
}
EXPORT_SYMBOL_GPL(wifi7_rate_get_next);

/* Module initialization */
static int __init wifi7_rate_init_module(void)
{
    pr_info("WiFi 7 Rate Control initialized\n");
    return 0;
}

static void __exit wifi7_rate_exit_module(void)
{
    pr_info("WiFi 7 Rate Control unloaded\n");
}

module_init(wifi7_rate_init_module);
module_exit(wifi7_rate_exit_module);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Rate Control");
MODULE_VERSION("1.0"); 