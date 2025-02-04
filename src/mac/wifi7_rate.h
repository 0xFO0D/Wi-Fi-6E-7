/*
 * WiFi 7 Rate Control
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_RATE_H
#define __WIFI7_RATE_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include "../core/wifi7_core.h"

/* Rate control parameters */
#define WIFI7_RATE_MAX_MCS      11  /* MCS0-11 */
#define WIFI7_RATE_MAX_NSS      16  /* Up to 16 spatial streams */
#define WIFI7_RATE_MAX_BW       4   /* 20/40/80/160/320 MHz */
#define WIFI7_RATE_MAX_GI       3   /* 0.8/1.6/3.2 us */
#define WIFI7_RATE_MAX_DCM      1   /* Dual carrier modulation */
#define WIFI7_RATE_MAX_EHT_MU   7   /* EHT MU MIMO modes */
#define WIFI7_RATE_MAX_RU       8   /* Resource unit sizes */
#define WIFI7_RATE_MAX_RETRIES  7   /* Maximum retry count */

/* Rate flags */
#define WIFI7_RATE_FLAG_SGI     BIT(0)  /* Short guard interval */
#define WIFI7_RATE_FLAG_LGI     BIT(1)  /* Long guard interval */
#define WIFI7_RATE_FLAG_DCM     BIT(2)  /* Dual carrier modulation */
#define WIFI7_RATE_FLAG_STBC    BIT(3)  /* Space-time block coding */
#define WIFI7_RATE_FLAG_LDPC    BIT(4)  /* Low density parity check */
#define WIFI7_RATE_FLAG_BF      BIT(5)  /* Beamforming */
#define WIFI7_RATE_FLAG_MU      BIT(6)  /* Multi-user */
#define WIFI7_RATE_FLAG_OFDMA   BIT(7)  /* OFDMA */
#define WIFI7_RATE_FLAG_PUNCT   BIT(8)  /* Preamble puncturing */
#define WIFI7_RATE_FLAG_EHT     BIT(9)  /* EHT format */
#define WIFI7_RATE_FLAG_MLO     BIT(10) /* Multi-link operation */

/* Rate control algorithms */
#define WIFI7_RATE_ALGO_MINSTREL   0  /* Minstrel rate control */
#define WIFI7_RATE_ALGO_PID        1  /* PID rate control */
#define WIFI7_RATE_ALGO_ML         2  /* Machine learning based */
#define WIFI7_RATE_ALGO_CUSTOM     3  /* Custom algorithm */

/* Rate table entry */
struct wifi7_rate_info {
    u8 mcs;                    /* Modulation and coding scheme */
    u8 nss;                    /* Number of spatial streams */
    u8 bw;                     /* Bandwidth */
    u8 gi;                     /* Guard interval */
    u32 flags;                /* Rate flags */
    u32 bitrate;              /* Bitrate in 100 kbps */
    u16 duration;             /* Frame duration in us */
    u8 retry_count;           /* Retry count */
    u8 power_level;           /* TX power level */
};

/* Rate statistics */
struct wifi7_rate_stats {
    u32 attempts;             /* Transmission attempts */
    u32 success;              /* Successful transmissions */
    u32 failures;             /* Failed transmissions */
    u32 retries;              /* Retry count */
    u32 avg_rssi;             /* Average RSSI */
    u32 avg_snr;              /* Average SNR */
    u32 avg_evm;              /* Average EVM */
    u32 perfect;              /* Perfect transmissions */
    u32 imperfect;            /* Imperfect transmissions */
    u32 ampdu_len;            /* Average A-MPDU length */
    u32 throughput;           /* Achieved throughput */
    u32 airtime;              /* Airtime consumption */
    ktime_t last_update;      /* Last update timestamp */
};

/* Rate control state */
struct wifi7_rate_table {
    u8 mcs_mask[WIFI7_RATE_MAX_MCS];   /* MCS mask */
    u8 nss_mask[WIFI7_RATE_MAX_NSS];   /* NSS mask */
    u8 bw_mask[WIFI7_RATE_MAX_BW];     /* Bandwidth mask */
    u8 gi_mask[WIFI7_RATE_MAX_GI];     /* Guard interval mask */
    u32 flags_mask;                    /* Rate flags mask */
    struct wifi7_rate_info rates[WIFI7_RATE_MAX_MCS *
                               WIFI7_RATE_MAX_NSS *
                               WIFI7_RATE_MAX_BW *
                               WIFI7_RATE_MAX_GI];
    u16 n_rates;                       /* Number of rates */
    u16 max_rate;                      /* Maximum rate index */
    u16 min_rate;                      /* Minimum rate index */
    u16 probe_rate;                    /* Probing rate index */
    u16 fallback_rate;                 /* Fallback rate index */
    struct wifi7_rate_stats stats[WIFI7_RATE_MAX_MCS *
                                WIFI7_RATE_MAX_NSS *
                                WIFI7_RATE_MAX_BW *
                                WIFI7_RATE_MAX_GI];
};

/* Rate control configuration */
struct wifi7_rate_config {
    u8 algorithm;             /* Rate control algorithm */
    u8 max_retry;            /* Maximum retry count */
    u16 update_interval;     /* Update interval in ms */
    u16 probe_interval;      /* Probe interval in ms */
    u16 ewma_level;         /* EWMA smoothing level */
    u16 success_threshold;   /* Success threshold */
    u16 tx_power_max;       /* Maximum TX power */
    u16 tx_power_min;       /* Minimum TX power */
    bool ampdu_enabled;      /* A-MPDU enabled */
    bool amsdu_enabled;      /* A-MSDU enabled */
    bool mu_enabled;         /* MU-MIMO enabled */
    bool ofdma_enabled;      /* OFDMA enabled */
    bool mlo_enabled;        /* MLO enabled */
};

/* Rate control device info */
struct wifi7_rate {
    /* Configuration */
    struct wifi7_rate_config config;
    
    /* Rate tables */
    struct wifi7_rate_table *tables;
    u8 n_tables;
    
    /* Current state */
    u16 current_rate;
    u16 last_rate;
    u32 update_count;
    ktime_t last_update;
    
    /* Statistics */
    struct {
        u32 rate_changes;
        u32 probes;
        u32 fallbacks;
        u32 resets;
        u32 max_tp;
        u32 avg_tp;
        u32 min_tp;
    } stats;
    
    /* Locks */
    spinlock_t lock;
    
    /* Work items */
    struct delayed_work update_work;
    struct delayed_work probe_work;
    
    /* Debugging */
    struct dentry *debugfs_dir;
    bool debug_enabled;
};

/* Function prototypes */
int wifi7_rate_init(struct wifi7_dev *dev);
void wifi7_rate_deinit(struct wifi7_dev *dev);

int wifi7_rate_start(struct wifi7_dev *dev);
void wifi7_rate_stop(struct wifi7_dev *dev);

int wifi7_rate_set_config(struct wifi7_dev *dev,
                         struct wifi7_rate_config *config);
int wifi7_rate_get_config(struct wifi7_dev *dev,
                         struct wifi7_rate_config *config);

int wifi7_rate_update(struct wifi7_dev *dev,
                     struct sk_buff *skb,
                     bool success, u8 retries);
                     
struct wifi7_rate_info *wifi7_rate_get_next(struct wifi7_dev *dev,
                                          struct sk_buff *skb);
                                          
int wifi7_rate_get_stats(struct wifi7_dev *dev,
                        struct wifi7_rate_stats *stats);
int wifi7_rate_clear_stats(struct wifi7_dev *dev);

#endif /* __WIFI7_RATE_H */ 