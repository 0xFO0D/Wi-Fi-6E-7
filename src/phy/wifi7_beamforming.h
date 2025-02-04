/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_BEAMFORMING_H
#define __WIFI7_BEAMFORMING_H

#include <linux/types.h>
#include <linux/ieee80211.h>
#include "wifi7_phy.h"

/* Beamforming capabilities */
#define WIFI7_BF_MAX_USERS          8
#define WIFI7_BF_MAX_STREAMS        16
#define WIFI7_BF_MAX_ANTENNAS       16
#define WIFI7_BF_CSI_MAX_TONES      996  /* For 320MHz */

/* Sounding parameters */
#define WIFI7_BF_MIN_SOUNDING_INTERVAL_MS  10
#define WIFI7_BF_MAX_SOUNDING_INTERVAL_MS  500
#define WIFI7_BF_GROUP_TIMEOUT_MS          100

/* CSI feedback compression */
#define WIFI7_BF_ANGLE_QUANTIZATION_BITS   7
#define WIFI7_BF_SNR_QUANTIZATION_BITS     4

/* Beamforming group state */
enum wifi7_bf_group_state {
    WIFI7_BF_GROUP_IDLE = 0,
    WIFI7_BF_GROUP_SOUNDING,
    WIFI7_BF_GROUP_FEEDBACK,
    WIFI7_BF_GROUP_COMPUTING,
    WIFI7_BF_GROUP_ACTIVE,
    WIFI7_BF_GROUP_ERROR
};

/* Channel state information */
struct wifi7_bf_csi_matrix {
    u16 num_tones;
    u8  num_tx_antennas;
    u8  num_rx_antennas;
    
    /* CSI data - compressed format */
    struct {
        u8 angle;          /* Phase angle, quantized */
        u8 magnitude;      /* Magnitude, quantized */
        u8 snr;           /* SNR for this stream */
    } __packed *elements;
    
    /* Timestamp for aging */
    ktime_t timestamp;
};

/* Steering matrix */
struct wifi7_bf_steering_matrix {
    u8 num_streams;
    u8 num_antennas;
    
    /* Steering vectors */
    struct {
        s8 i;             /* In-phase component */
        s8 q;             /* Quadrature component */
    } __packed *elements;
    
    /* Validity tracking */
    bool valid;
    ktime_t last_update;
};

/* Beamforming group */
struct wifi7_bf_group {
    u8 group_id;
    enum wifi7_bf_group_state state;
    
    /* Member information */
    u8 num_users;
    struct {
        u8 aid;           /* Association ID */
        u8 num_streams;   /* Number of spatial streams */
        bool feedback_ready;
        struct wifi7_bf_csi_matrix csi;
        struct wifi7_bf_steering_matrix steering;
    } users[WIFI7_BF_MAX_USERS];
    
    /* Group characteristics */
    u32 sounding_interval;    /* in milliseconds */
    u32 last_sounding;       /* jiffies */
    bool mu_mimo_capable;    /* MU-MIMO support */
    
    /* Statistics */
    struct {
        u32 sounding_count;
        u32 feedback_timeouts;
        u32 steering_updates;
        u32 error_count;
    } stats;
};

/* Main beamforming context */
struct wifi7_bf_context {
    struct wifi7_phy_dev *phy;
    
    /* Group management */
    struct wifi7_bf_group groups[WIFI7_BF_MAX_USERS];
    u8 num_active_groups;
    spinlock_t group_lock;
    
    /* Workqueue for async operations */
    struct workqueue_struct *bf_wq;
    struct delayed_work sounding_work;
    
    /* Global statistics */
    struct {
        atomic_t sounding_in_progress;
        u32 total_soundings;
        u32 successful_soundings;
        u32 failed_soundings;
        u32 feedback_timeouts;
    } stats;
};

/* Function prototypes */
struct wifi7_bf_context *wifi7_bf_alloc(struct wifi7_phy_dev *phy);
void wifi7_bf_free(struct wifi7_bf_context *bf);

/* Group management */
int wifi7_bf_group_add_user(struct wifi7_bf_context *bf,
                           u8 group_id, u8 aid,
                           u8 num_streams);
int wifi7_bf_group_remove_user(struct wifi7_bf_context *bf,
                              u8 group_id, u8 aid);
int wifi7_bf_group_start(struct wifi7_bf_context *bf, u8 group_id);
void wifi7_bf_group_stop(struct wifi7_bf_context *bf, u8 group_id);

/* Beamforming operations */
int wifi7_bf_send_ndp(struct wifi7_bf_context *bf, u8 group_id);
int wifi7_bf_process_feedback(struct wifi7_bf_context *bf,
                            u8 group_id, u8 aid,
                            const u8 *feedback_data,
                            size_t len);
int wifi7_bf_compute_steering(struct wifi7_bf_context *bf,
                            u8 group_id);

/* Status and debug */
void wifi7_bf_dump_stats(struct wifi7_bf_context *bf);
void wifi7_bf_dump_csi(struct wifi7_bf_context *bf,
                      u8 group_id, u8 aid);

#endif /* __WIFI7_BEAMFORMING_H */ 