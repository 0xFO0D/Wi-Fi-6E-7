#ifndef _WIFI67_PERF_TYPES_H_
#define _WIFI67_PERF_TYPES_H_

#include <linux/types.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>

/* Performance monitoring intervals */
#define WIFI67_PERF_INTERVAL_MS     1000    /* 1 second */
#define WIFI67_PERF_HISTORY_SIZE    60      /* 1 minute history */

/* Performance event types */
#define WIFI67_PERF_TX_EVENT        BIT(0)
#define WIFI67_PERF_RX_EVENT        BIT(1)
#define WIFI67_PERF_ERROR_EVENT     BIT(2)
#define WIFI67_PERF_QUEUE_EVENT     BIT(3)
#define WIFI67_PERF_ALL_EVENTS      0xFFFFFFFF

struct wifi67_perf_stats {
    /* Throughput statistics */
    u32 tx_bytes;
    u32 rx_bytes;
    u32 tx_packets;
    u32 rx_packets;
    
    /* Error statistics */
    u32 tx_errors;
    u32 rx_errors;
    u32 tx_retries;
    u32 rx_drops;
    
    /* Latency statistics (in microseconds) */
    u32 tx_latency_avg;
    u32 rx_latency_avg;
    u32 tx_latency_max;
    u32 rx_latency_max;
    
    /* Queue statistics */
    u32 tx_queue_len;
    u32 rx_queue_len;
    u32 tx_queue_drops;
    u32 rx_queue_drops;
    
    /* Timestamp */
    u64 timestamp;
};

struct wifi67_perf_monitor {
    /* Statistics */
    struct wifi67_perf_stats curr_stats;
    struct wifi67_perf_stats history[WIFI67_PERF_HISTORY_SIZE];
    int history_index;
    
    /* Event mask */
    u32 event_mask;
    
    /* Sampling */
    struct delayed_work dwork;
    atomic_t enabled;
    atomic64_t total_samples;
    
    /* Locks */
    spinlock_t lock;
};

#endif /* _WIFI67_PERF_TYPES_H_ */ 