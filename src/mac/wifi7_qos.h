/*
 * WiFi 7 QoS and Traffic Management
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_QOS_H
#define __WIFI7_QOS_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include "../core/wifi7_core.h"

/* QoS capabilities */
#define WIFI7_QOS_CAP_MULTI_TID    BIT(0)  /* Multi-TID support */
#define WIFI7_QOS_CAP_MLO          BIT(1)  /* MLO QoS support */
#define WIFI7_QOS_CAP_LATENCY      BIT(2)  /* Latency-based QoS */
#define WIFI7_QOS_CAP_DYNAMIC      BIT(3)  /* Dynamic prioritization */
#define WIFI7_QOS_CAP_ADVANCED_Q   BIT(4)  /* Advanced queue management */
#define WIFI7_QOS_CAP_ADMISSION    BIT(5)  /* Admission control */
#define WIFI7_QOS_CAP_SCHEDULER    BIT(6)  /* Advanced scheduler */
#define WIFI7_QOS_CAP_AIRTIME      BIT(7)  /* Airtime fairness */

/* QoS TID definitions */
#define WIFI7_QOS_TID_MAX          7
#define WIFI7_QOS_TID_VOICE        7  /* Highest priority */
#define WIFI7_QOS_TID_VIDEO        6
#define WIFI7_QOS_TID_BESTEFFORT   0
#define WIFI7_QOS_TID_BACKGROUND   1

/* QoS queue states */
#define WIFI7_QUEUE_STOPPED        0
#define WIFI7_QUEUE_RUNNING        1
#define WIFI7_QUEUE_BLOCKED        2
#define WIFI7_QUEUE_SUSPENDED      3

/* QoS traffic classes */
#define WIFI7_TC_VOICE             3  /* Highest priority */
#define WIFI7_TC_VIDEO             2
#define WIFI7_TC_BESTEFFORT        1
#define WIFI7_TC_BACKGROUND        0  /* Lowest priority */

/* QoS configuration */
struct wifi7_qos_config {
    u32 capabilities;          /* QoS capabilities */
    bool multi_tid;            /* Multi-TID enabled */
    bool mlo_qos;             /* MLO QoS enabled */
    bool dynamic_priority;     /* Dynamic priority enabled */
    bool admission_control;    /* Admission control enabled */
    u8 max_queues;            /* Maximum queues */
    u8 default_tid;           /* Default TID */
    u32 queue_size_max;       /* Maximum queue size */
    u32 queue_timeout;        /* Queue timeout in ms */
    struct {
        u32 voice_weight;      /* Voice traffic weight */
        u32 video_weight;      /* Video traffic weight */
        u32 besteffort_weight; /* Best effort weight */
        u32 background_weight; /* Background weight */
    } weights;
};

/* QoS queue statistics */
struct wifi7_qos_queue_stats {
    u32 enqueued;             /* Packets enqueued */
    u32 dequeued;             /* Packets dequeued */
    u32 dropped;              /* Packets dropped */
    u32 rejected;             /* Packets rejected */
    u32 requeued;            /* Packets requeued */
    u32 queue_length;        /* Current queue length */
    u32 queue_time;          /* Average queue time */
    u32 peak_length;         /* Peak queue length */
    u32 overflow_count;      /* Queue overflow count */
    u32 underflow_count;     /* Queue underflow count */
};

/* QoS statistics */
struct wifi7_qos_stats {
    struct wifi7_qos_queue_stats queue_stats[WIFI7_QOS_TID_MAX + 1];
    u32 total_enqueued;       /* Total packets enqueued */
    u32 total_dequeued;       /* Total packets dequeued */
    u32 total_dropped;        /* Total packets dropped */
    u32 total_rejected;       /* Total packets rejected */
    u32 admission_refused;    /* Admission control refusals */
    u32 priority_changes;     /* Priority changes */
    u32 scheduler_runs;       /* Scheduler run count */
    ktime_t last_update;      /* Last statistics update */
};

/* Function prototypes */
int wifi7_qos_init(struct wifi7_dev *dev);
void wifi7_qos_deinit(struct wifi7_dev *dev);

int wifi7_qos_start(struct wifi7_dev *dev);
void wifi7_qos_stop(struct wifi7_dev *dev);

int wifi7_qos_set_config(struct wifi7_dev *dev,
                        struct wifi7_qos_config *config);
int wifi7_qos_get_config(struct wifi7_dev *dev,
                        struct wifi7_qos_config *config);

int wifi7_qos_enqueue(struct wifi7_dev *dev,
                     struct sk_buff *skb,
                     u8 tid);
struct sk_buff *wifi7_qos_dequeue(struct wifi7_dev *dev,
                                 u8 tid);

int wifi7_qos_start_queue(struct wifi7_dev *dev, u8 tid);
int wifi7_qos_stop_queue(struct wifi7_dev *dev, u8 tid);
int wifi7_qos_wake_queue(struct wifi7_dev *dev, u8 tid);

int wifi7_qos_get_queue_stats(struct wifi7_dev *dev,
                             u8 tid,
                             struct wifi7_qos_queue_stats *stats);
int wifi7_qos_get_stats(struct wifi7_dev *dev,
                       struct wifi7_qos_stats *stats);
int wifi7_qos_clear_stats(struct wifi7_dev *dev);

/* Debug interface */
#ifdef CONFIG_WIFI7_QOS_DEBUG
int wifi7_qos_debugfs_init(struct wifi7_dev *dev);
void wifi7_qos_debugfs_remove(struct wifi7_dev *dev);
#else
static inline int wifi7_qos_debugfs_init(struct wifi7_dev *dev) { return 0; }
static inline void wifi7_qos_debugfs_remove(struct wifi7_dev *dev) {}
#endif

#endif /* __WIFI7_QOS_H */ 