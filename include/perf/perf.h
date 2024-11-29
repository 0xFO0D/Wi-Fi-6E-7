#ifndef _WIFI67_PERF_H_
#define _WIFI67_PERF_H_

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>

struct wifi67_perf_monitor {
    struct delayed_work dwork;
    atomic_t tx_packets;
    atomic_t rx_packets;
    atomic_t tx_bytes;
    atomic_t rx_bytes;
    atomic_t tx_errors;
    atomic_t rx_errors;
    u32 hw_errors;
    u32 fifo_errors; 
    u32 dma_errors;
    ktime_t last_sample;
};

#endif /* _WIFI67_PERF_H_ */ 