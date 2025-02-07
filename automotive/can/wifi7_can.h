/*
 * WiFi 7 CAN Bus Integration Module
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_CAN_H
#define __WIFI7_CAN_H

#include <linux/types.h>
#include <linux/can.h>
#include <linux/can/dev.h>
#include "../core/wifi7_core.h"

/* CAN Interface States */
#define WIFI7_CAN_STATE_DOWN      0  /* Interface down */
#define WIFI7_CAN_STATE_UP        1  /* Interface up */
#define WIFI7_CAN_STATE_ERROR     2  /* Error state */
#define WIFI7_CAN_STATE_SLEEP     3  /* Sleep mode */

/* CAN Message Priority */
#define WIFI7_CAN_PRIO_HIGH       0  /* High priority */
#define WIFI7_CAN_PRIO_MEDIUM     1  /* Medium priority */
#define WIFI7_CAN_PRIO_LOW        2  /* Low priority */

/* CAN Configuration */
struct wifi7_can_config {
    bool enabled;             /* CAN interface enabled */
    u32 bitrate;             /* Bitrate in bits/sec */
    u8 sjw;                  /* Synchronization Jump Width */
    u8 tseg1;               /* Time Segment 1 */
    u8 tseg2;               /* Time Segment 2 */
    bool listen_only;        /* Listen-only mode */
    bool loopback;          /* Loopback mode */
    bool one_shot;          /* One-shot mode */
    bool berr_reporting;    /* Bus error reporting */
    struct {
        u32 rx_queue_size;   /* RX queue size */
        u32 tx_queue_size;   /* TX queue size */
        u32 timeout;         /* Operation timeout */
    } queue;
};

/* CAN Statistics */
struct wifi7_can_stats {
    u32 frames_tx;           /* Frames transmitted */
    u32 frames_rx;           /* Frames received */
    u32 frames_dropped;      /* Frames dropped */
    u32 bus_errors;         /* Bus errors */
    u32 arbitration_lost;   /* Arbitration lost events */
    u32 rx_overflows;       /* RX queue overflows */
    u32 tx_timeouts;        /* TX timeouts */
    struct {
        u32 form;           /* Form errors */
        u32 ack;            /* ACK errors */
        u32 bit;            /* Bit errors */
        u32 stuff;          /* Stuff errors */
        u32 crc;            /* CRC errors */
    } error_counters;
};

/* Function prototypes */
int wifi7_can_init(struct wifi7_dev *dev);
void wifi7_can_deinit(struct wifi7_dev *dev);

int wifi7_can_start(struct wifi7_dev *dev);
void wifi7_can_stop(struct wifi7_dev *dev);

int wifi7_can_set_config(struct wifi7_dev *dev,
                        struct wifi7_can_config *config);
int wifi7_can_get_config(struct wifi7_dev *dev,
                        struct wifi7_can_config *config);

int wifi7_can_send_frame(struct wifi7_dev *dev,
                        struct can_frame *frame,
                        u8 priority);
int wifi7_can_recv_frame(struct wifi7_dev *dev,
                        struct can_frame *frame);

int wifi7_can_get_state(struct wifi7_dev *dev);
int wifi7_can_get_stats(struct wifi7_dev *dev,
                       struct wifi7_can_stats *stats);

/* Debug interface */
#ifdef CONFIG_WIFI7_CAN_DEBUG
int wifi7_can_debugfs_init(struct wifi7_dev *dev);
void wifi7_can_debugfs_remove(struct wifi7_dev *dev);
#else
static inline int wifi7_can_debugfs_init(struct wifi7_dev *dev) { return 0; }
static inline void wifi7_can_debugfs_remove(struct wifi7_dev *dev) {}
#endif

#endif /* __WIFI7_CAN_H */ 