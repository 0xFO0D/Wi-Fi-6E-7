/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_RATE_H
#define __WIFI7_RATE_H

#include <linux/types.h>
#include <linux/ieee80211.h>
#include "wifi7_phy.h"

/* Rate adaptation parameters */
#define WIFI7_RATE_MAX_RETRY       3
#define WIFI7_RATE_SCALE_INTERVAL  100  /* ms */
#define WIFI7_RATE_PROBE_INTERVAL  1000 /* ms */
#define WIFI7_RATE_HISTORY_SIZE    10

/* PER thresholds */
#define WIFI7_RATE_PER_THRESHOLD_LOW   10  /* 10% */
#define WIFI7_RATE_PER_THRESHOLD_HIGH  40  /* 40% */

/* SNR thresholds for MCS selection */
#define WIFI7_RATE_SNR_4KQAM      38  /* dB */
#define WIFI7_RATE_SNR_1KQAM      35
#define WIFI7_RATE_SNR_256QAM     32
#define WIFI7_RATE_SNR_64QAM      28
#define WIFI7_RATE_SNR_16QAM      24
#define WIFI7_RATE_SNR_QPSK       20
#define WIFI7_RATE_SNR_BPSK       15

/* Rate states */
enum wifi7_rate_state {
    WIFI7_RATE_STATE_INIT = 0,
    WIFI7_RATE_STATE_STABLE,
    WIFI7_RATE_STATE_PROBING,
    WIFI7_RATE_STATE_BACKOFF,
    WIFI7_RATE_STATE_RECOVERY
};

/* Rate statistics */
struct wifi7_rate_stats {
    u32 attempts;           /* Transmission attempts */
    u32 successes;         /* Successful transmissions */
    u32 retries;           /* Retry count */
    u32 failures;          /* Failed transmissions */
    u32 throughput;        /* Current throughput (Mbps) */
    u8 per;                /* Packet error rate (%) */
    ktime_t last_update;   /* Last statistics update */
};

/* Rate table entry */
struct wifi7_rate_entry {
    u8 mcs;                /* Modulation and coding scheme */
    u8 nss;                /* Number of spatial streams */
    u8 bw;                 /* Bandwidth */
    u8 gi;                 /* Guard interval */
    u8 dcm;                /* Dual carrier modulation */
    bool ldpc;             /* LDPC coding */
    u32 rate_kbps;         /* PHY rate in Kbps */
    struct wifi7_rate_stats stats;
};

/* Rate table */
struct wifi7_rate_table {
    u8 num_entries;
    u8 current_index;      /* Current rate index */
    u8 probe_index;        /* Probing rate index */
    u8 lowest_index;       /* Lowest allowed rate index */
    u8 highest_index;      /* Highest allowed rate index */
    struct wifi7_rate_entry entries[32];  /* Rate entries */
};

/* Rate control algorithm */
struct wifi7_rate_algorithm {
    const char *name;
    
    /* Algorithm operations */
    int (*init)(struct wifi7_rate_context *rc);
    void (*deinit)(struct wifi7_rate_context *rc);
    int (*tx_status)(struct wifi7_rate_context *rc,
                     struct sk_buff *skb,
                     bool success);
    struct wifi7_rate_entry *(*get_next_rate)(struct wifi7_rate_context *rc);
    void (*update_stats)(struct wifi7_rate_context *rc);
};

/* Rate control context */
struct wifi7_rate_context {
    struct wifi7_phy_dev *phy;
    
    /* Current state */
    enum wifi7_rate_state state;
    struct wifi7_rate_table rate_table;
    const struct wifi7_rate_algorithm *algorithm;
    
    /* Link quality */
    s8 rssi;               /* Current RSSI */
    u8 snr;                /* Current SNR */
    u8 interference;       /* Interference level */
    
    /* Operation control */
    spinlock_t lock;
    struct workqueue_struct *rate_wq;
    struct delayed_work rate_work;
    
    /* History tracking */
    struct {
        u8 mcs_history[WIFI7_RATE_HISTORY_SIZE];
        u8 per_history[WIFI7_RATE_HISTORY_SIZE];
        u8 history_index;
        u32 total_packets;
        u32 total_retries;
        ktime_t last_probe;
    } history;
    
    /* Statistics */
    struct {
        u32 rate_changes;
        u32 probe_attempts;
        u32 probe_successes;
        u32 fallbacks;
        u32 recoveries;
        ktime_t last_change;
    } stats;
};

/* Function prototypes */
struct wifi7_rate_context *wifi7_rate_alloc(struct wifi7_phy_dev *phy);
void wifi7_rate_free(struct wifi7_rate_context *rc);

/* Rate control */
int wifi7_rate_init_table(struct wifi7_rate_context *rc);
int wifi7_rate_set_algorithm(struct wifi7_rate_context *rc,
                            const char *name);
struct wifi7_rate_entry *wifi7_rate_get_next(struct wifi7_rate_context *rc);

/* Status updates */
void wifi7_rate_tx_status(struct wifi7_rate_context *rc,
                         struct sk_buff *skb,
                         bool success);
int wifi7_rate_update_link(struct wifi7_rate_context *rc,
                          s8 rssi, u8 snr,
                          u8 interference);

/* Rate probing */
int wifi7_rate_start_probe(struct wifi7_rate_context *rc);
void wifi7_rate_stop_probe(struct wifi7_rate_context *rc);

/* Statistics and debug */
void wifi7_rate_dump_stats(struct wifi7_rate_context *rc);
void wifi7_rate_dump_table(struct wifi7_rate_context *rc);

#endif /* __WIFI7_RATE_H */ 