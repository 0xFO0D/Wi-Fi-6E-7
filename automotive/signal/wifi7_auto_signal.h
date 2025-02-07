/*
 * WiFi 7 Automotive Signal Management
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_AUTO_SIGNAL_H
#define __WIFI7_AUTO_SIGNAL_H

#include <linux/types.h>
#include "../core/wifi7_core.h"

/* Signal Environment Types */
#define WIFI7_ENV_OPEN           0  /* Open environment */
#define WIFI7_ENV_URBAN          1  /* Urban environment */
#define WIFI7_ENV_TUNNEL         2  /* Tunnel/underground */
#define WIFI7_ENV_PARKING        3  /* Parking structure */
#define WIFI7_ENV_INDUSTRIAL     4  /* Industrial area */

/* Interference Sources */
#define WIFI7_INTERFERENCE_EMI   BIT(0)  /* Electromagnetic interference */
#define WIFI7_INTERFERENCE_METAL BIT(1)  /* Metal structures */
#define WIFI7_INTERFERENCE_ELECT BIT(2)  /* Electronic devices */
#define WIFI7_INTERFERENCE_WIFI  BIT(3)  /* Other WiFi networks */
#define WIFI7_INTERFERENCE_BT    BIT(4)  /* Bluetooth devices */

/* Signal Configuration */
struct wifi7_auto_signal_config {
    u8 environment;          /* Environment type */
    u32 interference_mask;   /* Active interference sources */
    bool adaptive_power;     /* Adaptive power control */
    bool beam_forming;       /* Beam forming enabled */
    bool mimo_optimize;      /* MIMO optimization */
    u32 min_rssi;           /* Minimum acceptable RSSI */
    u32 max_retry;          /* Maximum retry attempts */
    struct {
        u32 scan_interval;   /* Environment scanning interval */
        u32 adapt_interval;  /* Adaptation interval */
        u32 report_interval; /* Status reporting interval */
    } intervals;
    struct {
        s8 power_min;       /* Minimum TX power (dBm) */
        s8 power_max;       /* Maximum TX power (dBm) */
        u8 power_step;      /* Power adjustment step */
        u8 spatial_streams; /* Number of spatial streams */
    } radio;
};

/* Signal Quality Metrics */
struct wifi7_signal_metrics {
    s32 rssi;               /* Current RSSI value */
    u32 snr;                /* Signal-to-noise ratio */
    u32 noise_floor;        /* Noise floor level */
    u32 interference_level; /* Interference level */
    u32 retry_count;        /* Retry counter */
    u32 error_rate;         /* Error rate */
    s8 tx_power;           /* Current TX power */
    u8 mcs_index;          /* Current MCS index */
    u8 spatial_streams;    /* Active spatial streams */
    bool link_stable;      /* Link stability indicator */
};

/* Signal Statistics */
struct wifi7_auto_signal_stats {
    u32 adaptations;        /* Number of adaptations */
    u32 power_changes;      /* Power level changes */
    u32 beam_switches;      /* Beam switching events */
    u32 recovery_events;    /* Signal recovery events */
    struct {
        u32 rssi_drops;     /* RSSI drop events */
        u32 snr_drops;      /* SNR drop events */
        u32 noise_spikes;   /* Noise level spikes */
        u32 retry_spikes;   /* Retry count spikes */
    } events;
    struct {
        s32 rssi_min;       /* Minimum RSSI observed */
        s32 rssi_max;       /* Maximum RSSI observed */
        u32 snr_min;        /* Minimum SNR observed */
        u32 snr_max;        /* Maximum SNR observed */
    } ranges;
};

/* Function prototypes */
int wifi7_auto_signal_init(struct wifi7_dev *dev);
void wifi7_auto_signal_deinit(struct wifi7_dev *dev);

int wifi7_auto_signal_start(struct wifi7_dev *dev);
void wifi7_auto_signal_stop(struct wifi7_dev *dev);

int wifi7_auto_signal_set_config(struct wifi7_dev *dev,
                                struct wifi7_auto_signal_config *config);
int wifi7_auto_signal_get_config(struct wifi7_dev *dev,
                                struct wifi7_auto_signal_config *config);

int wifi7_auto_signal_get_metrics(struct wifi7_dev *dev,
                                 struct wifi7_signal_metrics *metrics);
int wifi7_auto_signal_get_stats(struct wifi7_dev *dev,
                               struct wifi7_auto_signal_stats *stats);

int wifi7_auto_signal_force_adapt(struct wifi7_dev *dev);
int wifi7_auto_signal_reset_stats(struct wifi7_dev *dev);

/* Debug interface */
#ifdef CONFIG_WIFI7_AUTO_SIGNAL_DEBUG
int wifi7_auto_signal_debugfs_init(struct wifi7_dev *dev);
void wifi7_auto_signal_debugfs_remove(struct wifi7_dev *dev);
#else
static inline int wifi7_auto_signal_debugfs_init(struct wifi7_dev *dev) { return 0; }
static inline void wifi7_auto_signal_debugfs_remove(struct wifi7_dev *dev) {}
#endif

#endif /* __WIFI7_AUTO_SIGNAL_H */ 