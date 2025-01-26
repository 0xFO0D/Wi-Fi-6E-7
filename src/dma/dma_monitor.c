#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include "../../include/dma/dma_core.h"
#include "../../include/dma/dma_monitor.h"

/* Performance thresholds */
#define DMA_LATENCY_THRESHOLD_NS   1000000  /* 1ms */
#define DMA_ERROR_THRESHOLD        100      /* errors per second */
#define DMA_RING_FULL_THRESHOLD    50       /* ring full events per second */
#define DMA_WATCHDOG_TIMEOUT_MS    5000     /* 5 seconds */

struct dma_monitor_stats {
    atomic64_t total_interrupts;
    atomic64_t error_interrupts;
    atomic64_t timeout_events;
    atomic64_t recovery_attempts;
    atomic64_t successful_recoveries;
    ktime_t last_interrupt;
    ktime_t last_error;
    u32 error_count_window;
    u32 ring_full_count_window;
    spinlock_t lock;
};

static struct dma_monitor_context {
    struct dentry *debugfs_root;
    struct dma_monitor_stats *channel_stats;
    struct delayed_work watchdog_work;
    struct workqueue_struct *monitor_wq;
    atomic_t is_suspended;
    u32 num_channels;
} monitor_ctx;

static void dma_monitor_dump_channel(struct seq_file *m, int channel)
{
    struct dma_monitor_stats *stats = &monitor_ctx.channel_stats[channel];
    ktime_t now = ktime_get();
    unsigned long flags;

    seq_printf(m, "Channel %d Statistics:\n", channel);
    seq_printf(m, "  Total Interrupts: %lld\n", 
               atomic64_read(&stats->total_interrupts));
    seq_printf(m, "  Error Interrupts: %lld\n",
               atomic64_read(&stats->error_interrupts));
    seq_printf(m, "  Timeout Events: %lld\n",
               atomic64_read(&stats->timeout_events));
    seq_printf(m, "  Recovery Attempts: %lld\n",
               atomic64_read(&stats->recovery_attempts));
    seq_printf(m, "  Successful Recoveries: %lld\n",
               atomic64_read(&stats->successful_recoveries));

    spin_lock_irqsave(&stats->lock, flags);
    seq_printf(m, "  Time since last interrupt: %lldns\n",
               ktime_to_ns(ktime_sub(now, stats->last_interrupt)));
    seq_printf(m, "  Time since last error: %lldns\n",
               ktime_to_ns(ktime_sub(now, stats->last_error)));
    seq_printf(m, "  Recent error count: %u\n", stats->error_count_window);
    seq_printf(m, "  Recent ring full count: %u\n", stats->ring_full_count_window);
    spin_unlock_irqrestore(&stats->lock, flags);
}

static int dma_monitor_show(struct seq_file *m, void *v)
{
    int i;
    
    if (atomic_read(&monitor_ctx.is_suspended)) {
        seq_puts(m, "DMA monitoring is suspended\n");
        return 0;
    }

    for (i = 0; i < monitor_ctx.num_channels; i++)
        dma_monitor_dump_channel(m, i);

    return 0;
}

static int dma_monitor_open(struct inode *inode, struct file *file)
{
    return single_open(file, dma_monitor_show, NULL);
}

static const struct file_operations dma_monitor_fops = {
    .owner = THIS_MODULE,
    .open = dma_monitor_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static void dma_monitor_watchdog(struct work_struct *work)
{
    int i;
    ktime_t now = ktime_get();
    bool need_recovery = false;

    if (atomic_read(&monitor_ctx.is_suspended))
        goto reschedule;

    for (i = 0; i < monitor_ctx.num_channels; i++) {
        struct dma_monitor_stats *stats = &monitor_ctx.channel_stats[i];
        unsigned long flags;
        s64 delta_ns;

        spin_lock_irqsave(&stats->lock, flags);
        
        /* Check for timeout */
        delta_ns = ktime_to_ns(ktime_sub(now, stats->last_interrupt));
        if (delta_ns > DMA_WATCHDOG_TIMEOUT_MS * NSEC_PER_MSEC) {
            atomic64_inc(&stats->timeout_events);
            need_recovery = true;
        }

        /* Reset error window counters periodically */
        if (delta_ns > NSEC_PER_SEC) {
            stats->error_count_window = 0;
            stats->ring_full_count_window = 0;
        }

        spin_unlock_irqrestore(&stats->lock, flags);
    }

    if (need_recovery)
        schedule_work(&monitor_ctx.watchdog_work.work);

reschedule:
    queue_delayed_work(monitor_ctx.monitor_wq, &monitor_ctx.watchdog_work,
                      msecs_to_jiffies(DMA_WATCHDOG_TIMEOUT_MS / 2));
}

void wifi67_dma_monitor_irq(struct wifi67_priv *priv, u32 channel_id, bool is_error)
{
    struct dma_monitor_stats *stats;
    unsigned long flags;
    ktime_t now = ktime_get();

    if (channel_id >= monitor_ctx.num_channels)
        return;

    stats = &monitor_ctx.channel_stats[channel_id];
    atomic64_inc(&stats->total_interrupts);

    if (is_error) {
        atomic64_inc(&stats->error_interrupts);
        spin_lock_irqsave(&stats->lock, flags);
        stats->last_error = now;
        stats->error_count_window++;
        spin_unlock_irqrestore(&stats->lock, flags);
    }

    spin_lock_irqsave(&stats->lock, flags);
    stats->last_interrupt = now;
    spin_unlock_irqrestore(&stats->lock, flags);
}
EXPORT_SYMBOL_GPL(wifi67_dma_monitor_irq);

void wifi67_dma_monitor_ring_full(struct wifi67_priv *priv, u32 channel_id)
{
    struct dma_monitor_stats *stats;
    unsigned long flags;

    if (channel_id >= monitor_ctx.num_channels)
        return;

    stats = &monitor_ctx.channel_stats[channel_id];
    spin_lock_irqsave(&stats->lock, flags);
    stats->ring_full_count_window++;
    spin_unlock_irqrestore(&stats->lock, flags);
}
EXPORT_SYMBOL_GPL(wifi67_dma_monitor_ring_full);

int wifi67_dma_monitor_init(struct wifi67_priv *priv)
{
    int i;

    monitor_ctx.num_channels = WIFI67_DMA_MAX_CHANNELS;
    atomic_set(&monitor_ctx.is_suspended, 0);

    /* Allocate per-channel statistics */
    monitor_ctx.channel_stats = kcalloc(monitor_ctx.num_channels,
                                      sizeof(*monitor_ctx.channel_stats),
                                      GFP_KERNEL);
    if (!monitor_ctx.channel_stats)
        return -ENOMEM;

    /* Initialize per-channel statistics */
    for (i = 0; i < monitor_ctx.num_channels; i++) {
        struct dma_monitor_stats *stats = &monitor_ctx.channel_stats[i];
        atomic64_set(&stats->total_interrupts, 0);
        atomic64_set(&stats->error_interrupts, 0);
        atomic64_set(&stats->timeout_events, 0);
        atomic64_set(&stats->recovery_attempts, 0);
        atomic64_set(&stats->successful_recoveries, 0);
        stats->last_interrupt = ktime_get();
        stats->last_error = ktime_get();
        stats->error_count_window = 0;
        stats->ring_full_count_window = 0;
        spin_lock_init(&stats->lock);
    }

    /* Create debugfs entries */
    monitor_ctx.debugfs_root = debugfs_create_dir("wifi67_dma", NULL);
    if (!monitor_ctx.debugfs_root)
        goto err_free_stats;

    if (!debugfs_create_file("monitor", 0444, monitor_ctx.debugfs_root,
                            NULL, &dma_monitor_fops))
        goto err_remove_debugfs;

    /* Initialize monitoring workqueue */
    monitor_ctx.monitor_wq = create_singlethread_workqueue("wifi67_dma_monitor");
    if (!monitor_ctx.monitor_wq)
        goto err_remove_debugfs;

    /* Initialize and schedule watchdog */
    INIT_DELAYED_WORK(&monitor_ctx.watchdog_work, dma_monitor_watchdog);
    queue_delayed_work(monitor_ctx.monitor_wq, &monitor_ctx.watchdog_work,
                      msecs_to_jiffies(DMA_WATCHDOG_TIMEOUT_MS));

    return 0;

err_remove_debugfs:
    debugfs_remove_recursive(monitor_ctx.debugfs_root);
err_free_stats:
    kfree(monitor_ctx.channel_stats);
    return -ENOMEM;
}
EXPORT_SYMBOL_GPL(wifi67_dma_monitor_init);

void wifi67_dma_monitor_deinit(struct wifi67_priv *priv)
{
    cancel_delayed_work_sync(&monitor_ctx.watchdog_work);
    destroy_workqueue(monitor_ctx.monitor_wq);
    debugfs_remove_recursive(monitor_ctx.debugfs_root);
    kfree(monitor_ctx.channel_stats);
}
EXPORT_SYMBOL_GPL(wifi67_dma_monitor_deinit); 