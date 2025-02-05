/*
 * WiFi 7 OFDMA Resource Unit Management
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 *
 * OFDMA functionality for WiFi 7 including:
 * - Resource unit allocation
 * - Trigger frame generation
 * - UL/DL OFDMA scheduling
 * - Multi-user OFDMA
 */

#ifndef __WIFI7_OFDMA_H
#define __WIFI7_OFDMA_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include "../core/wifi7_core.h"

/* OFDMA capabilities */
#define WIFI7_OFDMA_CAP_UL          BIT(0)  /* UL OFDMA */
#define WIFI7_OFDMA_CAP_DL          BIT(1)  /* DL OFDMA */
#define WIFI7_OFDMA_CAP_MU          BIT(2)  /* MU-MIMO */
#define WIFI7_OFDMA_CAP_TRIGGER     BIT(3)  /* Trigger frame */
#define WIFI7_OFDMA_CAP_PUNCTURE    BIT(4)  /* Preamble puncturing */
#define WIFI7_OFDMA_CAP_DYNAMIC     BIT(5)  /* Dynamic allocation */
#define WIFI7_OFDMA_CAP_ADAPTIVE    BIT(6)  /* Adaptive scheduling */
#define WIFI7_OFDMA_CAP_FEEDBACK    BIT(7)  /* Channel feedback */
#define WIFI7_OFDMA_CAP_POWER       BIT(8)  /* Power control */
#define WIFI7_OFDMA_CAP_SPATIAL     BIT(9)  /* Spatial reuse */
#define WIFI7_OFDMA_CAP_QOS         BIT(10) /* QoS support */
#define WIFI7_OFDMA_CAP_MULTI_TID   BIT(11) /* Multi-TID */

/* Resource unit sizes */
#define WIFI7_OFDMA_RU_26           0  /* 26-tone RU */
#define WIFI7_OFDMA_RU_52           1  /* 52-tone RU */
#define WIFI7_OFDMA_RU_106          2  /* 106-tone RU */
#define WIFI7_OFDMA_RU_242          3  /* 242-tone RU */
#define WIFI7_OFDMA_RU_484          4  /* 484-tone RU */
#define WIFI7_OFDMA_RU_996          5  /* 996-tone RU */
#define WIFI7_OFDMA_RU_2x996        6  /* 2x996-tone RU */
#define WIFI7_OFDMA_RU_4x996        7  /* 4x996-tone RU */

/* Maximum values */
#define WIFI7_OFDMA_MAX_RU          8   /* Maximum RU types */
#define WIFI7_OFDMA_MAX_USERS       8   /* Maximum users */
#define WIFI7_OFDMA_MAX_SS          16  /* Maximum spatial streams */
#define WIFI7_OFDMA_MAX_MCS         12  /* Maximum MCS index */
#define WIFI7_OFDMA_MAX_POWER       8   /* Maximum power levels */
#define WIFI7_OFDMA_MAX_RETRIES     4   /* Maximum retries */

/* OFDMA flags */
#define WIFI7_OFDMA_FLAG_UL         BIT(0)  /* UL transmission */
#define WIFI7_OFDMA_FLAG_DL         BIT(1)  /* DL transmission */
#define WIFI7_OFDMA_FLAG_MU         BIT(2)  /* MU-MIMO enabled */
#define WIFI7_OFDMA_FLAG_TRIGGER    BIT(3)  /* Trigger based */
#define WIFI7_OFDMA_FLAG_PUNCTURE   BIT(4)  /* Use puncturing */
#define WIFI7_OFDMA_FLAG_DYNAMIC    BIT(5)  /* Dynamic allocation */
#define WIFI7_OFDMA_FLAG_FEEDBACK   BIT(6)  /* Use feedback */
#define WIFI7_OFDMA_FLAG_POWER      BIT(7)  /* Power control */
#define WIFI7_OFDMA_FLAG_SPATIAL    BIT(8)  /* Spatial reuse */
#define WIFI7_OFDMA_FLAG_QOS        BIT(9)  /* QoS enabled */
#define WIFI7_OFDMA_FLAG_MULTI_TID  BIT(10) /* Multi-TID enabled */

/* Resource unit allocation */
struct wifi7_ofdma_ru {
    u8 type;                    /* RU type */
    u8 index;                   /* RU index */
    u16 tones;                  /* Number of tones */
    u8 start_tone;             /* Starting tone index */
    u8 end_tone;               /* Ending tone index */
    bool punctured;            /* RU is punctured */
    u32 flags;                 /* RU flags */
};

/* User allocation */
struct wifi7_ofdma_user {
    u8 user_id;                /* User identifier */
    u8 ru_index;              /* RU allocation index */
    u8 spatial_streams;        /* Number of spatial streams */
    u8 mcs;                    /* MCS index */
    u8 coding;                 /* Coding type */
    u8 power;                  /* Transmit power */
    u16 aid;                   /* Association ID */
    u8 tid;                    /* Traffic ID */
    bool mu_mimo;              /* MU-MIMO enabled */
    u32 flags;                 /* User flags */
};

/* Trigger frame info */
struct wifi7_ofdma_trigger {
    u8 type;                   /* Trigger type */
    u8 num_users;             /* Number of users */
    u16 duration;             /* Duration in us */
    u8 cs_required;           /* CS required */
    u8 mu_mimo;               /* MU-MIMO mode */
    u8 gi_ltf;                /* GI and LTF type */
    u8 ru_allocation;         /* RU allocation */
    u32 flags;                /* Trigger flags */
    struct wifi7_ofdma_user users[WIFI7_OFDMA_MAX_USERS];
};

/* Channel state info */
struct wifi7_ofdma_csi {
    u8 ru_index;              /* RU index */
    u8 snr;                   /* Signal-to-noise ratio */
    u8 rssi;                  /* Received signal strength */
    u8 evm;                   /* Error vector magnitude */
    u8 doppler;               /* Doppler shift */
    u8 phase_noise;           /* Phase noise */
    u8 timing_error;          /* Timing error */
    u8 frequency_error;       /* Frequency error */
};

/* Resource allocation */
struct wifi7_ofdma_alloc {
    u8 type;                   /* Allocation type */
    u8 num_users;             /* Number of users */
    u8 num_rus;               /* Number of RUs */
    u32 flags;                /* Allocation flags */
    
    /* Resource units */
    struct wifi7_ofdma_ru rus[WIFI7_OFDMA_MAX_RU];
    
    /* User allocations */
    struct wifi7_ofdma_user users[WIFI7_OFDMA_MAX_USERS];
    
    /* Channel state */
    struct wifi7_ofdma_csi csi[WIFI7_OFDMA_MAX_RU];
    
    /* Timing */
    ktime_t start_time;        /* Start timestamp */
    ktime_t end_time;          /* End timestamp */
    u32 duration;              /* Duration in us */
};

/* OFDMA statistics */
struct wifi7_ofdma_stats {
    /* Frame counts */
    u32 ul_frames;            /* UL frames */
    u32 dl_frames;            /* DL frames */
    u32 trigger_frames;       /* Trigger frames */
    u32 mu_frames;            /* MU frames */
    
    /* Resource utilization */
    u32 ru_allocated;         /* RUs allocated */
    u32 ru_unused;            /* RUs unused */
    u32 ru_punctured;         /* RUs punctured */
    u32 spatial_reuse;        /* Spatial reuse count */
    
    /* Performance */
    u32 efficiency;           /* Spectral efficiency */
    u32 throughput;           /* Achieved throughput */
    u32 latency;              /* Average latency */
    u32 retries;              /* Retry count */
    
    /* Errors */
    u32 trigger_fails;        /* Trigger failures */
    u32 feedback_fails;       /* Feedback failures */
    u32 timing_fails;         /* Timing failures */
    u32 power_fails;          /* Power control failures */
};

/* OFDMA device info */
struct wifi7_ofdma {
    /* Capabilities */
    u32 capabilities;         /* Supported features */
    u32 flags;                /* Enabled features */
    
    /* Configuration */
    u8 max_users;             /* Maximum users */
    u8 max_rus;               /* Maximum RUs */
    u8 min_ru_size;           /* Minimum RU size */
    u8 max_ru_size;           /* Maximum RU size */
    
    /* Resource allocation */
    struct wifi7_ofdma_alloc current_alloc;
    spinlock_t alloc_lock;
    
    /* Trigger frame generation */
    struct wifi7_ofdma_trigger trigger;
    spinlock_t trigger_lock;
    
    /* Statistics */
    struct wifi7_ofdma_stats stats;
    
    /* Work items */
    struct workqueue_struct *wq;
    struct delayed_work schedule_work;
    
    /* Debugging */
    struct dentry *debugfs_dir;
    bool debug_enabled;
};

/* Function prototypes */
int wifi7_ofdma_init(struct wifi7_dev *dev);
void wifi7_ofdma_deinit(struct wifi7_dev *dev);

int wifi7_ofdma_start(struct wifi7_dev *dev);
void wifi7_ofdma_stop(struct wifi7_dev *dev);

int wifi7_ofdma_alloc_ru(struct wifi7_dev *dev,
                        struct wifi7_ofdma_alloc *alloc);
void wifi7_ofdma_free_ru(struct wifi7_dev *dev,
                        struct wifi7_ofdma_alloc *alloc);

int wifi7_ofdma_add_user(struct wifi7_dev *dev,
                        struct wifi7_ofdma_user *user);
void wifi7_ofdma_del_user(struct wifi7_dev *dev,
                         u8 user_id);

int wifi7_ofdma_trigger_ul(struct wifi7_dev *dev,
                          struct wifi7_ofdma_trigger *trigger);
int wifi7_ofdma_trigger_dl(struct wifi7_dev *dev,
                          struct wifi7_ofdma_trigger *trigger);

int wifi7_ofdma_get_csi(struct wifi7_dev *dev,
                       struct wifi7_ofdma_csi *csi);
int wifi7_ofdma_set_csi(struct wifi7_dev *dev,
                       struct wifi7_ofdma_csi *csi);

int wifi7_ofdma_get_stats(struct wifi7_dev *dev,
                         struct wifi7_ofdma_stats *stats);
int wifi7_ofdma_clear_stats(struct wifi7_dev *dev);

#endif /* __WIFI7_OFDMA_H */ 