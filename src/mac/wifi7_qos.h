/*
 * WiFi 7 QoS Management Header
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 *
 * Defines the interface and data structures for WiFi 7 QoS management,
 * including traffic classification, queue management, and MLO support.
 */

#ifndef __WIFI7_QOS_H
#define __WIFI7_QOS_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include "../core/wifi7_core.h"

/*
 * Access Categories in order of increasing priority
 * Follows 802.11e/802.11ax QoS specifications
 */
#define WIFI7_AC_BK     0  /* Background */
#define WIFI7_AC_BE     1  /* Best Effort */
#define WIFI7_AC_VI     2  /* Video */
#define WIFI7_AC_VO     3  /* Voice */
#define WIFI7_NUM_ACS   4

/*
 * Traffic Identifier (TID) definitions
 * TIDs 0-7 map to user priorities
 * TID 8 reserved for management frames
 */
#define WIFI7_TID_MAX   7
#define WIFI7_TID_MASK  0x7
#define WIFI7_MGMT_TID  8
#define WIFI7_NUM_TIDS  9

/*
 * Queue and buffer management limits
 * Sized to handle typical traffic patterns while preventing buffer bloat
 */
#define WIFI7_MAX_QUEUES       16
#define WIFI7_MAX_QUEUE_DEPTH  512
#define WIFI7_MIN_QUEUE_DEPTH  32
#define WIFI7_MAX_AMPDU_LEN    256
#define WIFI7_MAX_AMPDU_TIDS   8

/*
 * Traffic steering modes
 * Different policies for MLO link selection
 */
#define WIFI7_STEER_NONE       0
#define WIFI7_STEER_LOAD       1
#define WIFI7_STEER_LATENCY    2
#define WIFI7_STEER_AIRTIME    3
#define WIFI7_STEER_THROUGHPUT 4
#define WIFI7_STEER_CUSTOM     5

/*
 * QoS capability flags
 * Controls features like aggregation and ACK policy
 */
#define WIFI7_QOS_FLAG_AMPDU   BIT(0)  /* Enable AMPDU aggregation */
#define WIFI7_QOS_FLAG_AMSDU   BIT(1)  /* Enable AMSDU aggregation */
#define WIFI7_QOS_FLAG_BLOCK   BIT(2)  /* Use block acknowledgment */
#define WIFI7_QOS_FLAG_NOACK   BIT(3)  /* No acknowledgment required */
#define WIFI7_QOS_FLAG_BURST   BIT(4)  /* Allow frame bursting */
#define WIFI7_QOS_FLAG_LOW_LAT BIT(5)  /* Optimize for low latency */
#define WIFI7_QOS_FLAG_HIGH_TP BIT(6)  /* Optimize for throughput */
#define WIFI7_QOS_FLAG_POWER   BIT(7)  /* Power save mode */

/* Traffic classification */
struct wifi7_tid_config {
    u8 tid;
    u8 ac;
    u8 link_mask;
    u16 queue_limit;
    u16 ampdu_limit;
    u32 flags;
    u32 min_rate;
    u32 max_rate;
    u32 target_latency;
    bool active;
};

/* Queue state tracking */
struct wifi7_queue_stats {
    u32 enqueued;
    u32 dequeued;
    u32 dropped;
    u32 rejected;
    u32 completed;
    u32 retried;
    u32 avg_sojourn;
    u32 avg_latency;
    u32 peak_latency;
    u32 bytes_pending;
    u32 airtime_used;
};

/* Per-link QoS state */
struct wifi7_link_qos {
    u8 link_id;
    u32 active_tids;
    u32 active_queues;
    u32 queue_flags[WIFI7_MAX_QUEUES];
    struct wifi7_queue_stats stats[WIFI7_MAX_QUEUES];
    spinlock_t lock;
    
    /* Traffic monitoring */
    u32 tx_airtime;
    u32 rx_airtime;
    u32 busy_time;
    u32 tx_bytes;
    u32 rx_bytes;
    u32 tx_mpdu;
    u32 rx_mpdu;
    u32 tx_ampdu;
    u32 rx_ampdu;
    
    /* Rate control state */
    u32 current_rate;
    u32 target_rate;
    u32 max_rate;
    u32 min_rate;
    u8 rate_flags;
    
    /* Buffer management */
    u32 buffer_used;
    u32 buffer_max;
    u32 buffer_min;
    u32 drops_overflow;
    u32 drops_underrun;
};

/* Multi-TID aggregation state */
struct wifi7_mtid_state {
    bool enabled;
    u8 max_tids;
    u8 active_tids;
    u16 max_ampdu;
    u32 timeout_us;
    u32 tid_bitmap;
    u32 last_update;
    spinlock_t lock;
};

/* Traffic steering policy */
struct wifi7_steer_policy {
    u8 mode;
    u8 link_weight[WIFI7_MAX_LINKS];
    u32 load_threshold;
    u32 latency_threshold;
    u32 airtime_threshold;
    u32 rate_threshold;
    bool active;
};

/* Main QoS management structure */
struct wifi7_qos {
    /* TID management */
    struct wifi7_tid_config tids[WIFI7_NUM_TIDS];
    struct wifi7_mtid_state mtid;
    spinlock_t tid_lock;
    
    /* Per-link state */
    struct wifi7_link_qos links[WIFI7_MAX_LINKS];
    struct wifi7_steer_policy steer;
    u32 active_links;
    
    /* Queue management */
    struct sk_buff_head queues[WIFI7_MAX_QUEUES];
    spinlock_t queue_locks[WIFI7_MAX_QUEUES];
    u32 queue_mapping[WIFI7_NUM_TIDS];
    
    /* Hardware interface */
    void *hw_queues;
    void *dma_rings;
    
    /* Statistics and monitoring */
    struct delayed_work stats_work;
    u32 update_interval;
    bool stats_enabled;
    
    /* Debugging */
    struct dentry *debugfs_dir;
    bool debug_enabled;
};

/* Function prototypes */
int wifi7_qos_init(struct wifi7_dev *dev);
void wifi7_qos_deinit(struct wifi7_dev *dev);

int wifi7_qos_setup_tid(struct wifi7_dev *dev, struct wifi7_tid_config *config);
int wifi7_qos_enable_mtid(struct wifi7_dev *dev, u8 max_tids, u32 timeout_us);
int wifi7_qos_set_link_mask(struct wifi7_dev *dev, u8 tid, u8 link_mask);

int wifi7_qos_enqueue(struct wifi7_dev *dev, struct sk_buff *skb, u8 tid);
struct sk_buff *wifi7_qos_dequeue(struct wifi7_dev *dev, u8 link_id);
void wifi7_qos_flush(struct wifi7_dev *dev, u8 tid);

int wifi7_qos_set_steering(struct wifi7_dev *dev, 
                          struct wifi7_steer_policy *policy);
int wifi7_qos_get_stats(struct wifi7_dev *dev, u8 link_id,
                        struct wifi7_queue_stats *stats);

/* TODO: Implement ML-based traffic prediction */
int wifi7_qos_predict_load(struct wifi7_dev *dev, u8 link_id,
                          u32 *predicted_load);

/* TODO: Add support for additional QoS metrics */
int wifi7_qos_get_metrics(struct wifi7_dev *dev, u8 link_id,
                         void *metrics_data);

/* TODO: Optimize queue depth management */
int wifi7_qos_optimize_queues(struct wifi7_dev *dev);

#endif /* __WIFI7_QOS_H */ 