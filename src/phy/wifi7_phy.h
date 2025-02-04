/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_PHY_H
#define __WIFI7_PHY_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

/* TODO: Fine-tune these values based on real hardware testing */
#define PHY_MAX_BANDWIDTH_MHZ    320
#define PHY_MAX_NSS              16   /* Probably overkill, but future-proof */
#define PHY_MIN_SNR_4K_QAM       38   /* Initial value, needs optimization */
#define PHY_MAX_RU_TONES         996

/* FIXME: These need validation on actual hardware */
#define PHY_DEFAULT_TX_POWER     20   /* dBm */
#define PHY_AGC_SETTLING_TIME    20   /* microseconds */
#define PHY_MAX_DOPPLER_HZ       500  /* Conservative estimate */

/* 
 * TODO: Add proper channel state tracking
 * Current implementation is too simplistic for real-world use
 */
struct wifi7_phy_channel_state {
    u32 center_freq;
    u32 bandwidth;
    s8  noise_floor;
    u8  busy_time;
    u32 last_update;    /* jiffies */
    
    /* FIXME: This is a hack, need proper interference detection */
    bool interference_detected;
    u32  interferer_freq;
};

/*
 * OFDMA resource unit allocation
 * TODO: Add support for flexible RU patterns
 * Current implementation only supports basic patterns
 */
struct wifi7_phy_ru_alloc {
    u16 start_tone;
    u16 num_tones;
    u8  mcs;
    u8  nss;
    bool punctured;     /* For preamble puncturing */
    
    /* XXX: Power scaling needs work */
    s8  power_offset;   /* Relative to base power, dB */
};

/* 
 * 4K-QAM configuration
 * NOTE: This is experimental and needs extensive testing
 */
struct wifi7_phy_4k_qam {
    bool enabled;
    u8   min_snr;      /* Minimum required SNR */
    u8   max_retries;  /* Max retries before fallback */
    u32  error_count;  /* Tracking for rate control */
    
    /* TODO: Add proper constellation mapping */
    void *constellation_map;  /* Placeholder */
};

/* Main PHY device structure */
struct wifi7_phy_dev {
    struct device *dev;
    void *hw_priv;     /* Hardware-specific private data */
    
    /* Basic configuration */
    u32 supported_bands;
    u32 current_band;
    u32 max_bandwidth;
    u8  max_nss;
    bool he_enabled;    /* 802.11ax/WiFi 6 capabilities */
    bool eht_enabled;   /* 802.11be/WiFi 7 capabilities */
    
    /* Channel state */
    struct wifi7_phy_channel_state channel_state;
    spinlock_t state_lock;  /* Protects channel state */
    
    /* OFDMA configuration */
    struct wifi7_phy_ru_alloc *ru_alloc;
    u32 num_ru_alloc;
    spinlock_t ru_lock;
    
    /* 4K-QAM state */
    struct wifi7_phy_4k_qam qam_state;
    atomic_t qam_active;    /* Currently using 4K-QAM */
    
    /* Workqueues for async operations */
    struct workqueue_struct *calib_wq;  /* Calibration */
    struct workqueue_struct *dfs_wq;    /* Dynamic frequency selection */
    
    /* TODO: Add proper power tracking */
    struct {
        s8 current_power;
        s8 max_power;
        u32 temperature;    /* in millicelsius */
    } power_tracking;
    
    /* Debug/Statistics */
    struct {
        u32 qam_switches;   /* 4K-QAM mode switches */
        u32 ru_changes;     /* RU allocation changes */
        u32 temp_warnings;  /* Temperature warnings */
        u64 total_symbols;  /* Total symbols processed */
    } stats;
};

/* Function prototypes */
struct wifi7_phy_dev *wifi7_phy_alloc(struct device *dev);
void wifi7_phy_free(struct wifi7_phy_dev *phy);

/* Basic operations */
int wifi7_phy_init(struct wifi7_phy_dev *phy);
void wifi7_phy_deinit(struct wifi7_phy_dev *phy);
int wifi7_phy_start(struct wifi7_phy_dev *phy);
void wifi7_phy_stop(struct wifi7_phy_dev *phy);

/* Channel operations */
int wifi7_phy_set_channel(struct wifi7_phy_dev *phy, u32 freq, u32 bandwidth);
int wifi7_phy_get_channel_state(struct wifi7_phy_dev *phy,
                               struct wifi7_phy_channel_state *state);

/* OFDMA operations */
int wifi7_phy_alloc_ru(struct wifi7_phy_dev *phy,
                      struct wifi7_phy_ru_alloc *alloc,
                      u32 num_alloc);
void wifi7_phy_free_ru(struct wifi7_phy_dev *phy);

/* 4K-QAM operations */
int wifi7_phy_enable_4k_qam(struct wifi7_phy_dev *phy, bool enable);
int wifi7_phy_get_qam_stats(struct wifi7_phy_dev *phy,
                           struct wifi7_phy_4k_qam *stats);

/*
 * TODO: Add these interfaces
 * - Beamforming configuration
 * - MU-MIMO setup
 * - Dynamic bandwidth adjustment
 * - Interference mitigation
 * - Power calibration
 */

#endif /* __WIFI7_PHY_H */ 