#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include "../../include/perf/perf_monitor.h"
#include "../../include/debug/debug.h"

static void wifi67_perf_process_stats(struct wifi67_perf_monitor *perf)
{
    u64 now = ktime_get_ns();
    u64 delta = now - ktime_to_ns(perf->last_sample);
    struct wifi67_priv *priv = container_of(perf, struct wifi67_priv, perf);
    
    // Calculate rates
    u32 tx_rate = atomic_read(&perf->tx_bytes) * NSEC_PER_SEC / delta;
    u32 rx_rate = atomic_read(&perf->rx_bytes) * NSEC_PER_SEC / delta;
    
    wifi67_debug(priv, WIFI67_DEBUG_INFO,
               "Performance stats:\n"
               "  TX: %u packets, %u bytes, %u Mbps\n"
               "  RX: %u packets, %u bytes, %u Mbps\n"
               "  Errors: TX=%u, RX=%u, HW=%u, FIFO=%u, DMA=%u\n",
               atomic_read(&perf->tx_packets),
               atomic_read(&perf->tx_bytes),
               tx_rate / 1000000,
               atomic_read(&perf->rx_packets), 
               atomic_read(&perf->rx_bytes),
               rx_rate / 1000000,
               atomic_read(&perf->tx_errors),
               atomic_read(&perf->rx_errors),
               perf->hw_errors,
               perf->fifo_errors,
               perf->dma_errors);
               
    perf->last_sample = ktime_get();
}

static void wifi67_perf_work(struct work_struct *work)
{
    struct wifi67_perf_monitor *perf = container_of(work, struct wifi67_perf_monitor,
                                                  dwork.work);
    struct wifi67_priv *priv = container_of(perf, struct wifi67_priv, perf);
    
    if (!perf->enabled)
        return;
        
    wifi67_perf_process_stats(perf);
    
    schedule_delayed_work(&perf->dwork, 
                         msecs_to_jiffies(perf->sample_interval));
}

int wifi67_perf_init(struct wifi67_priv *priv)
{
    struct wifi67_perf_monitor *perf = &priv->perf;
    
    INIT_DELAYED_WORK(&perf->dwork, wifi67_perf_work);
    
    atomic_set(&perf->tx_packets, 0);
    atomic_set(&perf->rx_packets, 0);
    atomic_set(&perf->tx_bytes, 0);
    atomic_set(&perf->rx_bytes, 0);
    atomic_set(&perf->tx_errors, 0);
    atomic_set(&perf->rx_errors, 0);
    atomic_set(&perf->tx_dropped, 0);
    atomic_set(&perf->rx_dropped, 0);
    
    perf->hw_errors = 0;
    perf->fifo_errors = 0;
    perf->dma_errors = 0;
    
    perf->last_sample = ktime_get();
    perf->sample_interval = WIFI67_PERF_SAMPLE_INTERVAL;
    perf->enabled = true;
    
    schedule_delayed_work(&perf->dwork,
                         msecs_to_jiffies(perf->sample_interval));
                         
    return 0;
}

void wifi67_perf_deinit(struct wifi67_priv *priv)
{
    struct wifi67_perf_monitor *perf = &priv->perf;
    
    perf->enabled = false;
    cancel_delayed_work_sync(&perf->dwork);
}

void wifi67_perf_sample(struct wifi67_priv *priv)
{
    struct wifi67_perf_monitor *perf = &priv->perf;
    
    if (!perf->enabled)
        return;
        
    wifi67_perf_process_stats(perf);
}

EXPORT_SYMBOL_GPL(wifi67_perf_init);
EXPORT_SYMBOL_GPL(wifi67_perf_deinit);
EXPORT_SYMBOL_GPL(wifi67_perf_sample); 