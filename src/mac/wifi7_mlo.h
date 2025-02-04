/*
 * WiFi 7 Multi-Link Operation Management
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_MLO_H
#define __WIFI7_MLO_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include "../core/wifi7_core.h"

/* MLO capabilities */
#define WIFI7_MLO_CAP_EMLSR     BIT(0)  /* Enhanced multi-link single radio */
#define WIFI7_MLO_CAP_EMLMR     BIT(1)  /* Enhanced multi-link multi radio */
#define WIFI7_MLO_CAP_TID_4     BIT(2)  /* Support for 4 TIDs */
#define WIFI7_MLO_CAP_TID_8     BIT(3)  /* Support for 8 TIDs */
#define WIFI7_MLO_CAP_LINK_4    BIT(4)  /* Support for 4 links */
#define WIFI7_MLO_CAP_LINK_8    BIT(5)  /* Support for 8 links */
#define WIFI7_MLO_CAP_LINK_16   BIT(6)  /* Support for 16 links */
#define WIFI7_MLO_CAP_ASYNC     BIT(7)  /* Asynchronous operation */
#define WIFI7_MLO_CAP_SYNC      BIT(8)  /* Synchronous operation */
#define WIFI7_MLO_CAP_SWITCH    BIT(9)  /* Fast link switching */
#define WIFI7_MLO_CAP_AGG       BIT(10) /* Cross-link aggregation */
#define WIFI7_MLO_CAP_REORDER   BIT(11) /* Cross-link reordering */

/* MLO operation modes */
#define WIFI7_MLO_MODE_DISABLED  0  /* MLO disabled */
#define WIFI7_MLO_MODE_EMLSR     1  /* EMLSR mode */
#define WIFI7_MLO_MODE_EMLMR     2  /* EMLMR mode */
#define WIFI7_MLO_MODE_ASYNC     3  /* Asynchronous mode */
#define WIFI7_MLO_MODE_SYNC      4  /* Synchronous mode */

/* MLO link selection policies */
#define WIFI7_MLO_SELECT_RSSI    0  /* RSSI-based selection */
#define WIFI7_MLO_SELECT_LOAD    1  /* Load-based selection */
#define WIFI7_MLO_SELECT_BW      2  /* Bandwidth-based selection */
#define WIFI7_MLO_SELECT_LAT     3  /* Latency-based selection */
#define WIFI7_MLO_SELECT_ML      4  /* Machine learning based */
#define WIFI7_MLO_SELECT_CUSTOM  5  /* Custom selection policy */

/* MLO link states */
#define WIFI7_MLO_LINK_DOWN      0  /* Link is down */
#define WIFI7_MLO_LINK_SCANNING  1  /* Link is scanning */
#define WIFI7_MLO_LINK_JOINING   2  /* Link is joining */
#define WIFI7_MLO_LINK_AUTH      3  /* Link is authenticating */
#define WIFI7_MLO_LINK_ASSOC     4  /* Link is associating */
#define WIFI7_MLO_LINK_UP        5  /* Link is up */
#define WIFI7_MLO_LINK_PS        6  /* Link is in power save */

/* MLO TID-to-link mapping */
struct wifi7_mlo_tid_map {
    u8 tid;                    /* Traffic ID */
    u8 primary_link;          /* Primary link ID */
    u8 secondary_link;        /* Secondary link ID */
    u32 link_mask;            /* Allowed links mask */
    bool redundant;           /* Redundant transmission */
    bool aggregation;         /* Allow cross-link aggregation */
};

/* MLO link metrics */
struct wifi7_mlo_metrics {
    u32 rssi;                 /* Signal strength */
    u32 noise;                /* Noise level */
    u32 snr;                  /* Signal-to-noise ratio */
    u32 tx_rate;             /* TX rate in Mbps */
    u32 rx_rate;             /* RX rate in Mbps */
    u32 tx_bytes;            /* TX bytes */
    u32 rx_bytes;            /* RX bytes */
    u32 tx_packets;          /* TX packets */
    u32 rx_packets;          /* RX packets */
    u32 retries;             /* Retry count */
    u32 failures;            /* Failure count */
    u32 airtime;             /* Airtime utilization */
    u32 latency;             /* Link latency */
    u32 jitter;              /* Link jitter */
    u32 loss;                /* Packet loss rate */
};

/* MLO link configuration */
struct wifi7_mlo_link_config {
    u8 link_id;              /* Link identifier */
    u8 band;                 /* Frequency band */
    u16 center_freq;         /* Center frequency */
    u8 width;                /* Channel width */
    u8 primary_chan;         /* Primary channel */
    u8 nss;                  /* Number of spatial streams */
    u8 mcs;                  /* MCS index */
    u32 capabilities;        /* Link capabilities */
    bool enabled;            /* Link enabled */
};

/* MLO device configuration */
struct wifi7_mlo_config {
    u8 mode;                 /* Operation mode */
    u8 num_links;           /* Number of links */
    u8 active_links;        /* Number of active links */
    u8 selection_policy;    /* Link selection policy */
    u32 capabilities;       /* MLO capabilities */
    bool power_save;        /* Power save enabled */
    bool spatial_reuse;     /* Spatial reuse enabled */
    struct wifi7_mlo_link_config links[WIFI7_MAX_LINKS];
};

/* MLO statistics */
struct wifi7_mlo_stats {
    u32 link_switches;      /* Number of link switches */
    u32 link_failures;      /* Number of link failures */
    u32 tid_switches;       /* Number of TID switches */
    u32 aggregated_frames;  /* Number of aggregated frames */
    u32 reordered_frames;   /* Number of reordered frames */
    u32 dropped_frames;     /* Number of dropped frames */
    u32 duplicate_frames;   /* Number of duplicate frames */
    u32 out_of_order;      /* Number of out-of-order frames */
    u32 switch_latency;     /* Average switch latency */
    u32 setup_latency;      /* Average setup latency */
    struct wifi7_mlo_metrics link_metrics[WIFI7_MAX_LINKS];
};

/* Function prototypes */
int wifi7_mlo_init(struct wifi7_dev *dev);
void wifi7_mlo_deinit(struct wifi7_dev *dev);

int wifi7_mlo_start(struct wifi7_dev *dev);
void wifi7_mlo_stop(struct wifi7_dev *dev);

int wifi7_mlo_set_config(struct wifi7_dev *dev,
                        struct wifi7_mlo_config *config);
int wifi7_mlo_get_config(struct wifi7_dev *dev,
                        struct wifi7_mlo_config *config);

int wifi7_mlo_add_link(struct wifi7_dev *dev,
                      struct wifi7_mlo_link_config *link);
int wifi7_mlo_del_link(struct wifi7_dev *dev, u8 link_id);

int wifi7_mlo_set_tid_map(struct wifi7_dev *dev,
                         struct wifi7_mlo_tid_map *map);
int wifi7_mlo_get_tid_map(struct wifi7_dev *dev,
                         struct wifi7_mlo_tid_map *map);

int wifi7_mlo_switch_link(struct wifi7_dev *dev, u8 link_id);
int wifi7_mlo_get_metrics(struct wifi7_dev *dev,
                         struct wifi7_mlo_metrics *metrics);

int wifi7_mlo_get_stats(struct wifi7_dev *dev,
                       struct wifi7_mlo_stats *stats);
int wifi7_mlo_clear_stats(struct wifi7_dev *dev);

#endif /* __WIFI7_MLO_H */ 