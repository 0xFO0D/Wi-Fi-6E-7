/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_BANDWIDTH_H
#define __WIFI7_BANDWIDTH_H

#include <linux/types.h>
#include <linux/ieee80211.h>
#include "wifi7_phy.h"

/* Bandwidth capabilities */
#define WIFI7_BW_20_MHZ          20
#define WIFI7_BW_40_MHZ          40
#define WIFI7_BW_80_MHZ          80
#define WIFI7_BW_160_MHZ         160
#define WIFI7_BW_320_MHZ         320

/* Channel state thresholds */
#define WIFI7_MIN_SNR_320MHZ     25  /* dB */
#define WIFI7_MIN_SNR_160MHZ     22
#define WIFI7_MIN_SNR_80MHZ      19
#define WIFI7_MIN_SNR_40MHZ      16
#define WIFI7_MIN_SNR_20MHZ      13

/* Interference thresholds */
#define WIFI7_MAX_INTERFERENCE    -62  /* dBm */
#define WIFI7_CLEAR_INTERFERENCE  -82

/* Bandwidth adaptation intervals */
#define WIFI7_BW_MIN_ADAPT_INTERVAL_MS   100
#define WIFI7_BW_MAX_ADAPT_INTERVAL_MS   1000
#define WIFI7_BW_MEASURE_INTERVAL_MS     20

/* Puncturing patterns */
#define WIFI7_PUNCT_PATTERN_NONE  0x0
#define WIFI7_PUNCT_PATTERN_LOW   0x1
#define WIFI7_PUNCT_PATTERN_MID   0x3
#define WIFI7_PUNCT_PATTERN_HIGH  0x7

/* Bandwidth states */
enum wifi7_bw_state {
    WIFI7_BW_STATE_INIT = 0,
    WIFI7_BW_STATE_MEASURING,
    WIFI7_BW_STATE_ADAPTING,
    WIFI7_BW_STATE_STABLE,
    WIFI7_BW_STATE_PUNCTURED,
    WIFI7_BW_STATE_ERROR
};

/* Channel quality metrics */
struct wifi7_channel_quality {
    s8 rssi;                  /* Signal strength in dBm */
    u8 snr;                   /* Signal-to-noise ratio */
    u8 interference_level;    /* Interference metric */
    u8 channel_utilization;   /* Percentage of channel busy time */
    
    /* Subband metrics for puncturing decisions */
    struct {
        s8 rssi;
        u8 interference;
        bool punctured;
    } subbands[16];          /* For 320MHz with 20MHz granularity */
    
    /* Time tracking */
    ktime_t last_update;
};

/* Bandwidth configuration */
struct wifi7_bw_config {
    u16 primary_width;        /* Primary channel width */
    u16 secondary_width;      /* Secondary channel width (for MLO) */
    u8 punct_pattern;         /* Current puncturing pattern */
    bool dynamic_enabled;     /* Dynamic adaptation enabled */
    
    /* Operating parameters */
    struct {
        u32 min_duration;     /* Minimum time before width change */
        u32 max_duration;     /* Maximum time without evaluation */
        u8 up_threshold;      /* Threshold to increase width */
        u8 down_threshold;    /* Threshold to decrease width */
    } params;
};

/* Bandwidth statistics */
struct wifi7_bw_stats {
    /* Width changes */
    u32 width_increases;
    u32 width_decreases;
    u32 punct_pattern_changes;
    
    /* Performance metrics */
    u32 throughput_samples[5];  /* For each width */
    u32 error_counts[5];
    u32 retries[5];
    
    /* Interference tracking */
    u32 interference_events;
    u32 recovery_events;
    ktime_t last_interference;
};

/* Main bandwidth context */
struct wifi7_bw_context {
    struct wifi7_phy_dev *phy;
    
    /* Current state */
    enum wifi7_bw_state state;
    struct wifi7_bw_config config;
    struct wifi7_channel_quality quality;
    
    /* Operation control */
    spinlock_t bw_lock;
    struct workqueue_struct *bw_wq;
    struct delayed_work monitor_work;
    
    /* Statistics */
    struct wifi7_bw_stats stats;
    
    /* MLO coordination */
    bool mlo_active;
    u8 link_id;
    struct wifi7_bw_context *peer_links[2];  /* For MLO */
};

/* Function prototypes */
struct wifi7_bw_context *wifi7_bw_alloc(struct wifi7_phy_dev *phy);
void wifi7_bw_free(struct wifi7_bw_context *bw);

/* Configuration */
int wifi7_bw_set_config(struct wifi7_bw_context *bw,
                       const struct wifi7_bw_config *config);
int wifi7_bw_get_config(struct wifi7_bw_context *bw,
                       struct wifi7_bw_config *config);

/* Operation control */
int wifi7_bw_start(struct wifi7_bw_context *bw);
void wifi7_bw_stop(struct wifi7_bw_context *bw);
int wifi7_bw_update_quality(struct wifi7_bw_context *bw,
                           const struct wifi7_channel_quality *quality);

/* Bandwidth adaptation */
int wifi7_bw_request_width(struct wifi7_bw_context *bw,
                          u16 width,
                          bool force);
int wifi7_bw_set_puncturing(struct wifi7_bw_context *bw,
                           u8 pattern);

/* MLO coordination */
int wifi7_bw_mlo_register_link(struct wifi7_bw_context *bw,
                              u8 link_id,
                              struct wifi7_bw_context *peer);
void wifi7_bw_mlo_unregister_link(struct wifi7_bw_context *bw,
                                 u8 link_id);

/* Status and debug */
void wifi7_bw_dump_stats(struct wifi7_bw_context *bw);
void wifi7_bw_dump_quality(struct wifi7_bw_context *bw);

#endif /* __WIFI7_BANDWIDTH_H */ 