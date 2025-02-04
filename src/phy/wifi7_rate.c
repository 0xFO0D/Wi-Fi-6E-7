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
#include "wifi7_rate.h"

/* Forward declarations */
static void wifi7_rate_work(struct work_struct *work);
static int wifi7_rate_minstrel_init(struct wifi7_rate_context *rc);
static void wifi7_rate_minstrel_deinit(struct wifi7_rate_context *rc);
static int wifi7_rate_minstrel_tx_status(struct wifi7_rate_context *rc,
                                        struct sk_buff *skb,
                                        bool success);
static struct wifi7_rate_entry *wifi7_rate_minstrel_get_next(struct wifi7_rate_context *rc);
static void wifi7_rate_minstrel_update_stats(struct wifi7_rate_context *rc);

/* Minstrel rate control algorithm */
static const struct wifi7_rate_algorithm minstrel_algorithm = {
    .name = "minstrel",
    .init = wifi7_rate_minstrel_init,
    .deinit = wifi7_rate_minstrel_deinit,
    .tx_status = wifi7_rate_minstrel_tx_status,
    .get_next_rate = wifi7_rate_minstrel_get_next,
    .update_stats = wifi7_rate_minstrel_update_stats,
};

/* Allocate rate control context */
struct wifi7_rate_context *wifi7_rate_alloc(struct wifi7_phy_dev *phy)
{
    struct wifi7_rate_context *rc;

    if (!phy)
        return NULL;

    rc = kzalloc(sizeof(*rc), GFP_KERNEL);
    if (!rc)
        return NULL;

    rc->phy = phy;
    rc->state = WIFI7_RATE_STATE_INIT;
    spin_lock_init(&rc->lock);

    /* Create workqueue */
    rc->rate_wq = alloc_workqueue("wifi7_rate_wq",
                                 WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
    if (!rc->rate_wq)
        goto err_free_rc;

    INIT_DELAYED_WORK(&rc->rate_work, wifi7_rate_work);

    /* Initialize rate table */
    if (wifi7_rate_init_table(rc))
        goto err_free_wq;

    /* Set default algorithm */
    rc->algorithm = &minstrel_algorithm;
    if (rc->algorithm->init && rc->algorithm->init(rc))
        goto err_free_wq;

    return rc;

err_free_wq:
    destroy_workqueue(rc->rate_wq);
err_free_rc:
    kfree(rc);
    return NULL;
}
EXPORT_SYMBOL_GPL(wifi7_rate_alloc);

void wifi7_rate_free(struct wifi7_rate_context *rc)
{
    if (!rc)
        return;

    if (rc->algorithm && rc->algorithm->deinit)
        rc->algorithm->deinit(rc);

    if (rc->rate_wq) {
        cancel_delayed_work_sync(&rc->rate_work);
        destroy_workqueue(rc->rate_wq);
    }

    kfree(rc);
}
EXPORT_SYMBOL_GPL(wifi7_rate_free);

/* Rate table initialization */
int wifi7_rate_init_table(struct wifi7_rate_context *rc)
{
    struct wifi7_rate_table *table = &rc->rate_table;
    int i = 0;

    /* Initialize rate table entries */
    /* Legacy rates */
    table->entries[i].mcs = 0;  /* BPSK 1/2 */
    table->entries[i].nss = 1;
    table->entries[i].rate_kbps = 6000;
    i++;

    /* HT/VHT rates */
    table->entries[i].mcs = 7;  /* 64-QAM 5/6 */
    table->entries[i].nss = 1;
    table->entries[i].rate_kbps = 65000;
    i++;

    /* HE/EHT rates */
    table->entries[i].mcs = 11;  /* 1024-QAM */
    table->entries[i].nss = 1;
    table->entries[i].rate_kbps = 143400;
    i++;

    table->entries[i].mcs = 13;  /* 4096-QAM */
    table->entries[i].nss = 1;
    table->entries[i].rate_kbps = 172100;
    i++;

    /* Set table parameters */
    table->num_entries = i;
    table->current_index = 0;
    table->probe_index = 0;
    table->lowest_index = 0;
    table->highest_index = i - 1;

    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_rate_init_table);

/* Minstrel algorithm implementation */
static int wifi7_rate_minstrel_init(struct wifi7_rate_context *rc)
{
    int i;

    /* Initialize statistics */
    for (i = 0; i < rc->rate_table.num_entries; i++) {
        struct wifi7_rate_stats *stats = &rc->rate_table.entries[i].stats;
        stats->attempts = 0;
        stats->successes = 0;
        stats->retries = 0;
        stats->failures = 0;
        stats->throughput = 0;
        stats->per = 0;
        stats->last_update = ktime_get();
    }

    /* Start rate control work */
    queue_delayed_work(rc->rate_wq, &rc->rate_work,
                      msecs_to_jiffies(WIFI7_RATE_SCALE_INTERVAL));

    return 0;
}

static void wifi7_rate_minstrel_deinit(struct wifi7_rate_context *rc)
{
    /* Nothing to do */
}

static int wifi7_rate_minstrel_tx_status(struct wifi7_rate_context *rc,
                                        struct sk_buff *skb,
                                        bool success)
{
    struct wifi7_rate_table *table = &rc->rate_table;
    struct wifi7_rate_entry *rate = &table->entries[table->current_index];
    unsigned long flags;

    spin_lock_irqsave(&rc->lock, flags);

    /* Update statistics */
    rate->stats.attempts++;
    if (success) {
        rate->stats.successes++;
    } else {
        rate->stats.failures++;
        if (skb->data_len > 0)  /* Retry */
            rate->stats.retries++;
    }

    /* Calculate PER */
    if (rate->stats.attempts > 0) {
        rate->stats.per = (rate->stats.failures * 100) /
                         rate->stats.attempts;
    }

    /* Update throughput estimate */
    if (rate->stats.successes > 0) {
        u32 duration = ktime_to_ms(ktime_sub(ktime_get(),
                                            rate->stats.last_update));
        if (duration > 0) {
            rate->stats.throughput = (rate->stats.successes *
                                    rate->rate_kbps) / duration;
        }
    }

    spin_unlock_irqrestore(&rc->lock, flags);

    return 0;
}

static struct wifi7_rate_entry *wifi7_rate_minstrel_get_next(struct wifi7_rate_context *rc)
{
    struct wifi7_rate_table *table = &rc->rate_table;
    struct wifi7_rate_entry *current_rate = &table->entries[table->current_index];
    unsigned long flags;
    int i, best_index = table->current_index;
    u32 best_throughput = 0;

    spin_lock_irqsave(&rc->lock, flags);

    /* Check if we should probe */
    if (rc->state == WIFI7_RATE_STATE_STABLE &&
        time_after(jiffies, rc->history.last_probe +
                  msecs_to_jiffies(WIFI7_RATE_PROBE_INTERVAL))) {
        rc->state = WIFI7_RATE_STATE_PROBING;
        table->probe_index = (table->current_index + 1) % table->num_entries;
        spin_unlock_irqrestore(&rc->lock, flags);
        return &table->entries[table->probe_index];
    }

    /* Find best performing rate */
    for (i = 0; i < table->num_entries; i++) {
        struct wifi7_rate_entry *rate = &table->entries[i];
        
        /* Skip rates with high PER */
        if (rate->stats.per > WIFI7_RATE_PER_THRESHOLD_HIGH)
            continue;

        /* Check SNR requirements */
        if (rc->snr < WIFI7_RATE_SNR_BPSK)
            continue;

        switch (rate->mcs) {
        case 13:  /* 4K-QAM */
            if (rc->snr < WIFI7_RATE_SNR_4KQAM)
                continue;
            break;
        case 11:  /* 1K-QAM */
            if (rc->snr < WIFI7_RATE_SNR_1KQAM)
                continue;
            break;
        case 7:   /* 64-QAM */
            if (rc->snr < WIFI7_RATE_SNR_64QAM)
                continue;
            break;
        }

        if (rate->stats.throughput > best_throughput) {
            best_throughput = rate->stats.throughput;
            best_index = i;
        }
    }

    /* Update rate selection */
    if (best_index != table->current_index) {
        table->current_index = best_index;
        rc->stats.rate_changes++;
        rc->stats.last_change = ktime_get();
    }

    spin_unlock_irqrestore(&rc->lock, flags);
    return &table->entries[table->current_index];
}

static void wifi7_rate_minstrel_update_stats(struct wifi7_rate_context *rc)
{
    struct wifi7_rate_table *table = &rc->rate_table;
    unsigned long flags;
    int i;

    spin_lock_irqsave(&rc->lock, flags);

    /* Update history */
    rc->history.mcs_history[rc->history.history_index] =
        table->entries[table->current_index].mcs;
    rc->history.per_history[rc->history.history_index] =
        table->entries[table->current_index].stats.per;
    rc->history.history_index = (rc->history.history_index + 1) %
                               WIFI7_RATE_HISTORY_SIZE;

    /* Reset statistics periodically */
    for (i = 0; i < table->num_entries; i++) {
        struct wifi7_rate_stats *stats = &table->entries[i].stats;
        if (time_after(jiffies, ktime_to_jiffies(stats->last_update) +
                      msecs_to_jiffies(WIFI7_RATE_SCALE_INTERVAL * 10))) {
            stats->attempts = 0;
            stats->successes = 0;
            stats->retries = 0;
            stats->failures = 0;
            stats->throughput = 0;
            stats->per = 0;
            stats->last_update = ktime_get();
        }
    }

    spin_unlock_irqrestore(&rc->lock, flags);
}

/* Rate control work */
static void wifi7_rate_work(struct work_struct *work)
{
    struct wifi7_rate_context *rc = container_of(work, struct wifi7_rate_context,
                                               rate_work.work);
    bool reschedule = true;

    /* Update statistics */
    if (rc->algorithm && rc->algorithm->update_stats)
        rc->algorithm->update_stats(rc);

    /* Handle state transitions */
    switch (rc->state) {
    case WIFI7_RATE_STATE_PROBING:
        /* Check probe results */
        if (rc->rate_table.entries[rc->rate_table.probe_index].stats.per >
            WIFI7_RATE_PER_THRESHOLD_HIGH) {
            rc->state = WIFI7_RATE_STATE_STABLE;
            rc->stats.probe_attempts++;
        } else {
            rc->state = WIFI7_RATE_STATE_STABLE;
            rc->stats.probe_successes++;
            rc->history.last_probe = jiffies;
        }
        break;

    case WIFI7_RATE_STATE_BACKOFF:
        /* Check if we can recover */
        if (rc->snr > WIFI7_RATE_SNR_QPSK &&
            rc->rate_table.entries[rc->rate_table.current_index].stats.per <
            WIFI7_RATE_PER_THRESHOLD_LOW) {
            rc->state = WIFI7_RATE_STATE_RECOVERY;
            rc->stats.recoveries++;
        }
        break;

    default:
        break;
    }

    /* Reschedule work */
    if (reschedule) {
        queue_delayed_work(rc->rate_wq, &rc->rate_work,
                          msecs_to_jiffies(WIFI7_RATE_SCALE_INTERVAL));
    }
}

/* Debug interface */
void wifi7_rate_dump_stats(struct wifi7_rate_context *rc)
{
    if (!rc)
        return;

    pr_info("WiFi 7 Rate Control Statistics:\n");
    pr_info("Rate changes: %u\n", rc->stats.rate_changes);
    pr_info("Probe attempts: %u\n", rc->stats.probe_attempts);
    pr_info("Probe successes: %u\n", rc->stats.probe_successes);
    pr_info("Fallbacks: %u\n", rc->stats.fallbacks);
    pr_info("Recoveries: %u\n", rc->stats.recoveries);

    pr_info("\nCurrent state:\n");
    pr_info("State: %d\n", rc->state);
    pr_info("RSSI: %d\n", rc->rssi);
    pr_info("SNR: %d\n", rc->snr);
    pr_info("Interference: %d\n", rc->interference);
}
EXPORT_SYMBOL_GPL(wifi7_rate_dump_stats);

/* Module initialization */
static int __init wifi7_rate_init(void)
{
    pr_info("WiFi 7 Rate Control initialized\n");
    return 0;
}

static void __exit wifi7_rate_exit(void)
{
    pr_info("WiFi 7 Rate Control unloaded\n");
}

module_init(wifi7_rate_init);
module_exit(wifi7_rate_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Rate Control");
MODULE_VERSION("1.0"); 