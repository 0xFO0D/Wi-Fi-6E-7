/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_PHY_PERF_H
#define __WIFI7_PHY_PERF_H

#include "wifi7_phy.h"

/* Performance tuning parameters */
#define PHY_PERF_MAX_AGC_RETRIES    3
#define PHY_PERF_MIN_CALIB_INTERVAL 100  /* ms */
#define PHY_PERF_MAX_TEMP_DELTA     5    /* degrees C */

/* DMA optimization thresholds */
#define PHY_DMA_SMALL_PKT_THRESH    256
#define PHY_DMA_LARGE_PKT_THRESH    1500
#define PHY_DMA_MAX_BURST_SIZE      32

/* Power optimization */
#define PHY_POWER_SAVE_THRESH_LOW   10   /* percent */
#define PHY_POWER_SAVE_THRESH_HIGH  80   /* percent */
#define PHY_POWER_SCALE_STEP        2    /* dB */

/* Performance monitoring structure */
struct wifi7_phy_perf_stats {
    /* AGC performance */
    atomic_t agc_retries;
    atomic_t agc_failures;
    u32 last_gain_update;
    
    /* DMA statistics */
    atomic_t dma_underruns;
    atomic_t dma_overruns;
    u32 avg_burst_size;
    u32 max_burst_size;
    
    /* Power tracking */
    atomic_t power_changes;
    ktime_t last_power_change;
    s8 min_power_level;
    s8 max_power_level;
    
    /* Thermal stats */
    u32 temp_readings[8];  /* Circular buffer */
    u8 temp_idx;
    u32 max_temp;
    u32 temp_throttle_count;
    
    /* Constellation performance */
    struct {
        u32 switches;
        u32 fallbacks;
        u32 error_count;
        u32 success_count;
    } qam_stats;
};

/* Performance optimization hints */
static inline bool wifi7_phy_needs_calibration(struct wifi7_phy_dev *phy)
{
    return time_after(jiffies, 
                     phy->last_calib + msecs_to_jiffies(PHY_PERF_MIN_CALIB_INTERVAL));
}

static inline bool wifi7_phy_temp_stable(struct wifi7_phy_dev *phy)
{
    return abs(phy->power_tracking.temperature - phy->last_temp) <= 
           PHY_PERF_MAX_TEMP_DELTA * 1000;
}

static inline bool wifi7_phy_power_optimize(struct wifi7_phy_dev *phy)
{
    int load = atomic_read(&phy->channel_state.busy_time);
    return (load < PHY_POWER_SAVE_THRESH_LOW || 
            load > PHY_POWER_SAVE_THRESH_HIGH);
}

/* DMA optimization helpers */
static inline int wifi7_phy_optimize_dma(struct wifi7_phy_dev *phy,
                                       u32 pkt_size)
{
    if (pkt_size <= PHY_DMA_SMALL_PKT_THRESH)
        return 1;  /* Single descriptor */
    else if (pkt_size <= PHY_DMA_LARGE_PKT_THRESH)
        return 4;  /* Medium chain */
    else
        return PHY_DMA_MAX_BURST_SIZE;  /* Max chain */
}

/* Power scaling helpers */
static inline s8 wifi7_phy_scale_power(struct wifi7_phy_dev *phy,
                                     s8 current_power,
                                     bool increase)
{
    s8 new_power = current_power + (increase ? 1 : -1) * PHY_POWER_SCALE_STEP;
    
    /* Clamp to valid range */
    if (new_power > phy->power_tracking.max_power)
        new_power = phy->power_tracking.max_power;
    else if (new_power < 0)
        new_power = 0;
        
    return new_power;
}

/* Performance monitoring */
void wifi7_phy_update_perf_stats(struct wifi7_phy_dev *phy);
void wifi7_phy_dump_perf_stats(struct wifi7_phy_dev *phy);

/* TODO: Add these optimizations
 * - Adaptive AGC thresholds
 * - Dynamic calibration intervals
 * - Thermal prediction
 * - DMA chaining optimization
 * - Power control algorithms
 */

#endif /* __WIFI7_PHY_PERF_H */ 