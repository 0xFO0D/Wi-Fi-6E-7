/*
 * WiFi 7 Automotive Security Module
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_AUTO_SEC_H
#define __WIFI7_AUTO_SEC_H

#include <linux/types.h>
#include "../core/wifi7_core.h"

/* Security threat levels */
#define WIFI7_SEC_THREAT_NONE     0  /* No threat detected */
#define WIFI7_SEC_THREAT_LOW      1  /* Low severity threat */
#define WIFI7_SEC_THREAT_MEDIUM   2  /* Medium severity threat */
#define WIFI7_SEC_THREAT_HIGH     3  /* High severity threat */
#define WIFI7_SEC_THREAT_CRITICAL 4  /* Critical severity threat */

/* Security event types */
#define WIFI7_SEC_EVT_JAMMING    BIT(0)  /* Signal jamming */
#define WIFI7_SEC_EVT_SPOOFING   BIT(1)  /* Message spoofing */
#define WIFI7_SEC_EVT_REPLAY     BIT(2)  /* Replay attack */
#define WIFI7_SEC_EVT_MITM       BIT(3)  /* Man-in-the-middle */
#define WIFI7_SEC_EVT_DOS        BIT(4)  /* Denial of service */
#define WIFI7_SEC_EVT_TAMPERING  BIT(5)  /* Physical tampering */

/* Security configuration */
struct wifi7_auto_sec_config {
    bool monitoring_enabled;     /* Security monitoring enabled */
    bool auto_response;         /* Automatic response enabled */
    u32 scan_interval;         /* Monitoring interval (ms) */
    u32 threat_threshold;      /* Threat detection threshold */
    u32 event_mask;           /* Enabled event types */
    struct {
        u32 jamming_threshold;  /* Jamming detection threshold */
        u32 replay_window;      /* Replay detection window */
        u32 auth_timeout;       /* Authentication timeout */
        u32 response_delay;     /* Response delay time */
    } params;
};

/* Security event */
struct wifi7_auto_sec_event {
    u32 type;                  /* Event type */
    u8 threat_level;          /* Threat level */
    u32 timestamp;            /* Event timestamp */
    u32 duration;             /* Event duration */
    u32 frequency;            /* Affected frequency */
    u32 source_id;            /* Source identifier */
    u32 sequence;             /* Sequence number */
    bool resolved;            /* Event resolved flag */
};

/* Security statistics */
struct wifi7_auto_sec_stats {
    u32 events_detected;       /* Total events detected */
    u32 events_resolved;       /* Events resolved */
    u32 false_positives;      /* False positive detections */
    struct {
        u32 jamming;          /* Jamming events */
        u32 spoofing;         /* Spoofing events */
        u32 replay;           /* Replay events */
        u32 mitm;             /* MITM events */
        u32 dos;              /* DoS events */
        u32 tampering;        /* Tampering events */
    } counts;
    struct {
        u32 detection_time;    /* Average detection time */
        u32 response_time;     /* Average response time */
        u32 resolution_time;   /* Average resolution time */
    } timing;
};

/* Function prototypes */
int wifi7_auto_sec_init(struct wifi7_dev *dev);
void wifi7_auto_sec_deinit(struct wifi7_dev *dev);

int wifi7_auto_sec_start(struct wifi7_dev *dev);
void wifi7_auto_sec_stop(struct wifi7_dev *dev);

int wifi7_auto_sec_set_config(struct wifi7_dev *dev,
                             struct wifi7_auto_sec_config *config);
int wifi7_auto_sec_get_config(struct wifi7_dev *dev,
                             struct wifi7_auto_sec_config *config);

int wifi7_auto_sec_report_event(struct wifi7_dev *dev,
                               struct wifi7_auto_sec_event *event);
int wifi7_auto_sec_get_event(struct wifi7_dev *dev,
                            struct wifi7_auto_sec_event *event);

int wifi7_auto_sec_get_stats(struct wifi7_dev *dev,
                            struct wifi7_auto_sec_stats *stats);
int wifi7_auto_sec_clear_stats(struct wifi7_dev *dev);

#ifdef CONFIG_WIFI7_AUTO_SEC_DEBUG
int wifi7_auto_sec_debugfs_init(struct wifi7_dev *dev);
void wifi7_auto_sec_debugfs_remove(struct wifi7_dev *dev);
#else
static inline int wifi7_auto_sec_debugfs_init(struct wifi7_dev *dev) { return 0; }
static inline void wifi7_auto_sec_debugfs_remove(struct wifi7_dev *dev) {}
#endif

#endif /* __WIFI7_AUTO_SEC_H */ 