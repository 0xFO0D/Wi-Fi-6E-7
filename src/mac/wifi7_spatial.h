/*
 * WiFi 7 Spatial Reuse
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 *
 * Spatial reuse functionality for WiFi 7 including:
 * - BSS coloring
 * - Spatial reuse parameter sets
 * - Non-SRG OBSS PD-based SR
 * - SRG OBSS PD-based SR
 * - PSR-based spatial reuse
 */

#ifndef __WIFI7_SPATIAL_H
#define __WIFI7_SPATIAL_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include "../core/wifi7_core.h"

/* Spatial reuse capabilities */
#define WIFI7_SR_CAP_BSS_COLOR      BIT(0)  /* BSS coloring */
#define WIFI7_SR_CAP_NON_SRG_OBSS   BIT(1)  /* Non-SRG OBSS PD SR */
#define WIFI7_SR_CAP_SRG_OBSS       BIT(2)  /* SRG OBSS PD SR */
#define WIFI7_SR_CAP_PSR            BIT(3)  /* PSR-based SR */
#define WIFI7_SR_CAP_HESIGA         BIT(4)  /* HE SIG-A SR */
#define WIFI7_SR_CAP_EHTVECTOR      BIT(5)  /* EHT SR vector */
#define WIFI7_SR_CAP_DYNAMIC        BIT(6)  /* Dynamic SR */
#define WIFI7_SR_CAP_ADAPTIVE       BIT(7)  /* Adaptive SR */
#define WIFI7_SR_CAP_MULTI_BSS      BIT(8)  /* Multi-BSS SR */
#define WIFI7_SR_CAP_SPATIAL_REUSE  BIT(9)  /* Spatial reuse */
#define WIFI7_SR_CAP_POWER_CONTROL  BIT(10) /* Power control */
#define WIFI7_SR_CAP_INTERFERENCE   BIT(11) /* Interference mgmt */

/* Maximum values */
#define WIFI7_SR_MAX_BSS_COLOR      63  /* Maximum BSS color */
#define WIFI7_SR_MAX_SRG_SIZE       64  /* Maximum SRG size */
#define WIFI7_SR_MAX_PSR_THRESH     8   /* Maximum PSR thresholds */
#define WIFI7_SR_MAX_OBSS_PD        20  /* Maximum OBSS PD level */
#define WIFI7_SR_MAX_TX_POWER       30  /* Maximum TX power (dBm) */
#define WIFI7_SR_MAX_INTERFERENCE   8   /* Maximum interference levels */

/* Spatial reuse flags */
#define WIFI7_SR_FLAG_BSS_COLOR     BIT(0)  /* BSS color enabled */
#define WIFI7_SR_FLAG_NON_SRG       BIT(1)  /* Non-SRG enabled */
#define WIFI7_SR_FLAG_SRG           BIT(2)  /* SRG enabled */
#define WIFI7_SR_FLAG_PSR           BIT(3)  /* PSR enabled */
#define WIFI7_SR_FLAG_HESIGA        BIT(4)  /* HE SIG-A enabled */
#define WIFI7_SR_FLAG_EHTVECTOR     BIT(5)  /* EHT vector enabled */
#define WIFI7_SR_FLAG_DYNAMIC       BIT(6)  /* Dynamic enabled */
#define WIFI7_SR_FLAG_ADAPTIVE      BIT(7)  /* Adaptive enabled */
#define WIFI7_SR_FLAG_MULTI_BSS     BIT(8)  /* Multi-BSS enabled */
#define WIFI7_SR_FLAG_POWER         BIT(9)  /* Power control enabled */
#define WIFI7_SR_FLAG_INTERFERENCE  BIT(10) /* Interference mgmt enabled */

/* BSS color info */
struct wifi7_sr_bss_color {
    u8 color;                   /* BSS color value */
    bool disabled;             /* Color disabled */
    bool partial;              /* Partial BSS color */
    u32 collision_count;       /* Collision count */
    ktime_t last_collision;    /* Last collision time */
};

/* SRG parameters */
struct wifi7_sr_srg {
    u8 obss_pd_min;           /* Minimum OBSS PD */
    u8 obss_pd_max;           /* Maximum OBSS PD */
    u8 bss_color_bitmap[8];   /* BSS color bitmap */
    u8 partial_bssid_bitmap[8]; /* Partial BSSID bitmap */
    bool hesiga_sr_disabled;   /* HE SIG-A SR disabled */
    u32 srg_obss_pd_min_offset; /* SRG OBSS PD min offset */
    u32 srg_obss_pd_max_offset; /* SRG OBSS PD max offset */
};

/* PSR parameters */
struct wifi7_sr_psr {
    u8 threshold;              /* PSR threshold */
    u8 margin;                /* PSR margin */
    u8 psr_spatial_reuse_value; /* PSR spatial reuse value */
    bool psr_disregard;       /* PSR disregard */
    u32 psr_reset_timeout;    /* PSR reset timeout */
};

/* Interference info */
struct wifi7_sr_interference {
    u8 level;                  /* Interference level */
    u8 type;                  /* Interference type */
    u8 source;                /* Interference source */
    u32 duration;             /* Duration in us */
    s8 rssi;                  /* RSSI value */
    u32 timestamp;            /* Detection timestamp */
};

/* Power control info */
struct wifi7_sr_power {
    s8 tx_power;              /* Transmit power */
    s8 rx_power;              /* Receive power */
    u8 pd_threshold;          /* PD threshold */
    u8 margin;                /* Power margin */
    bool power_boost;         /* Power boost enabled */
    u32 boost_timeout;        /* Power boost timeout */
};

/* Spatial reuse statistics */
struct wifi7_sr_stats {
    /* BSS color stats */
    u32 color_collisions;     /* Color collisions */
    u32 color_changes;        /* Color changes */
    
    /* OBSS PD stats */
    u32 obss_pd_opportunities; /* OBSS PD opportunities */
    u32 obss_pd_successes;    /* OBSS PD successes */
    u32 srg_opportunities;    /* SRG opportunities */
    u32 srg_successes;        /* SRG successes */
    
    /* PSR stats */
    u32 psr_opportunities;    /* PSR opportunities */
    u32 psr_successes;        /* PSR successes */
    u32 psr_failures;         /* PSR failures */
    
    /* Power control stats */
    u32 power_adjustments;    /* Power adjustments */
    u32 power_boosts;         /* Power boosts */
    u32 pd_adjustments;       /* PD adjustments */
    
    /* Interference stats */
    u32 interference_events;  /* Interference events */
    u32 interference_duration; /* Total interference duration */
    u32 interference_mitigations; /* Interference mitigations */
};

/* Spatial reuse device info */
struct wifi7_sr {
    /* Capabilities */
    u32 capabilities;         /* Supported features */
    u32 flags;                /* Enabled features */
    
    /* BSS color */
    struct wifi7_sr_bss_color bss_color;
    spinlock_t color_lock;
    
    /* SRG */
    struct wifi7_sr_srg srg;
    spinlock_t srg_lock;
    
    /* PSR */
    struct wifi7_sr_psr psr;
    spinlock_t psr_lock;
    
    /* Power control */
    struct wifi7_sr_power power;
    spinlock_t power_lock;
    
    /* Interference */
    struct wifi7_sr_interference interference;
    spinlock_t interference_lock;
    
    /* Statistics */
    struct wifi7_sr_stats stats;
    
    /* Work items */
    struct workqueue_struct *wq;
    struct delayed_work color_work;
    struct delayed_work srg_work;
    struct delayed_work psr_work;
    struct delayed_work power_work;
    
    /* Debugging */
    struct dentry *debugfs_dir;
    bool debug_enabled;
};

/* Function prototypes */
int wifi7_sr_init(struct wifi7_dev *dev);
void wifi7_sr_deinit(struct wifi7_dev *dev);

int wifi7_sr_start(struct wifi7_dev *dev);
void wifi7_sr_stop(struct wifi7_dev *dev);

int wifi7_sr_set_bss_color(struct wifi7_dev *dev,
                          struct wifi7_sr_bss_color *color);
int wifi7_sr_get_bss_color(struct wifi7_dev *dev,
                          struct wifi7_sr_bss_color *color);

int wifi7_sr_set_srg(struct wifi7_dev *dev,
                     struct wifi7_sr_srg *srg);
int wifi7_sr_get_srg(struct wifi7_dev *dev,
                     struct wifi7_sr_srg *srg);

int wifi7_sr_set_psr(struct wifi7_dev *dev,
                     struct wifi7_sr_psr *psr);
int wifi7_sr_get_psr(struct wifi7_dev *dev,
                     struct wifi7_sr_psr *psr);

int wifi7_sr_set_power(struct wifi7_dev *dev,
                      struct wifi7_sr_power *power);
int wifi7_sr_get_power(struct wifi7_dev *dev,
                      struct wifi7_sr_power *power);

int wifi7_sr_report_interference(struct wifi7_dev *dev,
                               struct wifi7_sr_interference *interference);

int wifi7_sr_get_stats(struct wifi7_dev *dev,
                      struct wifi7_sr_stats *stats);
int wifi7_sr_clear_stats(struct wifi7_dev *dev);

#endif /* __WIFI7_SPATIAL_H */ 