/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_MAC_H
#define __WIFI7_MAC_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/completion.h>

/* WiFi 7 MAC capabilities */
#define WIFI7_MAC_CAP_4K_MPDU          BIT(0)
#define WIFI7_MAC_CAP_MLO              BIT(1)
#define WIFI7_MAC_CAP_MULTI_TID        BIT(2)
#define WIFI7_MAC_CAP_EHT_TRIGGER      BIT(3)
#define WIFI7_MAC_CAP_320MHZ           BIT(4)
#define WIFI7_MAC_CAP_4K_QAM           BIT(5)

/* Maximum values for WiFi 7 */
#define WIFI7_MAX_LINKS                8
#define WIFI7_MAX_TID                  8
#define WIFI7_MAX_QUEUES              16
#define WIFI7_MAX_MPDU_LEN         4096
#define WIFI7_MAX_AMPDU_LEN      262144
#define WIFI7_MAX_RU_ALLOC          996

/* Multi-link operation states */
enum wifi7_mlo_state {
    MLO_STATE_DISABLED = 0,
    MLO_STATE_SETUP,
    MLO_STATE_ACTIVE,
    MLO_STATE_TEARDOWN,
    MLO_STATE_ERROR
};

/* Link quality metrics */
struct wifi7_link_metrics {
    u32 rssi;                  /* Signal strength in dBm */
    u32 snr;                   /* Signal-to-noise ratio in dB */
    u32 tx_rate;              /* Current TX rate in Mbps */
    u32 rx_rate;              /* Current RX rate in Mbps */
    u32 tx_retries;           /* Number of retransmissions */
    u32 rx_errors;            /* Number of receive errors */
    u32 busy_time;            /* Channel busy time % */
    u32 throughput;           /* Current throughput in Mbps */
};

/* Per-link state information */
struct wifi7_link_state {
    u8 link_id;
    bool enabled;
    enum wifi7_mlo_state mlo_state;
    struct wifi7_link_metrics metrics;
    spinlock_t lock;
    
    /* Hardware parameters */
    u32 freq;                 /* Operating frequency */
    u32 bandwidth;            /* Channel bandwidth */
    u8 nss;                   /* Number of spatial streams */
    u8 mcs;                   /* Modulation and coding scheme */
    bool ldpc;                /* LDPC coding enabled */
    bool stbc;                /* STBC enabled */
    
    /* Power management */
    bool power_save;
    u32 sleep_duration;
    u32 awake_duration;
    
    /* Statistics */
    u64 tx_bytes;
    u64 rx_bytes;
    u64 tx_packets;
    u64 rx_packets;
    u64 tx_errors;
    u64 rx_errors;
};

/* Multi-TID aggregation configuration */
struct wifi7_multi_tid_config {
    bool enabled;
    u8 max_tids;
    u16 max_ampdu_len;
    u8 tid_bitmap;
    u32 timeout_ms;
};

/* WiFi 7 MAC device structure */
struct wifi7_mac_dev {
    struct device *dev;
    u32 capabilities;
    
    /* Multi-link operation */
    struct wifi7_link_state links[WIFI7_MAX_LINKS];
    atomic_t active_links;
    struct workqueue_struct *mlo_wq;
    
    /* Frame aggregation */
    struct wifi7_multi_tid_config multi_tid;
    atomic_t ampdu_len;
    spinlock_t aggr_lock;
    
    /* Power management */
    struct workqueue_struct *pm_wq;
    atomic_t power_state;
    
    /* QoS and scheduling */
    struct sk_buff_head queues[WIFI7_MAX_QUEUES];
    spinlock_t queue_locks[WIFI7_MAX_QUEUES];
    u32 queue_params[WIFI7_MAX_QUEUES];
    
    /* Hardware interface */
    void *hw_priv;
    struct wifi7_mac_ops *ops;
    
    /* Debug and statistics */
    struct dentry *debugfs_dir;
    struct wifi7_mac_stats stats;
};

/* Hardware operation callbacks */
struct wifi7_mac_ops {
    int (*init)(struct wifi7_mac_dev *dev);
    void (*deinit)(struct wifi7_mac_dev *dev);
    
    /* Link management */
    int (*link_setup)(struct wifi7_mac_dev *dev, u8 link_id);
    int (*link_teardown)(struct wifi7_mac_dev *dev, u8 link_id);
    
    /* Frame transmission */
    int (*tx_frame)(struct wifi7_mac_dev *dev, struct sk_buff *skb, u8 link_id);
    void (*tx_done)(struct wifi7_mac_dev *dev, struct sk_buff *skb, bool success);
    
    /* Frame reception */
    int (*rx_frame)(struct wifi7_mac_dev *dev, struct sk_buff *skb, u8 link_id);
    
    /* Power management */
    int (*set_power_state)(struct wifi7_mac_dev *dev, u8 link_id, bool power_save);
    
    /* Hardware configuration */
    int (*set_hw_params)(struct wifi7_mac_dev *dev, u8 link_id,
                        u32 freq, u32 bw, u8 nss, u8 mcs);
};

/* Function prototypes */
struct wifi7_mac_dev *wifi7_mac_alloc(struct device *dev);
void wifi7_mac_free(struct wifi7_mac_dev *dev);
int wifi7_mac_register(struct wifi7_mac_dev *dev);
void wifi7_mac_unregister(struct wifi7_mac_dev *dev);

int wifi7_mac_link_setup(struct wifi7_mac_dev *dev, u8 link_id);
int wifi7_mac_link_teardown(struct wifi7_mac_dev *dev, u8 link_id);
int wifi7_mac_set_link_params(struct wifi7_mac_dev *dev, u8 link_id,
                            u32 freq, u32 bw, u8 nss, u8 mcs);

int wifi7_mac_tx_frame(struct wifi7_mac_dev *dev, struct sk_buff *skb, u8 link_id);
int wifi7_mac_rx_frame(struct wifi7_mac_dev *dev, struct sk_buff *skb, u8 link_id);

int wifi7_mac_set_power_save(struct wifi7_mac_dev *dev, u8 link_id, bool enable);
int wifi7_mac_set_multi_tid(struct wifi7_mac_dev *dev, struct wifi7_multi_tid_config *config);

/* Statistics and debug */
void wifi7_mac_get_stats(struct wifi7_mac_dev *dev, struct wifi7_mac_stats *stats);
int wifi7_mac_debugfs_init(struct wifi7_mac_dev *dev);
void wifi7_mac_debugfs_remove(struct wifi7_mac_dev *dev);

#endif /* __WIFI7_MAC_H */ 