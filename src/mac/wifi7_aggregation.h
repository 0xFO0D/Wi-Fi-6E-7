/*
 * WiFi 7 Cross-Link Frame Aggregation and Reordering
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_AGGREGATION_H
#define __WIFI7_AGGREGATION_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include "../core/wifi7_core.h"

/* Maximum values */
#define WIFI7_NUM_TIDS          8
#define WIFI7_MAX_LINKS         16
#define WIFI7_MAX_AGG_SIZE_MIN  (32 * 1024)    /* 32KB */
#define WIFI7_MAX_AGG_SIZE_MAX  (16 * 1024 * 1024) /* 16MB */
#define WIFI7_MIN_AGG_TIMEOUT   10   /* 10ms */
#define WIFI7_MAX_AGG_TIMEOUT   500  /* 500ms */
#define WIFI7_MIN_REORDER_TIMEOUT 20  /* 20ms */
#define WIFI7_MAX_REORDER_TIMEOUT 1000 /* 1s */

/* Aggregation capabilities */
#define WIFI7_AGG_CAP_BASIC      BIT(0)  /* Basic aggregation */
#define WIFI7_AGG_CAP_EXTENDED   BIT(1)  /* Extended aggregation */
#define WIFI7_AGG_CAP_MULTI_TID  BIT(2)  /* Multi-TID aggregation */
#define WIFI7_AGG_CAP_CROSS_LINK BIT(3)  /* Cross-link aggregation */
#define WIFI7_AGG_CAP_REORDER    BIT(4)  /* Frame reordering */
#define WIFI7_AGG_CAP_COMPRESS   BIT(5)  /* Header compression */
#define WIFI7_AGG_CAP_ENCRYPT    BIT(6)  /* Per-MPDU encryption */
#define WIFI7_AGG_CAP_AMSDU      BIT(7)  /* A-MSDU support */
#define WIFI7_AGG_CAP_AMPDU      BIT(8)  /* A-MPDU support */
#define WIFI7_AGG_CAP_HYBRID     BIT(9)  /* Hybrid aggregation */
#define WIFI7_AGG_CAP_DYNAMIC    BIT(10) /* Dynamic sizing */
#define WIFI7_AGG_CAP_QOS        BIT(11) /* QoS-aware aggregation */
#define WIFI7_AGG_CAP_RETRY      BIT(12) /* Selective retry */
#define WIFI7_AGG_CAP_FEEDBACK   BIT(13) /* Aggregation feedback */
#define WIFI7_AGG_CAP_METRICS    BIT(14) /* Performance metrics */
#define WIFI7_AGG_CAP_DEBUG      BIT(15) /* Debug capabilities */

/* Aggregation flags */
#define WIFI7_AGG_FLAG_IMMEDIATE  BIT(0)  /* Immediate transmission */
#define WIFI7_AGG_FLAG_NO_REORDER BIT(1)  /* Disable reordering */
#define WIFI7_AGG_FLAG_NO_TIMEOUT BIT(2)  /* Disable timeout */
#define WIFI7_AGG_FLAG_PRIORITY   BIT(3)  /* High priority */
#define WIFI7_AGG_FLAG_COMPRESS   BIT(4)  /* Enable compression */
#define WIFI7_AGG_FLAG_ENCRYPT    BIT(5)  /* Enable encryption */
#define WIFI7_AGG_FLAG_FEEDBACK   BIT(6)  /* Request feedback */
#define WIFI7_AGG_FLAG_RETRY      BIT(7)  /* Allow retries */

/* Aggregation statistics */
struct wifi7_agg_stats {
    u32 agg_frames;        /* Total aggregated frames */
    u32 agg_bytes;         /* Total aggregated bytes */
    u32 agg_retries;       /* Aggregation retries */
    u32 agg_drops;         /* Dropped aggregations */
    u32 agg_timeouts;      /* Aggregation timeouts */
    u32 reorder_frames;    /* Reordered frames */
    u32 reorder_drops;     /* Reorder drops */
    u32 reorder_timeouts;  /* Reorder timeouts */
    u32 cross_link_aggs;   /* Cross-link aggregations */
    u32 multi_tid_aggs;    /* Multi-TID aggregations */
    u32 compress_saves;    /* Bytes saved by compression */
    u32 encrypt_frames;    /* Encrypted frames */
    u32 feedback_reqs;     /* Feedback requests */
    u32 retry_success;     /* Successful retries */
    u32 avg_agg_size;      /* Average aggregation size */
    u32 avg_agg_delay;     /* Average aggregation delay */
};

/* Aggregation configuration */
struct wifi7_agg_config {
    u32 capabilities;      /* Aggregation capabilities */
    u32 max_size;         /* Maximum aggregation size */
    u32 max_frames;       /* Maximum frames per aggregation */
    u32 timeout;          /* Aggregation timeout */
    u32 reorder_timeout;  /* Reorder timeout */
    u32 reorder_buffer;   /* Reorder buffer size */
    u8 tid_mask;          /* Enabled TIDs mask */
    u8 link_mask;         /* Enabled links mask */
    bool dynamic_sizing;  /* Enable dynamic sizing */
    bool cross_link;      /* Enable cross-link aggregation */
    bool multi_tid;       /* Enable multi-TID aggregation */
    bool compression;     /* Enable compression */
    bool encryption;      /* Enable encryption */
    bool feedback;        /* Enable feedback */
    bool metrics;         /* Enable metrics collection */
};

/* Function prototypes */
int wifi7_aggregation_init(struct wifi7_dev *dev);
void wifi7_aggregation_deinit(struct wifi7_dev *dev);

int wifi7_add_agg_frame(struct wifi7_dev *dev, struct sk_buff *skb,
                       u8 tid, u8 link_id);
int wifi7_add_reorder_frame(struct wifi7_dev *dev, struct sk_buff *skb,
                          u8 tid, u8 link_id);

void wifi7_process_agg_frames(struct wifi7_dev *dev, u8 tid);
void wifi7_process_reordered_frames(struct wifi7_dev *dev, u8 tid);

int wifi7_set_agg_config(struct wifi7_dev *dev,
                        struct wifi7_agg_config *config);
int wifi7_get_agg_config(struct wifi7_dev *dev,
                        struct wifi7_agg_config *config);

int wifi7_get_agg_stats(struct wifi7_dev *dev,
                       struct wifi7_agg_stats *stats);
int wifi7_clear_agg_stats(struct wifi7_dev *dev);

int wifi7_set_agg_tid_config(struct wifi7_dev *dev, u8 tid,
                            struct wifi7_agg_config *config);
int wifi7_get_agg_tid_config(struct wifi7_dev *dev, u8 tid,
                            struct wifi7_agg_config *config);

int wifi7_set_agg_link_config(struct wifi7_dev *dev, u8 link_id,
                             struct wifi7_agg_config *config);
int wifi7_get_agg_link_config(struct wifi7_dev *dev, u8 link_id,
                             struct wifi7_agg_config *config);

/* Debug interface */
#ifdef CONFIG_WIFI7_AGG_DEBUG
int wifi7_agg_debugfs_init(struct wifi7_dev *dev);
void wifi7_agg_debugfs_remove(struct wifi7_dev *dev);
#else
static inline int wifi7_agg_debugfs_init(struct wifi7_dev *dev) { return 0; }
static inline void wifi7_agg_debugfs_remove(struct wifi7_dev *dev) {}
#endif

#endif /* __WIFI7_AGGREGATION_H */ 