/*
 * WiFi 7 Rate Control and Adaptation
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
#include <linux/math64.h>
#include <linux/slab.h>
#include "wifi7_rate.h"
#include "wifi7_mac.h"
#include "../hal/wifi7_rf.h"
#include "wifi7_mlo.h"

/* Device state */
struct wifi7_rate_dev {
    struct wifi7_dev *dev;           /* Core device structure */
    struct wifi7_rate_config config; /* Rate configuration */
    struct wifi7_rate_stats stats;   /* Rate statistics */
    struct dentry *debugfs_dir;      /* debugfs directory */
    spinlock_t lock;                 /* Device lock */
    bool initialized;                /* Initialization flag */
    struct {
        struct wifi7_rate_table table;  /* Rate table */
        spinlock_t lock;                /* Table lock */
    } rate_table;
    struct {
        struct delayed_work update_work; /* Rate update work */
        struct delayed_work stats_work;  /* Stats collection work */
        struct completion update_done;   /* Update completion */
    } workers;
    struct {
        u32 *history;                   /* Rate history buffer */
        u32 history_size;               /* History size */
        u32 history_index;              /* Current index */
        spinlock_t lock;                /* History lock */
    } history;
    struct {
        void *model;                    /* ML model */
        u32 model_size;                 /* Model size */
        spinlock_t lock;                /* Model lock */
    } ml;
};

/* Global device context */
static struct wifi7_rate_dev *rate_dev;

/* Helper functions */
static inline bool is_valid_mcs(u8 mcs)
{
    return mcs <= WIFI7_RATE_MAX_MCS;
}

static inline bool is_valid_nss(u8 nss)
{
    return nss > 0 && nss <= WIFI7_RATE_MAX_NSS;
}

static inline bool is_valid_bw(u8 bw)
{
    return bw == 20 || bw == 40 || bw == 80 || bw == 160 || bw == 320;
}

static inline bool is_valid_gi(u8 gi)
{
    return gi <= WIFI7_RATE_MAX_GI;
}

/* Rate table management */
static void init_rate_table(struct wifi7_rate_table *table)
{
    int i;
    struct wifi7_rate_entry *entry;

    memset(table, 0, sizeof(*table));

    table->max_mcs = WIFI7_RATE_MAX_MCS;
    table->max_nss = WIFI7_RATE_MAX_NSS;
    table->max_bw = WIFI7_RATE_MAX_BW;
    table->max_gi = WIFI7_RATE_MAX_GI;
    table->capabilities = WIFI7_RATE_CAP_MCS_15 |
                         WIFI7_RATE_CAP_4K_QAM |
                         WIFI7_RATE_CAP_OFDMA |
                         WIFI7_RATE_CAP_MU_MIMO |
                         WIFI7_RATE_CAP_320MHZ |
                         WIFI7_RATE_CAP_16_SS |
                         WIFI7_RATE_CAP_EHT |
                         WIFI7_RATE_CAP_MLO |
                         WIFI7_RATE_CAP_DYNAMIC;
    table->eht_supported = true;
    table->mlo_supported = true;

    /* Initialize rate entries */
    for (i = 0; i <= WIFI7_RATE_MAX_MCS; i++) {
        entry = &table->entries[i];
        entry->mcs = i;
        entry->nss = 1;
        entry->bw = 80;  /* Default to 80MHz */
        entry->gi = 1;   /* Default to short GI */
        entry->dcm = 0;
        entry->flags = WIFI7_RATE_FLAG_EHT;
        entry->bitrate = wifi7_calculate_bitrate(i, 1, 80, 1);
        entry->tries = 0;
        entry->success = 0;
        entry->attempts = 0;
        entry->last_success = 0;
        entry->last_attempt = 0;
        entry->perfect_tx_time = wifi7_calculate_tx_time(i, 1, 80, 1);
        entry->max_tp_rate = 0;
        entry->valid = true;
    }
}

static void update_rate_stats(struct wifi7_rate_dev *dev,
                            struct wifi7_rate_entry *rate,
                            bool success)
{
    unsigned long flags;
    u32 now = jiffies_to_msecs(jiffies);

    spin_lock_irqsave(&dev->lock, flags);

    /* Update rate entry statistics */
    rate->attempts++;
    if (success) {
        rate->success++;
        rate->last_success = now;
    }
    rate->last_attempt = now;

    /* Update global statistics */
    dev->stats.tx_packets++;
    if (success) {
        dev->stats.tx_success++;
    } else {
        dev->stats.tx_failures++;
        dev->stats.tx_retries++;
    }

    spin_unlock_irqrestore(&dev->lock, flags);
}

/* Rate selection algorithms */
static struct wifi7_rate_entry *select_rate_minstrel(struct wifi7_rate_dev *dev,
                                                   struct sk_buff *skb)
{
    struct wifi7_rate_table *table = &dev->rate_table.table;
    struct wifi7_rate_entry *best_rate = NULL;
    unsigned long flags;
    int i;

    spin_lock_irqsave(&dev->rate_table.lock, flags);

    /* Find rate with best throughput */
    for (i = 0; i <= table->max_mcs; i++) {
        struct wifi7_rate_entry *rate = &table->entries[i];
        
        if (!rate->valid)
            continue;

        if (!best_rate || rate->max_tp_rate > best_rate->max_tp_rate)
            best_rate = rate;
    }

    spin_unlock_irqrestore(&dev->rate_table.lock, flags);
    return best_rate;
}

static struct wifi7_rate_entry *select_rate_pid(struct wifi7_rate_dev *dev,
                                              struct sk_buff *skb)
{
    struct wifi7_rate_table *table = &dev->rate_table.table;
    struct wifi7_rate_entry *current_rate;
    unsigned long flags;
    int error, delta;

    spin_lock_irqsave(&dev->rate_table.lock, flags);

    /* Get current rate */
    current_rate = &table->entries[table->max_mcs / 2];

    /* Calculate PID error */
    error = current_rate->success * 100 / current_rate->attempts - 75;
    delta = error / 10;

    /* Adjust MCS based on error */
    if (delta > 0 && current_rate->mcs < table->max_mcs) {
        current_rate = &table->entries[current_rate->mcs + 1];
    } else if (delta < 0 && current_rate->mcs > 0) {
        current_rate = &table->entries[current_rate->mcs - 1];
    }

    spin_unlock_irqrestore(&dev->rate_table.lock, flags);
    return current_rate;
}

static struct wifi7_rate_entry *select_rate_ml(struct wifi7_rate_dev *dev,
                                             struct sk_buff *skb)
{
    /* TODO: Implement ML-based rate selection */
    return select_rate_minstrel(dev, skb);
}

/* Work handlers */
static void rate_update_work_handler(struct work_struct *work)
{
    struct wifi7_rate_dev *dev = rate_dev;
    struct wifi7_rate_table *table = &dev->rate_table.table;
    unsigned long flags;
    int i;

    if (!dev->initialized)
        return;

    spin_lock_irqsave(&dev->rate_table.lock, flags);

    /* Update rate statistics */
    for (i = 0; i <= table->max_mcs; i++) {
        struct wifi7_rate_entry *rate = &table->entries[i];
        u32 success_ratio;

        if (!rate->valid)
            continue;

        if (rate->attempts == 0)
            continue;

        /* Calculate success ratio */
        success_ratio = rate->success * 100 / rate->attempts;

        /* Update EWMA probability */
        dev->stats.prob_ewma = (dev->stats.prob_ewma * 75 + success_ratio * 25) / 100;

        /* Update throughput */
        dev->stats.cur_tp = rate->bitrate * success_ratio / 100;
        if (dev->stats.cur_tp > dev->stats.max_tp)
            dev->stats.max_tp = dev->stats.cur_tp;
    }

    dev->stats.last_update = ktime_get();

    spin_unlock_irqrestore(&dev->rate_table.lock, flags);

    /* Schedule next update */
    if (dev->config.auto_adjust)
        schedule_delayed_work(&dev->workers.update_work,
                            msecs_to_jiffies(dev->config.update_interval));
}

static void rate_stats_work_handler(struct work_struct *work)
{
    struct wifi7_rate_dev *dev = rate_dev;
    unsigned long flags;

    if (!dev->initialized)
        return;

    spin_lock_irqsave(&dev->lock, flags);

    /* Update perfect/average TX times */
    dev->stats.perfect_tx_time = 0;  /* TODO: Calculate from rate table */
    dev->stats.avg_tx_time = 0;      /* TODO: Calculate from rate table */

    /* Update sampling statistics */
    if (dev->config.sample_mode) {
        dev->stats.sample_count++;
        /* TODO: Update sampling statistics */
    }

    spin_unlock_irqrestore(&dev->lock, flags);

    /* Schedule next collection */
    schedule_delayed_work(&dev->workers.stats_work, HZ);
}

/* Module initialization */
int wifi7_rate_init(struct wifi7_dev *dev)
{
    struct wifi7_rate_dev *rdev;
    int ret;

    /* Allocate device context */
    rdev = kzalloc(sizeof(*rdev), GFP_KERNEL);
    if (!rdev)
        return -ENOMEM;

    rdev->dev = dev;
    spin_lock_init(&rdev->lock);
    spin_lock_init(&rdev->rate_table.lock);
    spin_lock_init(&rdev->history.lock);
    spin_lock_init(&rdev->ml.lock);
    rate_dev = rdev;

    /* Initialize work queues */
    INIT_DELAYED_WORK(&rdev->workers.update_work, rate_update_work_handler);
    INIT_DELAYED_WORK(&rdev->workers.stats_work, rate_stats_work_handler);
    init_completion(&rdev->workers.update_done);

    /* Initialize rate table */
    init_rate_table(&rdev->rate_table.table);

    /* Allocate history buffer */
    rdev->history.history = kzalloc(sizeof(u32) * 1024, GFP_KERNEL);
    if (!rdev->history.history) {
        ret = -ENOMEM;
        goto err_free;
    }
    rdev->history.history_size = 1024;

    /* Set default configuration */
    rdev->config.algorithm = WIFI7_RATE_ALGO_MINSTREL;
    rdev->config.capabilities = WIFI7_RATE_CAP_MCS_15 |
                               WIFI7_RATE_CAP_4K_QAM |
                               WIFI7_RATE_CAP_OFDMA |
                               WIFI7_RATE_CAP_MU_MIMO |
                               WIFI7_RATE_CAP_320MHZ |
                               WIFI7_RATE_CAP_16_SS |
                               WIFI7_RATE_CAP_EHT |
                               WIFI7_RATE_CAP_MLO |
                               WIFI7_RATE_CAP_DYNAMIC;
    rdev->config.max_retry = WIFI7_RATE_MAX_RETRY;
    rdev->config.update_interval = 100;
    rdev->config.auto_adjust = true;
    rdev->config.sample_mode = true;
    rdev->config.ml_enabled = false;
    rdev->config.limits.min_mcs = 0;
    rdev->config.limits.max_mcs = WIFI7_RATE_MAX_MCS;
    rdev->config.limits.min_nss = 1;
    rdev->config.limits.max_nss = WIFI7_RATE_MAX_NSS;
    rdev->config.limits.min_bw = 20;
    rdev->config.limits.max_bw = WIFI7_RATE_MAX_BW;

    rdev->initialized = true;
    dev_info(dev->dev, "Rate control initialized\n");

    return 0;

err_free:
    kfree(rdev);
    return ret;
}
EXPORT_SYMBOL(wifi7_rate_init);

void wifi7_rate_deinit(struct wifi7_dev *dev)
{
    struct wifi7_rate_dev *rdev = rate_dev;

    if (!rdev)
        return;

    rdev->initialized = false;

    /* Cancel workers */
    cancel_delayed_work_sync(&rdev->workers.update_work);
    cancel_delayed_work_sync(&rdev->workers.stats_work);

    kfree(rdev->history.history);
    kfree(rdev);
    rate_dev = NULL;

    dev_info(dev->dev, "Rate control deinitialized\n");
}
EXPORT_SYMBOL(wifi7_rate_deinit);

/* Module interface */
int wifi7_rate_start(struct wifi7_dev *dev)
{
    struct wifi7_rate_dev *rdev = rate_dev;

    if (!rdev || !rdev->initialized)
        return -EINVAL;

    /* Start workers */
    schedule_delayed_work(&rdev->workers.update_work,
                         msecs_to_jiffies(rdev->config.update_interval));
    schedule_delayed_work(&rdev->workers.stats_work, HZ);

    dev_info(dev->dev, "Rate control started\n");
    return 0;
}
EXPORT_SYMBOL(wifi7_rate_start);

void wifi7_rate_stop(struct wifi7_dev *dev)
{
    struct wifi7_rate_dev *rdev = rate_dev;

    if (!rdev || !rdev->initialized)
        return;

    /* Cancel workers */
    cancel_delayed_work_sync(&rdev->workers.update_work);
    cancel_delayed_work_sync(&rdev->workers.stats_work);

    dev_info(dev->dev, "Rate control stopped\n");
}
EXPORT_SYMBOL(wifi7_rate_stop);

/* Configuration interface */
int wifi7_rate_set_config(struct wifi7_dev *dev,
                         struct wifi7_rate_config *config)
{
    struct wifi7_rate_dev *rdev = rate_dev;
    unsigned long flags;

    if (!rdev || !rdev->initialized || !config)
        return -EINVAL;

    spin_lock_irqsave(&rdev->lock, flags);
    memcpy(&rdev->config, config, sizeof(*config));
    spin_unlock_irqrestore(&rdev->lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_rate_set_config);

int wifi7_rate_get_config(struct wifi7_dev *dev,
                         struct wifi7_rate_config *config)
{
    struct wifi7_rate_dev *rdev = rate_dev;
    unsigned long flags;

    if (!rdev || !rdev->initialized || !config)
        return -EINVAL;

    spin_lock_irqsave(&rdev->lock, flags);
    memcpy(config, &rdev->config, sizeof(*config));
    spin_unlock_irqrestore(&rdev->lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_rate_get_config);

/* Rate table interface */
int wifi7_rate_get_table(struct wifi7_dev *dev,
                        struct wifi7_rate_table *table)
{
    struct wifi7_rate_dev *rdev = rate_dev;
    unsigned long flags;

    if (!rdev || !rdev->initialized || !table)
        return -EINVAL;

    spin_lock_irqsave(&rdev->rate_table.lock, flags);
    memcpy(table, &rdev->rate_table.table, sizeof(*table));
    spin_unlock_irqrestore(&rdev->rate_table.lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_rate_get_table);

int wifi7_rate_update_table(struct wifi7_dev *dev,
                           struct wifi7_rate_table *table)
{
    struct wifi7_rate_dev *rdev = rate_dev;
    unsigned long flags;

    if (!rdev || !rdev->initialized || !table)
        return -EINVAL;

    spin_lock_irqsave(&rdev->rate_table.lock, flags);
    memcpy(&rdev->rate_table.table, table, sizeof(*table));
    spin_unlock_irqrestore(&rdev->rate_table.lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_rate_update_table);

/* Statistics interface */
int wifi7_rate_get_stats(struct wifi7_dev *dev,
                        struct wifi7_rate_stats *stats)
{
    struct wifi7_rate_dev *rdev = rate_dev;
    unsigned long flags;

    if (!rdev || !rdev->initialized || !stats)
        return -EINVAL;

    spin_lock_irqsave(&rdev->lock, flags);
    memcpy(stats, &rdev->stats, sizeof(*stats));
    spin_unlock_irqrestore(&rdev->lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_rate_get_stats);

int wifi7_rate_clear_stats(struct wifi7_dev *dev)
{
    struct wifi7_rate_dev *rdev = rate_dev;
    unsigned long flags;

    if (!rdev || !rdev->initialized)
        return -EINVAL;

    spin_lock_irqsave(&rdev->lock, flags);
    memset(&rdev->stats, 0, sizeof(rdev->stats));
    spin_unlock_irqrestore(&rdev->lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_rate_clear_stats);

/* Rate selection interface */
int wifi7_rate_get_max_rate(struct wifi7_dev *dev,
                           struct wifi7_rate_entry *rate)
{
    struct wifi7_rate_dev *rdev = rate_dev;
    struct wifi7_rate_table *table = &rdev->rate_table.table;
    unsigned long flags;

    if (!rdev || !rdev->initialized || !rate)
        return -EINVAL;

    spin_lock_irqsave(&rdev->rate_table.lock, flags);
    memcpy(rate, &table->entries[table->max_mcs], sizeof(*rate));
    spin_unlock_irqrestore(&rdev->rate_table.lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_rate_get_max_rate);

int wifi7_rate_get_min_rate(struct wifi7_dev *dev,
                           struct wifi7_rate_entry *rate)
{
    struct wifi7_rate_dev *rdev = rate_dev;
    struct wifi7_rate_table *table = &rdev->rate_table.table;
    unsigned long flags;

    if (!rdev || !rdev->initialized || !rate)
        return -EINVAL;

    spin_lock_irqsave(&rdev->rate_table.lock, flags);
    memcpy(rate, &table->entries[0], sizeof(*rate));
    spin_unlock_irqrestore(&rdev->rate_table.lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_rate_get_min_rate);

int wifi7_rate_select(struct wifi7_dev *dev,
                     struct sk_buff *skb,
                     struct wifi7_rate_entry *rate)
{
    struct wifi7_rate_dev *rdev = rate_dev;
    struct wifi7_rate_entry *selected_rate = NULL;

    if (!rdev || !rdev->initialized || !skb || !rate)
        return -EINVAL;

    /* Select rate based on algorithm */
    switch (rdev->config.algorithm) {
    case WIFI7_RATE_ALGO_MINSTREL:
        selected_rate = select_rate_minstrel(rdev, skb);
        break;
    case WIFI7_RATE_ALGO_PID:
        selected_rate = select_rate_pid(rdev, skb);
        break;
    case WIFI7_RATE_ALGO_ML:
        selected_rate = select_rate_ml(rdev, skb);
        break;
    default:
        selected_rate = select_rate_minstrel(rdev, skb);
        break;
    }

    if (!selected_rate)
        return -EINVAL;

    memcpy(rate, selected_rate, sizeof(*rate));
    return 0;
}
EXPORT_SYMBOL(wifi7_rate_select);

int wifi7_rate_update(struct wifi7_dev *dev,
                     struct sk_buff *skb,
                     struct wifi7_rate_entry *rate,
                     bool success)
{
    struct wifi7_rate_dev *rdev = rate_dev;

    if (!rdev || !rdev->initialized || !skb || !rate)
        return -EINVAL;

    update_rate_stats(rdev, rate, success);
    return 0;
}
EXPORT_SYMBOL(wifi7_rate_update);

/* Module parameters */
static bool rate_auto_adjust = true;
module_param(rate_auto_adjust, bool, 0644);
MODULE_PARM_DESC(rate_auto_adjust, "Enable automatic rate adjustment");

static bool rate_sample_mode = true;
module_param(rate_sample_mode, bool, 0644);
MODULE_PARM_DESC(rate_sample_mode, "Enable rate sampling");

static bool rate_ml_enable = false;
module_param(rate_ml_enable, bool, 0644);
MODULE_PARM_DESC(rate_ml_enable, "Enable ML-based rate selection");

/* Module initialization */
static int __init wifi7_rate_init_module(void)
{
    pr_info("WiFi 7 rate control loaded\n");
    return 0;
}

static void __exit wifi7_rate_exit_module(void)
{
    pr_info("WiFi 7 rate control unloaded\n");
}

module_init(wifi7_rate_init_module);
module_exit(wifi7_rate_exit_module);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Rate Control and Adaptation");
MODULE_VERSION("1.0"); 