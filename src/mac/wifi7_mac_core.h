/*
 * WiFi 7 MAC Layer Core Implementation
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 *
 * Core MAC layer functionality for WiFi 7 including:
 * - Frame handling and management
 * - Queue management and scheduling
 * - Power management
 * - Security features
 * - Statistics and monitoring
 */

#ifndef __WIFI7_MAC_CORE_H
#define __WIFI7_MAC_CORE_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/etherdevice.h>
#include "../core/wifi7_core.h"

/* MAC capabilities */
#define WIFI7_MAC_CAP_320MHZ          BIT(0)  /* 320 MHz channels */
#define WIFI7_MAC_CAP_4K_QAM          BIT(1)  /* 4K-QAM modulation */
#define WIFI7_MAC_CAP_16_SS           BIT(2)  /* 16 spatial streams */
#define WIFI7_MAC_CAP_MLO             BIT(3)  /* Multi-link operation */
#define WIFI7_MAC_CAP_EHT_MU          BIT(4)  /* EHT MU-MIMO */
#define WIFI7_MAC_CAP_UL_OFDMA        BIT(5)  /* UL OFDMA */
#define WIFI7_MAC_CAP_DL_OFDMA        BIT(6)  /* DL OFDMA */
#define WIFI7_MAC_CAP_TWT             BIT(7)  /* Target wake time */
#define WIFI7_MAC_CAP_BSS_COLOR       BIT(8)  /* BSS coloring */
#define WIFI7_MAC_CAP_SR              BIT(9)  /* Spatial reuse */
#define WIFI7_MAC_CAP_MBSSID          BIT(10) /* Multiple BSSID */
#define WIFI7_MAC_CAP_MU_RTS          BIT(11) /* MU-RTS */
#define WIFI7_MAC_CAP_PSR             BIT(12) /* Preamble puncturing */
#define WIFI7_MAC_CAP_4K_BA           BIT(13) /* 4K block ack */
#define WIFI7_MAC_CAP_MULTI_TID       BIT(14) /* Multi-TID aggregation */
#define WIFI7_MAC_CAP_EMLSR           BIT(15) /* Enhanced MLO single radio */
#define WIFI7_MAC_CAP_EMLMR           BIT(16) /* Enhanced MLO multi radio */

/* MAC states */
#define WIFI7_MAC_STATE_STOPPED       0
#define WIFI7_MAC_STATE_STARTING      1
#define WIFI7_MAC_STATE_RUNNING       2
#define WIFI7_MAC_STATE_STOPPING      3
#define WIFI7_MAC_STATE_SUSPENDED     4
#define WIFI7_MAC_STATE_ERROR         5

/* Queue parameters */
#define WIFI7_MAC_MAX_QUEUES         16
#define WIFI7_MAC_MAX_QUEUE_LEN     256
#define WIFI7_MAC_MIN_QUEUE_LEN      32
#define WIFI7_MAC_MAX_AMPDU_LEN    4096
#define WIFI7_MAC_MAX_AMSDU_LEN    4096
#define WIFI7_MAC_MAX_RETRY          16

/* Frame types */
#define WIFI7_MAC_FRAME_MGMT         0
#define WIFI7_MAC_FRAME_CTRL         1
#define WIFI7_MAC_FRAME_DATA         2
#define WIFI7_MAC_FRAME_EXT          3

/* Frame subtypes */
#define WIFI7_MAC_FRAME_BEACON       0
#define WIFI7_MAC_FRAME_PROBE_REQ    1
#define WIFI7_MAC_FRAME_PROBE_RESP   2
#define WIFI7_MAC_FRAME_AUTH         3
#define WIFI7_MAC_FRAME_DEAUTH       4
#define WIFI7_MAC_FRAME_ASSOC_REQ    5
#define WIFI7_MAC_FRAME_ASSOC_RESP   6
#define WIFI7_MAC_FRAME_DISASSOC     7
#define WIFI7_MAC_FRAME_ACTION       8

/* Security modes */
#define WIFI7_MAC_SEC_NONE           0
#define WIFI7_MAC_SEC_WEP            1
#define WIFI7_MAC_SEC_TKIP           2
#define WIFI7_MAC_SEC_CCMP           3
#define WIFI7_MAC_SEC_GCMP           4
#define WIFI7_MAC_SEC_GCMP_256       5
#define WIFI7_MAC_SEC_CCMP_256       6
#define WIFI7_MAC_SEC_BIP            7
#define WIFI7_MAC_SEC_BIP_GMAC_128   8
#define WIFI7_MAC_SEC_BIP_GMAC_256   9
#define WIFI7_MAC_SEC_BIP_CMAC_256   10

/* Power management modes */
#define WIFI7_MAC_PM_DISABLED        0
#define WIFI7_MAC_PM_PSM             1
#define WIFI7_MAC_PM_UAPSD          2
#define WIFI7_MAC_PM_WMM_PS         3
#define WIFI7_MAC_PM_TWT            4
#define WIFI7_MAC_PM_ADAPTIVE       5

/* Frame header */
struct wifi7_mac_frame_hdr {
    __le16 frame_control;
    __le16 duration_id;
    u8 addr1[ETH_ALEN];
    u8 addr2[ETH_ALEN];
    u8 addr3[ETH_ALEN];
    __le16 seq_ctrl;
    u8 addr4[ETH_ALEN];
    __le16 qos_ctrl;
    __le32 ht_ctrl;
} __packed;

/* Queue entry */
struct wifi7_mac_queue_entry {
    struct sk_buff *skb;
    u16 seq_num;
    u8 tid;
    u8 retry_count;
    u32 flags;
    ktime_t timestamp;
    bool aggregated;
    bool encrypted;
    u16 lifetime;
};

/* Queue descriptor */
struct wifi7_mac_queue {
    u8 queue_id;
    u8 tid;
    u16 max_len;
    u16 len;
    u32 flags;
    spinlock_t lock;
    struct sk_buff_head skb_queue;
    struct wifi7_mac_queue_entry entries[WIFI7_MAC_MAX_QUEUE_LEN];
    
    /* Statistics */
    u32 enqueued;
    u32 dequeued;
    u32 dropped;
    u32 rejected;
    u32 requeued;
    u32 flushed;
    
    /* Rate control */
    u8 ac;
    u16 txop_limit;
    u8 aifs;
    u16 cw_min;
    u16 cw_max;
    
    /* Aggregation */
    bool ampdu_enabled;
    bool amsdu_enabled;
    u16 ampdu_len;
    u16 amsdu_len;
    u8 agg_timeout;
    u8 agg_retry_limit;
};

/* Security context */
struct wifi7_mac_security {
    u8 mode;
    u8 key_idx;
    u8 key_len;
    u8 key[32];
    u8 rx_pn[16];
    u8 tx_pn[16];
    bool hw_crypto;
    spinlock_t lock;
};

/* Power management context */
struct wifi7_mac_power {
    u8 mode;
    bool enabled;
    u32 listen_interval;
    u32 beacon_interval;
    u32 dtim_period;
    u32 ps_poll_interval;
    bool ps_enabled;
    bool uapsd_enabled;
    u8 uapsd_queues;
    u32 sleep_duration;
    u32 awake_duration;
    ktime_t last_activity;
    struct delayed_work ps_work;
    spinlock_t lock;
};

/* Statistics */
struct wifi7_mac_stats {
    /* Frame counts */
    u32 tx_frames;
    u32 rx_frames;
    u32 tx_bytes;
    u32 rx_bytes;
    u32 tx_errors;
    u32 rx_errors;
    
    /* Queue stats */
    u32 queue_full;
    u32 queue_empty;
    u32 queue_drops;
    u32 queue_rejects;
    
    /* Aggregation stats */
    u32 ampdu_tx;
    u32 ampdu_rx;
    u32 amsdu_tx;
    u32 amsdu_rx;
    u32 ba_tx;
    u32 ba_rx;
    
    /* Error stats */
    u32 fcs_errors;
    u32 decrypt_errors;
    u32 mic_errors;
    u32 key_errors;
    u32 retry_errors;
    u32 lifetime_errors;
    
    /* Power stats */
    u32 ps_enters;
    u32 ps_exits;
    u32 ps_wake_ups;
    u32 ps_timeouts;
    
    /* Timing stats */
    u32 channel_time;
    u32 tx_time;
    u32 rx_time;
    u32 idle_time;
    u32 busy_time;
};

/* MAC device context */
struct wifi7_mac {
    /* Device info */
    struct wifi7_dev *dev;
    u32 capabilities;
    u8 state;
    bool enabled;
    
    /* Frame handling */
    struct {
        spinlock_t lock;
        struct sk_buff_head tx_queue;
        struct sk_buff_head rx_queue;
        struct delayed_work tx_work;
        struct delayed_work rx_work;
    } frames;
    
    /* Queue management */
    struct {
        struct wifi7_mac_queue queues[WIFI7_MAC_MAX_QUEUES];
        u8 num_queues;
        spinlock_t lock;
    } queues;
    
    /* Security */
    struct wifi7_mac_security security;
    
    /* Power management */
    struct wifi7_mac_power power;
    
    /* Statistics */
    struct wifi7_mac_stats stats;
    
    /* Work items */
    struct workqueue_struct *wq;
    struct delayed_work housekeeping_work;
    
    /* Debugging */
    struct dentry *debugfs_dir;
    bool debug_enabled;
};

/* Function prototypes */
int wifi7_mac_init(struct wifi7_dev *dev);
void wifi7_mac_deinit(struct wifi7_dev *dev);

int wifi7_mac_start(struct wifi7_dev *dev);
void wifi7_mac_stop(struct wifi7_dev *dev);

int wifi7_mac_tx(struct wifi7_dev *dev, struct sk_buff *skb);
int wifi7_mac_rx(struct wifi7_dev *dev, struct sk_buff *skb);

int wifi7_mac_queue_init(struct wifi7_dev *dev);
int wifi7_mac_queue_deinit(struct wifi7_dev *dev);

int wifi7_mac_security_init(struct wifi7_dev *dev);
int wifi7_mac_security_deinit(struct wifi7_dev *dev);

int wifi7_mac_power_init(struct wifi7_dev *dev);
int wifi7_mac_power_deinit(struct wifi7_dev *dev);

int wifi7_mac_get_stats(struct wifi7_dev *dev,
                       struct wifi7_mac_stats *stats);
int wifi7_mac_clear_stats(struct wifi7_dev *dev);

#endif /* __WIFI7_MAC_CORE_H */ 