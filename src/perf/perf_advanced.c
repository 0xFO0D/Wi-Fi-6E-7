#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "../../include/perf/perf_advanced.h"
#include "../../include/debug/debug.h"

/* Ring buffer size for historical data */
#define WIFI67_HIST_SIZE 1024
#define WIFI67_SAMPLE_INTERVAL_MS 100

struct wifi67_perf_ring {
    struct wifi67_perf_sample samples[WIFI67_HIST_SIZE];
    unsigned int head;
    unsigned int tail;
    spinlock_t lock;
};

struct wifi67_perf_advanced {
    struct wifi67_perf_ring history;
    struct delayed_work sample_work;
    atomic_t active;
    
    /* Performance metrics */
    struct {
        atomic_t tx_attempts;
        atomic_t tx_success;
        atomic_t rx_packets;
        atomic_t rx_dropped;
        atomic_t tx_retries;
        atomic_t tx_failed;
        atomic_t rx_crc_errors;
        atomic_t rx_decrypt_fails;
    } counters;
    
    /* Rate adaptation metrics */
    struct {
        u32 current_rate;
        u32 target_rate;
        u32 max_rate;
        u32 min_rate;
        u8 rate_scale_up_thresh;
        u8 rate_scale_down_thresh;
        u8 consecutive_failures;
        bool probing;
    } rate_ctrl;
    
    /* Thermal metrics */
    struct {
        s32 current_temp;
        s32 max_temp;
        u32 throttle_events;
        ktime_t last_throttle;
    } thermal;
    
    /* Debug interface */
    struct dentry *debugfs_dir;
    struct dentry *stats_file;
    struct dentry *history_file;
};

static void wifi67_perf_update_rate(struct wifi67_perf_advanced *perf)
{
    u32 success_ratio;
    bool should_scale = false;
    
    success_ratio = (atomic_read(&perf->counters.tx_success) * 100) /
                    max(1U, atomic_read(&perf->counters.tx_attempts));
                    
    if (success_ratio > perf->rate_ctrl.rate_scale_up_thresh) {
        if (perf->rate_ctrl.current_rate < perf->rate_ctrl.max_rate) {
            perf->rate_ctrl.current_rate = min(
                perf->rate_ctrl.current_rate * 3 / 2,
                perf->rate_ctrl.max_rate
            );
            should_scale = true;
        }
    } else if (success_ratio < perf->rate_ctrl.rate_scale_down_thresh) {
        if (perf->rate_ctrl.current_rate > perf->rate_ctrl.min_rate) {
            perf->rate_ctrl.current_rate = max(
                perf->rate_ctrl.current_rate * 2 / 3,
                perf->rate_ctrl.min_rate
            );
            should_scale = true;
        }
    }
    
    if (should_scale) {
        perf->rate_ctrl.probing = false;
        perf->rate_ctrl.consecutive_failures = 0;
    }
}

static void wifi67_perf_sample_stats(struct wifi67_perf_advanced *perf)
{
    struct wifi67_perf_sample sample;
    unsigned long flags;
    
    sample.timestamp = ktime_get_real_ns();
    sample.tx_attempts = atomic_xchg(&perf->counters.tx_attempts, 0);
    sample.tx_success = atomic_xchg(&perf->counters.tx_success, 0);
    sample.rx_packets = atomic_xchg(&perf->counters.rx_packets, 0);
    sample.rx_dropped = atomic_xchg(&perf->counters.rx_dropped, 0);
    sample.tx_retries = atomic_xchg(&perf->counters.tx_retries, 0);
    sample.tx_failed = atomic_xchg(&perf->counters.tx_failed, 0);
    sample.current_rate = perf->rate_ctrl.current_rate;
    sample.temperature = perf->thermal.current_temp;
    
    spin_lock_irqsave(&perf->history.lock, flags);
    
    perf->history.samples[perf->history.head] = sample;
    perf->history.head = (perf->history.head + 1) % WIFI67_HIST_SIZE;
    
    if (perf->history.head == perf->history.tail)
        perf->history.tail = (perf->history.tail + 1) % WIFI67_HIST_SIZE;
        
    spin_unlock_irqrestore(&perf->history.lock, flags);
    
    wifi67_perf_update_rate(perf);
}

static void wifi67_perf_work_handler(struct work_struct *work)
{
    struct wifi67_perf_advanced *perf = container_of(work,
        struct wifi67_perf_advanced, sample_work.work);
        
    if (atomic_read(&perf->active)) {
        wifi67_perf_sample_stats(perf);
        schedule_delayed_work(&perf->sample_work,
                            msecs_to_jiffies(WIFI67_SAMPLE_INTERVAL_MS));
    }
}

/* Debug filesystem interface */
static int wifi67_perf_stats_show(struct seq_file *file, void *v)
{
    struct wifi67_perf_advanced *perf = file->private;
    
    seq_printf(file,
        "Performance Statistics:\n"
        "TX Success Rate: %d%%\n"
        "Current Rate: %u Mbps\n"
        "Temperature: %d°C\n"
        "Throttle Events: %u\n"
        "TX Retries: %u\n"
        "RX Dropped: %u\n",
        (atomic_read(&perf->counters.tx_success) * 100) /
            max(1U, atomic_read(&perf->counters.tx_attempts)),
        perf->rate_ctrl.current_rate,
        perf->thermal.current_temp,
        perf->thermal.throttle_events,
        atomic_read(&perf->counters.tx_retries),
        atomic_read(&perf->counters.rx_dropped));
        
    return 0;
}

static int wifi67_perf_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, wifi67_perf_stats_show, inode->i_private);
}

static const struct file_operations wifi67_perf_stats_fops = {
    .owner = THIS_MODULE,
    .open = wifi67_perf_stats_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

int wifi67_perf_advanced_init(struct wifi67_priv *priv)
{
    struct wifi67_perf_advanced *perf;
    
    perf = kzalloc(sizeof(*perf), GFP_KERNEL);
    if (!perf)
        return -ENOMEM;
        
    spin_lock_init(&perf->history.lock);
    INIT_DELAYED_WORK(&perf->sample_work, wifi67_perf_work_handler);
    
    /* Initialize rate control parameters */
    perf->rate_ctrl.current_rate = 6;  /* Start with basic rate */
    perf->rate_ctrl.min_rate = 6;
    perf->rate_ctrl.max_rate = 1200;   /* 1.2 Gbps */
    perf->rate_ctrl.rate_scale_up_thresh = 85;
    perf->rate_ctrl.rate_scale_down_thresh = 60;
    
    /* Create debugfs entries */
    perf->debugfs_dir = debugfs_create_dir("wifi67_perf", NULL);
    if (IS_ERR(perf->debugfs_dir)) {
        kfree(perf);
        return PTR_ERR(perf->debugfs_dir);
    }
    
    perf->stats_file = debugfs_create_file("stats", 0444,
                                          perf->debugfs_dir,
                                          perf,
                                          &wifi67_perf_stats_fops);
    
    atomic_set(&perf->active, 1);
    priv->perf_advanced = perf;
    
    schedule_delayed_work(&perf->sample_work,
                         msecs_to_jiffies(WIFI67_SAMPLE_INTERVAL_MS));
    
    return 0;
}

void wifi67_perf_advanced_deinit(struct wifi67_priv *priv)
{
    struct wifi67_perf_advanced *perf = priv->perf_advanced;
    
    if (!perf)
        return;
        
    atomic_set(&perf->active, 0);
    cancel_delayed_work_sync(&perf->sample_work);
    debugfs_remove_recursive(perf->debugfs_dir);
    kfree(perf);
    priv->perf_advanced = NULL;
}

EXPORT_SYMBOL_GPL(wifi67_perf_advanced_init);
EXPORT_SYMBOL_GPL(wifi67_perf_advanced_deinit); 