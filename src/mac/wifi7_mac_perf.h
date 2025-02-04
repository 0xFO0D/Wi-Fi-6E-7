/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_MAC_PERF_H
#define __WIFI7_MAC_PERF_H

#include "wifi7_mac.h"

/* Performance tuning parameters */
#define PERF_MAX_QUEUED_FRAMES   1024    /* Need to validate this */
#define PERF_MIN_AMPDU_LEN       8       /* Too small? */
#define PERF_MAX_RETRIES         3       /* Conservative */

/* XXX: These thresholds need tuning */
#define PERF_HIGH_LOAD_THRESH    80      /* percent */
#define PERF_MEM_PRESSURE_THRESH 75      /* percent */

/* FIXME: Memory leak debugging */
#define DEBUG_TRACK_ALLOCS       1
#define MAX_TRACKED_ALLOCS       1000

/* Performance monitoring structure */
struct wifi7_perf_stats {
    /* Memory tracking */
    atomic_t current_allocs;
    atomic_t peak_allocs;
    atomic_t failed_allocs;
    
    /* Queue monitoring */
    u32 queue_overflow_count;
    u32 dropped_frames;
    u32 retry_count;
    
    /* Throughput */
    u64 bytes_processed;
    ktime_t last_update;
    u32 current_throughput;  /* Mbps */
    
    /* TODO: Add more metrics
     * - CPU utilization
     * - Cache hit rates
     * - DMA efficiency
     */
};

#ifdef DEBUG_TRACK_ALLOCS
struct alloc_track {
    void *addr;
    size_t size;
    const char *func;
    int line;
};

struct alloc_debug {
    struct alloc_track tracks[MAX_TRACKED_ALLOCS];
    atomic_t count;
    spinlock_t lock;
};
#endif

/* Memory management wrappers */
void *wifi7_mac_alloc_mem(size_t size, gfp_t flags, 
                         const char *func, int line);
void wifi7_mac_free_mem(void *ptr, const char *func, int line);

/* Queue management helpers */
bool wifi7_mac_queue_full(struct wifi7_mac_dev *dev, u8 queue_id);
void wifi7_mac_queue_cleanup(struct wifi7_mac_dev *dev, u8 queue_id);

/* Performance monitoring */
void wifi7_mac_update_perf_stats(struct wifi7_mac_dev *dev);
void wifi7_mac_dump_perf_stats(struct wifi7_mac_dev *dev);

#ifdef DEBUG_TRACK_ALLOCS
void wifi7_mac_dump_allocs(struct wifi7_mac_dev *dev);
#endif

/* Optimization hints */
#define likely_high_load(dev) \
    (atomic_read(&(dev)->stats.queue_length) > PERF_HIGH_LOAD_THRESH)

#define under_memory_pressure(dev) \
    (atomic_read(&(dev)->stats.current_allocs) > PERF_MEM_PRESSURE_THRESH)

/* Memory allocation tracking macros */
#ifdef DEBUG_TRACK_ALLOCS
#define wifi7_alloc(size, flags) \
    wifi7_mac_alloc_mem(size, flags, __func__, __LINE__)
#define wifi7_free(ptr) \
    wifi7_mac_free_mem(ptr, __func__, __LINE__)
#else
#define wifi7_alloc(size, flags) kmalloc(size, flags)
#define wifi7_free(ptr) kfree(ptr)
#endif

/*
 * TODO: Implement these optimizations
 * - Batch processing for small packets
 * - Zero-copy path for large transfers
 * - Dynamic queue threshold adjustment
 * - Adaptive retry mechanism
 */

#endif /* __WIFI7_MAC_PERF_H */ 