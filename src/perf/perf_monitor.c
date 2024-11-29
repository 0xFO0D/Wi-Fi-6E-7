#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "../../include/core/wifi67.h"
#include "../../include/perf/perf_monitor.h"

static void wifi67_perf_process_stats(struct work_struct *work)
{
    struct wifi67_perf_monitor *perf = container_of(to_delayed_work(work),
                                                  struct wifi67_perf_monitor,
                                                  dwork);
    struct wifi67_priv *priv = container_of(perf, struct wifi67_priv, perf);
    unsigned long flags;

    if (!atomic_read(&perf->enabled))
        return;

    spin_lock_irqsave(&perf->lock, flags);

    /* Store current stats in history */
    memcpy(&perf->history[perf->history_index], &perf->curr_stats,
           sizeof(struct wifi67_perf_stats));

    /* Update history index */
    perf->history_index = (perf->history_index + 1) % WIFI67_PERF_HISTORY_SIZE;

    /* Reset current stats */
    memset(&perf->curr_stats, 0, sizeof(struct wifi67_perf_stats));
    perf->curr_stats.timestamp = ktime_get_real_ns();

    spin_unlock_irqrestore(&perf->lock, flags);

    /* Schedule next stats processing */
    if (atomic_read(&perf->enabled)) {
        schedule_delayed_work(&perf->dwork,
                            msecs_to_jiffies(WIFI67_PERF_INTERVAL_MS));
    }
}

int wifi67_perf_init(struct wifi67_priv *priv)
{
    struct wifi67_perf_monitor *perf = &priv->perf;

    spin_lock_init(&perf->lock);
    atomic_set(&perf->enabled, 0);
    atomic64_set(&perf->total_samples, 0);
    perf->event_mask = WIFI67_PERF_ALL_EVENTS;
    perf->history_index = 0;

    INIT_DELAYED_WORK(&perf->dwork, wifi67_perf_process_stats);

    return 0;
}

void wifi67_perf_deinit(struct wifi67_priv *priv)
{
    struct wifi67_perf_monitor *perf = &priv->perf;

    atomic_set(&perf->enabled, 0);
    cancel_delayed_work_sync(&perf->dwork);
}

void wifi67_perf_sample(struct wifi67_priv *priv)
{
    struct wifi67_perf_monitor *perf = &priv->perf;
    unsigned long flags;

    if (!atomic_read(&perf->enabled))
        return;

    spin_lock_irqsave(&perf->lock, flags);
    atomic64_inc(&perf->total_samples);
    spin_unlock_irqrestore(&perf->lock, flags);
} 