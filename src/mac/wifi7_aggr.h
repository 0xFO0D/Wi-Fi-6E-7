/*
 * WiFi 7 Frame Aggregation
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 *
 * Frame aggregation functionality for WiFi 7 including:
 * - A-MPDU aggregation
 * - A-MSDU aggregation
 * - Multi-TID aggregation
 * - Dynamic aggregation control
 */

#ifndef __WIFI7_AGGR_H
#define __WIFI7_AGGR_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include "../core/wifi7_core.h"

/* Aggregation capabilities */
#define WIFI7_AGGR_CAP_AMPDU        BIT(0)  /* A-MPDU support */
#define WIFI7_AGGR_CAP_AMSDU        BIT(1)  /* A-MSDU support */
#define WIFI7_AGGR_CAP_MULTI_TID    BIT(2)  /* Multi-TID support */
#define WIFI7_AGGR_CAP_DYNAMIC      BIT(3)  /* Dynamic aggregation */
#define WIFI7_AGGR_CAP_REORDER      BIT(4)  /* Reordering support */
#define WIFI7_AGGR_CAP_IMMEDIATE    BIT(5)  /* Immediate BA */
#define WIFI7_AGGR_CAP_COMPRESSED   BIT(6)  /* Compressed BA */
#define WIFI7_AGGR_CAP_EXTENDED     BIT(7)  /* Extended BA */
#define WIFI7_AGGR_CAP_MULTI_STA    BIT(8)  /* Multi-STA BA */
#define WIFI7_AGGR_CAP_ALL_ACK      BIT(9)  /* All-ack policy */
#define WIFI7_AGGR_CAP_SYNC         BIT(10) /* Sync BA */
#define WIFI7_AGGR_CAP_GCR          BIT(11) /* GCR BA */

/* Aggregation parameters */
#define WIFI7_AGGR_MAX_AMPDU_LEN    1048575  /* Max A-MPDU length */
#define WIFI7_AGGR_MAX_AMSDU_LEN    11454    /* Max A-MSDU length */
#define WIFI7_AGGR_MAX_SUBFRAMES    256      /* Max subframes */
#define WIFI7_AGGR_MAX_TIDS         8        /* Max TIDs */
#define WIFI7_AGGR_MAX_QUEUES       16       /* Max queues */
#define WIFI7_AGGR_MAX_DENSITY      16       /* Max density */
#define WIFI7_AGGR_MIN_SPACING      0        /* Min spacing */
#define WIFI7_AGGR_MAX_SPACING      16       /* Max spacing */

/* Aggregation flags */
#define WIFI7_AGGR_FLAG_AMPDU       BIT(0)  /* A-MPDU enabled */
#define WIFI7_AGGR_FLAG_AMSDU       BIT(1)  /* A-MSDU enabled */
#define WIFI7_AGGR_FLAG_IMMEDIATE   BIT(2)  /* Immediate BA */
#define WIFI7_AGGR_FLAG_COMPRESSED  BIT(3)  /* Compressed BA */
#define WIFI7_AGGR_FLAG_EXTENDED    BIT(4)  /* Extended BA */
#define WIFI7_AGGR_FLAG_MULTI_TID   BIT(5)  /* Multi-TID enabled */
#define WIFI7_AGGR_FLAG_DYNAMIC     BIT(6)  /* Dynamic enabled */
#define WIFI7_AGGR_FLAG_REORDER     BIT(7)  /* Reordering enabled */
#define WIFI7_AGGR_FLAG_SYNC        BIT(8)  /* Sync enabled */
#define WIFI7_AGGR_FLAG_GCR         BIT(9)  /* GCR enabled */
#define WIFI7_AGGR_FLAG_ALL_ACK     BIT(10) /* All-ack enabled */

/* Aggregation states */
#define WIFI7_AGGR_STATE_IDLE       0  /* No aggregation */
#define WIFI7_AGGR_STATE_STARTING   1  /* Starting aggregation */
#define WIFI7_AGGR_STATE_ACTIVE     2  /* Aggregation active */
#define WIFI7_AGGR_STATE_STOPPING   3  /* Stopping aggregation */
#define WIFI7_AGGR_STATE_ERROR      4  /* Error state */

/* Aggregation policies */
#define WIFI7_AGGR_POLICY_NONE      0  /* No aggregation */
#define WIFI7_AGGR_POLICY_AMPDU     1  /* A-MPDU only */
#define WIFI7_AGGR_POLICY_AMSDU     2  /* A-MSDU only */
#define WIFI7_AGGR_POLICY_BOTH      3  /* Both A-MPDU and A-MSDU */
#define WIFI7_AGGR_POLICY_DYNAMIC   4  /* Dynamic selection */
#define WIFI7_AGGR_POLICY_ADAPTIVE  5  /* Adaptive selection */

/* Subframe descriptor */
struct wifi7_aggr_subframe {
    struct sk_buff *skb;           /* Subframe buffer */
    u16 len;                      /* Subframe length */
    u8 tid;                       /* Traffic ID */
    u8 ac;                        /* Access category */
    u16 seq;                      /* Sequence number */
    bool qos;                     /* QoS frame */
    bool retry;                   /* Retry frame */
    bool more_frag;              /* More fragments */
    u32 flags;                   /* Subframe flags */
};

/* Aggregation descriptor */
struct wifi7_aggr_desc {
    u8 type;                     /* Aggregation type */
    u8 policy;                   /* Aggregation policy */
    u8 state;                    /* Aggregation state */
    u32 flags;                   /* Aggregation flags */
    
    /* Frame info */
    u16 n_frames;                /* Number of frames */
    u32 len;                     /* Total length */
    struct wifi7_aggr_subframe frames[WIFI7_AGGR_MAX_SUBFRAMES];
    
    /* TID info */
    u8 tid_bitmap;               /* Active TIDs */
    u8 n_tids;                   /* Number of TIDs */
    u8 primary_tid;             /* Primary TID */
    
    /* BA info */
    bool ba_req;                 /* BA requested */
    u8 ba_policy;               /* BA policy */
    u16 ba_timeout;             /* BA timeout */
    u16 ba_size;                /* BA size */
    
    /* Timing */
    ktime_t start_time;          /* Start timestamp */
    ktime_t end_time;            /* End timestamp */
    u32 duration;                /* Duration in us */
    
    /* Status */
    bool complete;               /* Aggregation complete */
    bool transmitted;            /* Frame transmitted */
    bool acknowledged;           /* Frame acknowledged */
    u8 retry_count;             /* Retry count */
};

/* Queue aggregation state */
struct wifi7_aggr_queue {
    u8 queue_id;                /* Queue identifier */
    u8 tid;                     /* Traffic ID */
    u8 ac;                      /* Access category */
    u32 flags;                  /* Queue flags */
    
    /* Parameters */
    u32 max_ampdu_len;          /* Max A-MPDU length */
    u32 max_amsdu_len;          /* Max A-MSDU length */
    u16 max_subframes;          /* Max subframes */
    u8 density;                 /* Aggregation density */
    u8 spacing;                 /* MPDU spacing */
    
    /* State */
    bool active;                /* Queue active */
    u16 n_frames;               /* Number of frames */
    u32 bytes;                  /* Total bytes */
    
    /* Statistics */
    u32 ampdu_tx;              /* A-MPDUs transmitted */
    u32 amsdu_tx;              /* A-MSDUs transmitted */
    u32 ampdu_retry;           /* A-MPDU retries */
    u32 amsdu_retry;           /* A-MSDU retries */
    u32 ampdu_fail;            /* A-MPDU failures */
    u32 amsdu_fail;            /* A-MSDU failures */
    
    /* Timing */
    ktime_t last_aggr;          /* Last aggregation */
    u32 aggr_interval;          /* Aggregation interval */
    
    /* Work */
    struct delayed_work aggr_work;
    spinlock_t lock;
};

/* Aggregation device info */
struct wifi7_aggr {
    /* Capabilities */
    u32 capabilities;           /* Supported features */
    u32 flags;                  /* Enabled features */
    
    /* Parameters */
    u8 policy;                  /* Aggregation policy */
    u16 timeout;               /* Aggregation timeout */
    u8 max_tids;               /* Maximum TIDs */
    u8 max_queues;             /* Maximum queues */
    
    /* Queues */
    struct wifi7_aggr_queue queues[WIFI7_AGGR_MAX_QUEUES];
    u8 n_queues;               /* Number of queues */
    spinlock_t queue_lock;     /* Queue lock */
    
    /* Statistics */
    struct {
        u32 ampdu_tx;          /* A-MPDUs transmitted */
        u32 amsdu_tx;          /* A-MSDUs transmitted */
        u32 ampdu_retry;       /* A-MPDU retries */
        u32 amsdu_retry;       /* A-MSDU retries */
        u32 ampdu_fail;        /* A-MPDU failures */
        u32 amsdu_fail;        /* A-MSDU failures */
        u32 timeouts;          /* Aggregation timeouts */
        u32 overflows;         /* Buffer overflows */
        u32 underruns;         /* Buffer underruns */
    } stats;
    
    /* Work items */
    struct workqueue_struct *wq;
    struct delayed_work housekeeping_work;
    
    /* Debugging */
    struct dentry *debugfs_dir;
    bool debug_enabled;
};

/* Function prototypes */
int wifi7_aggr_init(struct wifi7_dev *dev);
void wifi7_aggr_deinit(struct wifi7_dev *dev);

int wifi7_aggr_start(struct wifi7_dev *dev);
void wifi7_aggr_stop(struct wifi7_dev *dev);

int wifi7_aggr_queue_init(struct wifi7_dev *dev, u8 queue_id);
void wifi7_aggr_queue_deinit(struct wifi7_dev *dev, u8 queue_id);

int wifi7_aggr_add_frame(struct wifi7_dev *dev,
                        struct sk_buff *skb,
                        struct wifi7_aggr_desc *desc);
                        
int wifi7_aggr_process(struct wifi7_dev *dev,
                      struct wifi7_aggr_desc *desc);
                      
int wifi7_aggr_complete(struct wifi7_dev *dev,
                       struct wifi7_aggr_desc *desc);
                       
int wifi7_aggr_get_stats(struct wifi7_dev *dev,
                        struct wifi7_aggr_queue *stats);
                        
int wifi7_aggr_clear_stats(struct wifi7_dev *dev);

#endif /* __WIFI7_AGGR_H */ 