/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_MU_MIMO_H
#define __WIFI7_MU_MIMO_H

#include <linux/types.h>
#include <linux/ieee80211.h>
#include "wifi7_phy.h"
#include "wifi7_beamforming.h"

/* MU-MIMO capabilities */
#define WIFI7_MU_MAX_GROUPS         16
#define WIFI7_MU_MAX_USERS_PER_GROUP 8
#define WIFI7_MU_MAX_SPATIAL_STREAMS 16

/* Group formation parameters */
#define WIFI7_MU_MIN_RSSI_DB        -82
#define WIFI7_MU_MIN_SNR_DB         15
#define WIFI7_MU_MAX_SPATIAL_REUSE   4

/* Scheduling parameters */
#define WIFI7_MU_MIN_SCHED_INTERVAL_US  100
#define WIFI7_MU_MAX_SCHED_INTERVAL_US  5000
#define WIFI7_MU_GROUP_TIMEOUT_US       1000

/* MU-MIMO group states */
enum wifi7_mu_group_state {
    WIFI7_MU_GROUP_IDLE = 0,
    WIFI7_MU_GROUP_FORMING,
    WIFI7_MU_GROUP_READY,
    WIFI7_MU_GROUP_ACTIVE,
    WIFI7_MU_GROUP_PAUSED,
    WIFI7_MU_GROUP_ERROR
};

/* Spatial compatibility metrics */
struct wifi7_mu_spatial_info {
    s8 rssi;                  /* Signal strength in dBm */
    u8 snr;                   /* Signal-to-noise ratio */
    u8 spatial_reuse;         /* Spatial reuse factor */
    u8 interference_level;    /* Interference metric */
    
    /* Correlation tracking */
    struct {
        u16 correlation;      /* Spatial correlation metric */
        u16 isolation;        /* Inter-user isolation */
        u8  rank;            /* Channel rank estimate */
    } metrics;
    
    /* Time tracking */
    ktime_t last_update;
};

/* User stream allocation */
struct wifi7_mu_stream_alloc {
    u8 aid;                  /* Association ID */
    u8 num_streams;          /* Allocated spatial streams */
    u8 mcs;                  /* MCS selection */
    u8 power_level;          /* Transmit power level */
    
    /* Stream mapping */
    struct {
        u8 stream_idx;       /* Physical stream index */
        u8 antenna_mask;     /* Antenna selection mask */
    } *stream_map;
    
    /* QoS tracking */
    u32 queue_length;        /* Current queue length */
    u32 airtime_deficit;     /* Airtime tracking */
};

/* MU-MIMO group */
struct wifi7_mu_group {
    u8 group_id;
    enum wifi7_mu_group_state state;
    
    /* Member information */
    u8 num_users;
    struct {
        u8 aid;
        struct wifi7_mu_spatial_info spatial;
        struct wifi7_mu_stream_alloc streams;
        bool ready;
    } users[WIFI7_MU_MAX_USERS_PER_GROUP];
    
    /* Group characteristics */
    u8 total_spatial_streams;
    u32 last_schedule;        /* in microseconds */
    bool dl_mu_mimo_ready;    /* Downlink MU-MIMO ready */
    bool ul_mu_mimo_ready;    /* Uplink MU-MIMO ready */
    
    /* Performance tracking */
    struct {
        u32 successful_tx;
        u32 failed_tx;
        u32 collisions;
        u32 scheduling_errors;
    } stats;
};

/* Main MU-MIMO context */
struct wifi7_mu_context {
    struct wifi7_phy_dev *phy;
    struct wifi7_bf_context *bf;
    
    /* Group management */
    struct wifi7_mu_group groups[WIFI7_MU_MAX_GROUPS];
    u8 num_active_groups;
    spinlock_t group_lock;
    
    /* Scheduling */
    struct workqueue_struct *mu_wq;
    struct delayed_work sched_work;
    
    /* Global statistics */
    struct {
        atomic_t groups_active;
        u32 total_tx_success;
        u32 total_tx_failed;
        u32 spatial_streams_used;
        u32 scheduling_conflicts;
    } stats;
};

/* Function prototypes */
struct wifi7_mu_context *wifi7_mu_alloc(struct wifi7_phy_dev *phy,
                                       struct wifi7_bf_context *bf);
void wifi7_mu_free(struct wifi7_mu_context *mu);

/* Group management */
int wifi7_mu_group_add_user(struct wifi7_mu_context *mu,
                           u8 group_id, u8 aid,
                           const struct wifi7_mu_spatial_info *spatial);
int wifi7_mu_group_remove_user(struct wifi7_mu_context *mu,
                              u8 group_id, u8 aid);
int wifi7_mu_group_update_spatial(struct wifi7_mu_context *mu,
                                u8 group_id, u8 aid,
                                const struct wifi7_mu_spatial_info *spatial);

/* Stream management */
int wifi7_mu_alloc_streams(struct wifi7_mu_context *mu,
                          u8 group_id, u8 aid,
                          u8 num_streams);
int wifi7_mu_update_streams(struct wifi7_mu_context *mu,
                           u8 group_id, u8 aid,
                           const struct wifi7_mu_stream_alloc *alloc);

/* Group operations */
int wifi7_mu_group_start(struct wifi7_mu_context *mu, u8 group_id);
void wifi7_mu_group_stop(struct wifi7_mu_context *mu, u8 group_id);
int wifi7_mu_group_pause(struct wifi7_mu_context *mu, u8 group_id);
int wifi7_mu_group_resume(struct wifi7_mu_context *mu, u8 group_id);

/* Transmission control */
int wifi7_mu_tx_prepare(struct wifi7_mu_context *mu,
                       u8 group_id,
                       struct sk_buff *skb);
void wifi7_mu_tx_done(struct wifi7_mu_context *mu,
                      u8 group_id,
                      bool success);

/* Status and debug */
void wifi7_mu_dump_stats(struct wifi7_mu_context *mu);
void wifi7_mu_dump_group(struct wifi7_mu_context *mu, u8 group_id);

#endif /* __WIFI7_MU_MIMO_H */ 