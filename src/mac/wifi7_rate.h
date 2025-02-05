/*
 * WiFi 7 Rate Control and Adaptation
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_RATE_H
#define __WIFI7_RATE_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include "../core/wifi7_core.h"

/* Rate control capabilities */
#define WIFI7_RATE_CAP_MCS_15        BIT(0)  /* Support for MCS 15 */
#define WIFI7_RATE_CAP_4K_QAM        BIT(1)  /* 4K-QAM support */
#define WIFI7_RATE_CAP_OFDMA         BIT(2)  /* OFDMA support */
#define WIFI7_RATE_CAP_MU_MIMO       BIT(3)  /* MU-MIMO support */
#define WIFI7_RATE_CAP_320MHZ        BIT(4)  /* 320MHz channels */
#define WIFI7_RATE_CAP_16_SS         BIT(5)  /* 16 spatial streams */
#define WIFI7_RATE_CAP_EHT           BIT(6)  /* EHT support */
#define WIFI7_RATE_CAP_MLO           BIT(7)  /* Multi-link operation */
#define WIFI7_RATE_CAP_DYNAMIC       BIT(8)  /* Dynamic adaptation */
#define WIFI7_RATE_CAP_PER_LINK      BIT(9)  /* Per-link rates */
#define WIFI7_RATE_CAP_ML_ADAPT      BIT(10) /* ML-based adaptation */
#define WIFI7_RATE_CAP_PREDICTION    BIT(11) /* Rate prediction */

/* Rate control algorithms */
#define WIFI7_RATE_ALGO_MINSTREL     0  /* Minstrel algorithm */
#define WIFI7_RATE_ALGO_PID          1  /* PID controller */
#define WIFI7_RATE_ALGO_ML           2  /* Machine learning */
#define WIFI7_RATE_ALGO_HYBRID       3  /* Hybrid approach */
#define WIFI7_RATE_ALGO_CUSTOM       4  /* Custom algorithm */

/* Rate parameters */
#define WIFI7_RATE_MAX_MCS           15  /* Maximum MCS index */
#define WIFI7_RATE_MAX_NSS           16  /* Maximum spatial streams */
#define WIFI7_RATE_MAX_BW            320 /* Maximum bandwidth */
#define WIFI7_RATE_MAX_GI            3   /* Maximum guard interval */
#define WIFI7_RATE_MAX_DCM           1   /* Maximum DCM index */
#define WIFI7_RATE_MAX_EHT_MCS       15  /* Maximum EHT MCS */
#define WIFI7_RATE_MAX_RETRY         10  /* Maximum retry count */

/* Rate flags */
#define WIFI7_RATE_FLAG_SGI          BIT(0)  /* Short guard interval */
#define WIFI7_RATE_FLAG_DCM          BIT(1)  /* Dual carrier modulation */
#define WIFI7_RATE_FLAG_STBC         BIT(2)  /* Space-time coding */
#define WIFI7_RATE_FLAG_LDPC         BIT(3)  /* LDPC coding */
#define WIFI7_RATE_FLAG_MU           BIT(4)  /* Multi-user */
#define WIFI7_RATE_FLAG_OFDMA        BIT(5)  /* OFDMA enabled */
#define WIFI7_RATE_FLAG_EHT          BIT(6)  /* EHT mode */
#define WIFI7_RATE_FLAG_MLO          BIT(7)  /* Multi-link */
#define WIFI7_RATE_FLAG_DYNAMIC      BIT(8)  /* Dynamic rate */
#define WIFI7_RATE_FLAG_FIXED        BIT(9)  /* Fixed rate */
#define WIFI7_RATE_FLAG_FALLBACK     BIT(10) /* Fallback rate */

/* Rate entry */
struct wifi7_rate_entry {
    u8 mcs;                /* MCS index */
    u8 nss;               /* Number of spatial streams */
    u8 bw;                /* Bandwidth */
    u8 gi;                /* Guard interval */
    u8 dcm;               /* DCM index */
    u32 flags;            /* Rate flags */
    u32 bitrate;          /* Bitrate in Mbps */
    u16 tries;            /* Retry count */
    u16 success;          /* Success count */
    u16 attempts;         /* Attempt count */
    u16 last_success;     /* Last success timestamp */
    u16 last_attempt;     /* Last attempt timestamp */
    u8 perfect_tx_time;   /* Perfect tx time */
    u8 max_tp_rate;       /* Max throughput rate */
    bool valid;           /* Valid entry */
};

/* Rate table */
struct wifi7_rate_table {
    struct wifi7_rate_entry entries[WIFI7_RATE_MAX_MCS + 1];
    u8 max_mcs;           /* Maximum MCS */
    u8 max_nss;           /* Maximum NSS */
    u8 max_bw;            /* Maximum bandwidth */
    u8 max_gi;            /* Maximum GI */
    u32 capabilities;     /* Table capabilities */
    bool eht_supported;   /* EHT support */
    bool mlo_supported;   /* MLO support */
};

/* Rate statistics */
struct wifi7_rate_stats {
    u32 tx_packets;       /* Transmitted packets */
    u32 tx_success;       /* Successful transmissions */
    u32 tx_failures;      /* Failed transmissions */
    u32 tx_retries;       /* Retry count */
    u32 perfect_tx_time;  /* Perfect tx time */
    u32 avg_tx_time;      /* Average tx time */
    u32 prob_ewma;        /* EWMA probability */
    u32 cur_prob;         /* Current probability */
    u32 cur_tp;           /* Current throughput */
    u32 max_tp;           /* Maximum throughput */
    u32 max_prob;         /* Maximum probability */
    u32 packet_count;     /* Packet count */
    u32 sample_count;     /* Sample count */
    u32 sample_skipped;   /* Skipped samples */
    ktime_t last_update;  /* Last update time */
};

/* Rate configuration */
struct wifi7_rate_config {
    u8 algorithm;         /* Rate algorithm */
    u32 capabilities;     /* Rate capabilities */
    u8 max_retry;        /* Maximum retries */
    u32 update_interval; /* Update interval */
    bool auto_adjust;    /* Auto adjustment */
    bool sample_mode;    /* Sampling enabled */
    bool ml_enabled;     /* ML enabled */
    struct {
        u8 min_mcs;      /* Minimum MCS */
        u8 max_mcs;      /* Maximum MCS */
        u8 min_nss;      /* Minimum NSS */
        u8 max_nss;      /* Maximum NSS */
        u8 min_bw;       /* Minimum bandwidth */
        u8 max_bw;       /* Maximum bandwidth */
    } limits;
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

int wifi7_rate_get_table(struct wifi7_dev *dev,
                        struct wifi7_rate_table *table);
int wifi7_rate_update_table(struct wifi7_dev *dev,
                           struct wifi7_rate_table *table);

int wifi7_rate_get_stats(struct wifi7_dev *dev,
                        struct wifi7_rate_stats *stats);
int wifi7_rate_clear_stats(struct wifi7_dev *dev);

int wifi7_rate_get_max_rate(struct wifi7_dev *dev,
                           struct wifi7_rate_entry *rate);
int wifi7_rate_get_min_rate(struct wifi7_dev *dev,
                           struct wifi7_rate_entry *rate);

int wifi7_rate_select(struct wifi7_dev *dev,
                     struct sk_buff *skb,
                     struct wifi7_rate_entry *rate);
int wifi7_rate_update(struct wifi7_dev *dev,
                     struct sk_buff *skb,
                     struct wifi7_rate_entry *rate,
                     bool success);

/* Debug interface */
#ifdef CONFIG_WIFI7_RATE_DEBUG
int wifi7_rate_debugfs_init(struct wifi7_dev *dev);
void wifi7_rate_debugfs_remove(struct wifi7_dev *dev);
#else
static inline int wifi7_rate_debugfs_init(struct wifi7_dev *dev) { return 0; }
static inline void wifi7_rate_debugfs_remove(struct wifi7_dev *dev) {}
#endif

#endif /* __WIFI7_RATE_H */ 