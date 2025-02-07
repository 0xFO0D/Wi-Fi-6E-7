/*
 * WiFi 7 V2X Communication Module
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_V2X_H
#define __WIFI7_V2X_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include "../core/wifi7_core.h"

/* V2X Operation Modes */
#define WIFI7_V2X_MODE_DIRECT     0  /* Direct communication */
#define WIFI7_V2X_MODE_INFRA      1  /* Infrastructure mode */
#define WIFI7_V2X_MODE_HYBRID     2  /* Hybrid mode */

/* V2X Message Types */
#define WIFI7_V2X_MSG_BSM         0  /* Basic Safety Message */
#define WIFI7_V2X_MSG_EVA         1  /* Emergency Vehicle Alert */
#define WIFI7_V2X_MSG_RSA         2  /* Road Side Alert */
#define WIFI7_V2X_MSG_TIM         3  /* Traveler Information Message */
#define WIFI7_V2X_MSG_SPAT        4  /* Signal Phase and Timing */
#define WIFI7_V2X_MSG_MAP         5  /* Map Data */

/* V2X Priority Levels */
#define WIFI7_V2X_PRIO_EMERGENCY  0  /* Emergency messages */
#define WIFI7_V2X_PRIO_SAFETY     1  /* Safety-critical messages */
#define WIFI7_V2X_PRIO_MOBILITY   2  /* Mobility data */
#define WIFI7_V2X_PRIO_INFO       3  /* Information messages */

/* V2X Configuration */
struct wifi7_v2x_config {
    u8 mode;                  /* Operation mode */
    bool low_latency;         /* Low latency mode */
    bool congestion_control;  /* Congestion control */
    bool security_enabled;    /* Security features */
    u32 max_range;           /* Maximum communication range */
    u32 channel_interval;    /* Channel switching interval */
    u32 max_retries;        /* Maximum retry attempts */
    struct {
        u32 emergency;       /* Emergency message interval */
        u32 safety;         /* Safety message interval */
        u32 mobility;       /* Mobility message interval */
        u32 info;           /* Info message interval */
    } intervals;
};

/* V2X Statistics */
struct wifi7_v2x_stats {
    u32 msgs_tx;             /* Messages transmitted */
    u32 msgs_rx;             /* Messages received */
    u32 msgs_dropped;        /* Messages dropped */
    u32 retries;             /* Retry count */
    u32 latency_avg;         /* Average latency (μs) */
    u32 latency_max;         /* Maximum latency (μs) */
    u32 range_avg;           /* Average range (m) */
    u32 congestion_events;   /* Congestion events */
    u32 security_failures;   /* Security validation failures */
    struct {
        u32 emergency;       /* Emergency messages */
        u32 safety;         /* Safety messages */
        u32 mobility;       /* Mobility messages */
        u32 info;           /* Info messages */
    } msg_counts;
};

/* Function prototypes */
int wifi7_v2x_init(struct wifi7_dev *dev);
void wifi7_v2x_deinit(struct wifi7_dev *dev);

int wifi7_v2x_start(struct wifi7_dev *dev);
void wifi7_v2x_stop(struct wifi7_dev *dev);

int wifi7_v2x_set_config(struct wifi7_dev *dev,
                        struct wifi7_v2x_config *config);
int wifi7_v2x_get_config(struct wifi7_dev *dev,
                        struct wifi7_v2x_config *config);

int wifi7_v2x_send_msg(struct wifi7_dev *dev,
                      struct sk_buff *skb,
                      u8 msg_type,
                      u8 priority);
int wifi7_v2x_get_stats(struct wifi7_dev *dev,
                       struct wifi7_v2x_stats *stats);

#ifdef CONFIG_WIFI7_V2X_DEBUG
int wifi7_v2x_debugfs_init(struct wifi7_dev *dev);
void wifi7_v2x_debugfs_remove(struct wifi7_dev *dev);
#else
static inline int wifi7_v2x_debugfs_init(struct wifi7_dev *dev) { return 0; }
static inline void wifi7_v2x_debugfs_remove(struct wifi7_dev *dev) {}
#endif

#endif /* __WIFI7_V2X_H */ 