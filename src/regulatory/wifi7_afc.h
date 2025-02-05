/*
 * WiFi 7 Automatic Frequency Coordination (AFC)
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_AFC_H
#define __WIFI7_AFC_H

#include <linux/types.h>
#include <linux/workqueue.h>
#include "../core/wifi7_core.h"

/* AFC operation modes */
#define WIFI7_AFC_MODE_DISABLED    0  /* AFC disabled */
#define WIFI7_AFC_MODE_STANDARD    1  /* Standard power operation */
#define WIFI7_AFC_MODE_LOW_POWER   2  /* Low power operation */
#define WIFI7_AFC_MODE_INDOOR      3  /* Indoor operation only */
#define WIFI7_AFC_MODE_OUTDOOR     4  /* Outdoor operation */
#define WIFI7_AFC_MODE_MOBILE      5  /* Mobile operation */

/* AFC capabilities */
#define WIFI7_AFC_CAP_STANDARD    BIT(0)  /* Standard power operation */
#define WIFI7_AFC_CAP_LOW_POWER   BIT(1)  /* Low power operation */
#define WIFI7_AFC_CAP_INDOOR      BIT(2)  /* Indoor operation */
#define WIFI7_AFC_CAP_OUTDOOR     BIT(3)  /* Outdoor operation */
#define WIFI7_AFC_CAP_MOBILE      BIT(4)  /* Mobile operation */
#define WIFI7_AFC_CAP_GPS         BIT(5)  /* GPS location support */
#define WIFI7_AFC_CAP_MULTI_CHAN  BIT(6)  /* Multi-channel support */
#define WIFI7_AFC_CAP_FAST_UPDATE BIT(7)  /* Fast update support */
#define WIFI7_AFC_CAP_REDUNDANCY  BIT(8)  /* Redundant server support */
#define WIFI7_AFC_CAP_CACHE       BIT(9)  /* Response caching */
#define WIFI7_AFC_CAP_PREDICTION  BIT(10) /* Spectrum prediction */
#define WIFI7_AFC_CAP_SHARING     BIT(11) /* Spectrum sharing */

/* AFC channel states */
#define WIFI7_AFC_CHAN_UNAVAILABLE 0  /* Channel unavailable */
#define WIFI7_AFC_CHAN_AVAILABLE   1  /* Channel available */
#define WIFI7_AFC_CHAN_RESTRICTED  2  /* Channel restricted */
#define WIFI7_AFC_CHAN_PENDING     3  /* Channel pending approval */
#define WIFI7_AFC_CHAN_ERROR       4  /* Channel error state */

/* AFC power limits (in dBm) */
#define WIFI7_AFC_POWER_MIN       -10
#define WIFI7_AFC_POWER_MAX        36
#define WIFI7_AFC_POWER_DEFAULT    24
#define WIFI7_AFC_POWER_LOW        17
#define WIFI7_AFC_POWER_INDOOR     24
#define WIFI7_AFC_POWER_OUTDOOR    36

/* AFC update intervals (in seconds) */
#define WIFI7_AFC_UPDATE_MIN       300   /* 5 minutes */
#define WIFI7_AFC_UPDATE_MAX       7200  /* 2 hours */
#define WIFI7_AFC_UPDATE_DEFAULT   1800  /* 30 minutes */
#define WIFI7_AFC_RETRY_MIN        60    /* 1 minute */
#define WIFI7_AFC_RETRY_MAX        600   /* 10 minutes */

/* AFC location accuracy (in meters) */
#define WIFI7_AFC_LOC_ACCURACY_MIN  10
#define WIFI7_AFC_LOC_ACCURACY_MAX  100
#define WIFI7_AFC_HEIGHT_MIN        0
#define WIFI7_AFC_HEIGHT_MAX        1000

/* AFC channel information */
struct wifi7_afc_channel {
    u32 frequency;           /* Channel frequency */
    u8 bandwidth;           /* Channel bandwidth */
    u8 state;               /* Channel state */
    s8 max_power;           /* Maximum allowed power */
    u32 expiry;            /* Availability expiry time */
    u32 restrictions;      /* Usage restrictions */
    bool indoor_only;      /* Indoor use only */
    bool mobile_allowed;   /* Mobile operation allowed */
};

/* AFC location information */
struct wifi7_afc_location {
    s32 latitude;          /* Latitude (microdegrees) */
    s32 longitude;         /* Longitude (microdegrees) */
    s32 height;            /* Height above ground (meters) */
    u32 accuracy;          /* Location accuracy (meters) */
    bool indoor;           /* Indoor location flag */
    bool mobile;           /* Mobile operation flag */
    u32 timestamp;         /* Location timestamp */
};

/* AFC configuration */
struct wifi7_afc_config {
    u8 mode;               /* Operation mode */
    u32 capabilities;      /* AFC capabilities */
    u32 update_interval;   /* Update interval */
    u32 retry_interval;    /* Retry interval */
    u32 max_channels;      /* Maximum channels */
    s8 max_power;          /* Maximum power */
    bool auto_update;      /* Automatic updates */
    bool cache_enabled;    /* Response caching */
    bool prediction;       /* Spectrum prediction */
    bool redundancy;       /* Server redundancy */
    char server_url[256];  /* AFC server URL */
    char api_key[64];      /* API key */
};

/* AFC statistics */
struct wifi7_afc_stats {
    u32 requests;          /* Total requests */
    u32 responses;         /* Total responses */
    u32 failures;          /* Request failures */
    u32 retries;          /* Request retries */
    u32 timeouts;         /* Request timeouts */
    u32 cache_hits;       /* Cache hits */
    u32 cache_misses;     /* Cache misses */
    u32 power_updates;    /* Power updates */
    u32 channel_updates;  /* Channel updates */
    u32 location_updates; /* Location updates */
    u32 prediction_hits;  /* Prediction hits */
    u32 prediction_misses;/* Prediction misses */
    u32 avg_response_time;/* Average response time */
    u32 last_update;      /* Last update timestamp */
};

/* Function prototypes */
int wifi7_afc_init(struct wifi7_dev *dev);
void wifi7_afc_deinit(struct wifi7_dev *dev);

int wifi7_afc_start(struct wifi7_dev *dev);
void wifi7_afc_stop(struct wifi7_dev *dev);

int wifi7_afc_set_config(struct wifi7_dev *dev,
                        struct wifi7_afc_config *config);
int wifi7_afc_get_config(struct wifi7_dev *dev,
                        struct wifi7_afc_config *config);

int wifi7_afc_set_location(struct wifi7_dev *dev,
                          struct wifi7_afc_location *location);
int wifi7_afc_get_location(struct wifi7_dev *dev,
                          struct wifi7_afc_location *location);

int wifi7_afc_request_channels(struct wifi7_dev *dev,
                             struct wifi7_afc_channel *channels,
                             u32 num_channels);
int wifi7_afc_get_channel_info(struct wifi7_dev *dev,
                              u32 frequency,
                              struct wifi7_afc_channel *channel);

int wifi7_afc_update_power(struct wifi7_dev *dev,
                          u32 frequency,
                          s8 max_power);
int wifi7_afc_get_max_power(struct wifi7_dev *dev,
                           u32 frequency,
                           s8 *max_power);

int wifi7_afc_get_stats(struct wifi7_dev *dev,
                       struct wifi7_afc_stats *stats);
int wifi7_afc_clear_stats(struct wifi7_dev *dev);

/* Debug interface */
#ifdef CONFIG_WIFI7_AFC_DEBUG
int wifi7_afc_debugfs_init(struct wifi7_dev *dev);
void wifi7_afc_debugfs_remove(struct wifi7_dev *dev);
#else
static inline int wifi7_afc_debugfs_init(struct wifi7_dev *dev) { return 0; }
static inline void wifi7_afc_debugfs_remove(struct wifi7_dev *dev) {}
#endif

#endif /* __WIFI7_AFC_H */ 