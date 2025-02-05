/*
 * WiFi 7 TP-Link Router Support
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_TPLINK_H
#define __WIFI7_TPLINK_H

#include <linux/types.h>
#include "../../core/wifi7_core.h"

/* TP-Link router models */
#define TPLINK_MODEL_AXE300      0x0001  /* Archer AXE300 */
#define TPLINK_MODEL_AXE75       0x0002  /* Archer AXE75 */
#define TPLINK_MODEL_BE800       0x0003  /* Archer BE800 */
#define TPLINK_MODEL_BE900       0x0004  /* Archer BE900 */
#define TPLINK_MODEL_GX90       0x0005  /* GX90 Gaming Router */
#define TPLINK_MODEL_AXE200     0x0006  /* Archer AXE200 */
#define TPLINK_MODEL_AXE500     0x0007  /* Archer AXE500 */

/* Hardware capabilities */
#define TPLINK_CAP_320MHZ       BIT(0)  /* 320MHz channels */
#define TPLINK_CAP_4K_QAM       BIT(1)  /* 4K-QAM */
#define TPLINK_CAP_16_SS        BIT(2)  /* 16 spatial streams */
#define TPLINK_CAP_MLO          BIT(3)  /* Multi-link operation */
#define TPLINK_CAP_EHT_MU       BIT(4)  /* EHT MU-MIMO */
#define TPLINK_CAP_AFC          BIT(5)  /* AFC support */
#define TPLINK_CAP_MESH         BIT(6)  /* Mesh networking */
#define TPLINK_CAP_GAMING       BIT(7)  /* Gaming features */
#define TPLINK_CAP_AI           BIT(8)  /* AI optimization */
#define TPLINK_CAP_IOT          BIT(9)  /* IoT support */
#define TPLINK_CAP_VPN          BIT(10) /* VPN acceleration */
#define TPLINK_CAP_QOS          BIT(11) /* Advanced QoS */
#define TPLINK_CAP_SECURITY     BIT(12) /* Enhanced security */

/* Router configuration */
struct wifi7_tplink_config {
    u16 model;                  /* Router model */
    u32 capabilities;           /* Hardware capabilities */
    u8 num_radios;             /* Number of radios */
    u8 max_spatial_streams;     /* Maximum spatial streams */
    u8 max_bandwidth;          /* Maximum bandwidth */
    bool afc_enabled;          /* AFC enabled */
    bool mesh_enabled;         /* Mesh enabled */
    bool gaming_mode;          /* Gaming mode */
    bool ai_optimization;      /* AI optimization */
    struct {
        u8 band;              /* Frequency band */
        u16 max_power;        /* Maximum TX power */
        u8 antenna_config;    /* Antenna configuration */
        u8 spatial_streams;   /* Number of spatial streams */
    } radio_config[4];        /* Per-radio configuration */
};

/* Router statistics */
struct wifi7_tplink_stats {
    u32 tx_packets;            /* Transmitted packets */
    u32 rx_packets;            /* Received packets */
    u32 tx_bytes;              /* Transmitted bytes */
    u32 rx_bytes;              /* Received bytes */
    u32 tx_errors;             /* Transmission errors */
    u32 rx_errors;             /* Reception errors */
    u32 tx_dropped;            /* Dropped TX packets */
    u32 rx_dropped;            /* Dropped RX packets */
    u32 clients;               /* Connected clients */
    u32 interference;          /* Interference level */
    u32 noise_floor;           /* Noise floor */
    u32 channel_utilization;   /* Channel utilization */
    struct {
        u32 tx_power;         /* Current TX power */
        u32 temperature;      /* Radio temperature */
        u32 phy_errors;       /* PHY errors */
        u32 crc_errors;       /* CRC errors */
        u32 retry_count;      /* Retry count */
    } radio_stats[4];         /* Per-radio statistics */
};

/* Function prototypes */
int wifi7_tplink_init(struct wifi7_dev *dev);
void wifi7_tplink_deinit(struct wifi7_dev *dev);

int wifi7_tplink_probe(struct wifi7_dev *dev);
int wifi7_tplink_remove(struct wifi7_dev *dev);

int wifi7_tplink_start(struct wifi7_dev *dev);
void wifi7_tplink_stop(struct wifi7_dev *dev);

int wifi7_tplink_set_config(struct wifi7_dev *dev,
                           struct wifi7_tplink_config *config);
int wifi7_tplink_get_config(struct wifi7_dev *dev,
                           struct wifi7_tplink_config *config);

int wifi7_tplink_get_stats(struct wifi7_dev *dev,
                          struct wifi7_tplink_stats *stats);
int wifi7_tplink_clear_stats(struct wifi7_dev *dev);

int wifi7_tplink_set_radio_config(struct wifi7_dev *dev, u8 radio_id,
                                 struct wifi7_tplink_radio_config *config);
int wifi7_tplink_get_radio_config(struct wifi7_dev *dev, u8 radio_id,
                                 struct wifi7_tplink_radio_config *config);

int wifi7_tplink_set_gaming_mode(struct wifi7_dev *dev, bool enable);
int wifi7_tplink_set_mesh_mode(struct wifi7_dev *dev, bool enable);
int wifi7_tplink_set_ai_optimization(struct wifi7_dev *dev, bool enable);

/* Debug interface */
#ifdef CONFIG_WIFI7_TPLINK_DEBUG
int wifi7_tplink_debugfs_init(struct wifi7_dev *dev);
void wifi7_tplink_debugfs_remove(struct wifi7_dev *dev);
#else
static inline int wifi7_tplink_debugfs_init(struct wifi7_dev *dev) { return 0; }
static inline void wifi7_tplink_debugfs_remove(struct wifi7_dev *dev) {}
#endif

#endif /* __WIFI7_TPLINK_H */ 