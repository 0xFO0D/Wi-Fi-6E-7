/*
 * WiFi 7 Firmware Loader
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_FIRMWARE_LOADER_H
#define __WIFI7_FIRMWARE_LOADER_H

#include <linux/types.h>
#include "../../core/wifi7_core.h"

/* Firmware version information */
struct wifi7_fw_version {
    u8 major;              /* Major version */
    u8 minor;              /* Minor version */
    u8 patch;              /* Patch level */
    u8 build;              /* Build number */
    char date[16];         /* Build date */
    char time[16];         /* Build time */
    char commit[40];       /* Git commit hash */
};

/* Firmware capabilities */
#define FW_CAP_BASIC       BIT(0)  /* Basic features */
#define FW_CAP_ADVANCED    BIT(1)  /* Advanced features */
#define FW_CAP_DEBUG       BIT(2)  /* Debug features */
#define FW_CAP_TEST        BIT(3)  /* Test features */
#define FW_CAP_SECURE      BIT(4)  /* Secure features */
#define FW_CAP_RECOVERY    BIT(5)  /* Recovery features */
#define FW_CAP_CUSTOM      BIT(6)  /* Custom features */
#define FW_CAP_ALL         0xFF    /* All features */

/* Firmware features */
struct wifi7_fw_features {
    u32 capabilities;      /* Firmware capabilities */
    u32 max_vifs;         /* Maximum number of VIFs */
    u32 max_stations;     /* Maximum number of stations */
    u32 max_keys;         /* Maximum number of keys */
    u32 max_beacons;      /* Maximum number of beacons */
    u32 max_rates;        /* Maximum number of rates */
    u32 max_channels;     /* Maximum number of channels */
    u32 max_power;        /* Maximum transmit power */
    u32 flags;            /* Feature flags */
};

/* Firmware configuration */
struct wifi7_fw_config {
    u32 log_level;        /* Log level */
    u32 debug_mask;       /* Debug mask */
    u32 test_mode;        /* Test mode */
    u32 recovery_mode;    /* Recovery mode */
    u32 secure_mode;      /* Secure mode */
    u32 custom_mode;      /* Custom mode */
    u32 flags;            /* Configuration flags */
};

/* Function prototypes */
int wifi7_load_firmware(struct wifi7_dev *dev);
void wifi7_firmware_complete(struct wifi7_dev *dev, int status);

int wifi7_get_fw_version(struct wifi7_dev *dev,
                        struct wifi7_fw_version *version);
int wifi7_get_fw_features(struct wifi7_dev *dev,
                         struct wifi7_fw_features *features);
int wifi7_set_fw_config(struct wifi7_dev *dev,
                       struct wifi7_fw_config *config);
int wifi7_get_fw_config(struct wifi7_dev *dev,
                       struct wifi7_fw_config *config);

/* Debug interface */
#ifdef CONFIG_WIFI7_FW_DEBUG
int wifi7_fw_debugfs_init(struct wifi7_dev *dev);
void wifi7_fw_debugfs_remove(struct wifi7_dev *dev);
#else
static inline int wifi7_fw_debugfs_init(struct wifi7_dev *dev) { return 0; }
static inline void wifi7_fw_debugfs_remove(struct wifi7_dev *dev) {}
#endif

#endif /* __WIFI7_FIRMWARE_LOADER_H */ 