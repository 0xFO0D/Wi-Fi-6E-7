/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_POWER_H
#define __WIFI7_POWER_H

#include <linux/types.h>
#include <linux/thermal.h>
#include "wifi7_phy.h"

/* Power states */
#define WIFI7_POWER_STATE_ACTIVE    0
#define WIFI7_POWER_STATE_SLEEP     1
#define WIFI7_POWER_STATE_DEEP_SLEEP 2

/* Power limits and thresholds */
#define WIFI7_MAX_TX_POWER_DBM     30
#define WIFI7_MIN_TX_POWER_DBM     0
#define WIFI7_DEFAULT_TX_POWER_DBM 20

/* Temperature thresholds (in millicelsius) */
#define WIFI7_TEMP_NORMAL          65000
#define WIFI7_TEMP_WARNING         75000
#define WIFI7_TEMP_CRITICAL        85000
#define WIFI7_TEMP_SHUTDOWN        90000

/* Voltage and frequency scaling */
#define WIFI7_MAX_VOLTAGE_MV       1200
#define WIFI7_MIN_VOLTAGE_MV       800
#define WIFI7_MAX_FREQ_MHZ         2500
#define WIFI7_MIN_FREQ_MHZ         100

/* Calibration intervals */
#define WIFI7_CAL_INTERVAL_SHORT_MS  100
#define WIFI7_CAL_INTERVAL_NORMAL_MS 1000
#define WIFI7_CAL_INTERVAL_LONG_MS   10000

/* Calibration types */
enum wifi7_cal_type {
    WIFI7_CAL_AGC = 0,
    WIFI7_CAL_DC_OFFSET,
    WIFI7_CAL_IQ_IMBALANCE,
    WIFI7_CAL_PHASE_NOISE,
    WIFI7_CAL_TEMP_COMP,
    WIFI7_CAL_MAX
};

/* Power profile */
struct wifi7_power_profile {
    u8 state;                  /* Current power state */
    u16 voltage_mv;           /* Operating voltage */
    u16 frequency_mhz;        /* Operating frequency */
    u8 tx_chains_active;      /* Active TX chains */
    u8 rx_chains_active;      /* Active RX chains */
    bool sleep_capable;       /* Sleep mode support */
};

/* Calibration data */
struct wifi7_cal_data {
    enum wifi7_cal_type type;
    u32 last_cal_time;        /* jiffies */
    u32 interval_ms;          /* Calibration interval */
    bool in_progress;
    bool valid;
    
    /* Calibration results */
    union {
        struct {
            s16 dc_i;         /* I-path DC offset */
            s16 dc_q;         /* Q-path DC offset */
        } dc;
        
        struct {
            u16 gain;         /* AGC gain value */
            s16 offset;       /* AGC offset */
        } agc;
        
        struct {
            s16 phase;        /* Phase correction */
            u16 magnitude;    /* Magnitude correction */
        } iq;
        
        struct {
            u16 noise_floor;  /* Noise floor estimate */
            u16 phase_error;  /* Phase error estimate */
        } phase;
        
        struct {
            s16 temp_offset;  /* Temperature compensation */
            s16 gain_adjust;  /* Temperature-based gain adjustment */
        } temp;
    } results;
};

/* Chain-specific power control */
struct wifi7_chain_power {
    bool enabled;
    s8 current_power;         /* Current TX power in dBm */
    s8 max_power;            /* Maximum allowed power */
    u8 gain_index;           /* Current gain setting */
    
    /* Power tracking */
    struct {
        u32 samples;         /* Number of power samples */
        s32 avg_power;       /* Average power (in 0.1 dBm) */
        s32 peak_power;      /* Peak power observed */
        u32 overpower_count; /* Number of overpower events */
    } tracking;
};

/* Main power management context */
struct wifi7_power_context {
    struct wifi7_phy_dev *phy;
    struct thermal_zone_device *thermal_zone;
    
    /* Current state */
    struct wifi7_power_profile profile;
    int temperature;          /* Current temperature in millicelsius */
    atomic_t power_state;     /* Current power state */
    
    /* Chain management */
    struct wifi7_chain_power chains[WIFI7_MAX_TX_CHAINS];
    spinlock_t chain_lock;
    
    /* Calibration */
    struct wifi7_cal_data cal_data[WIFI7_CAL_MAX];
    struct workqueue_struct *cal_wq;
    struct delayed_work cal_work;
    spinlock_t cal_lock;
    
    /* Power monitoring */
    struct workqueue_struct *power_wq;
    struct delayed_work power_work;
    spinlock_t power_lock;
    
    /* Statistics */
    struct {
        u32 state_changes;
        u32 voltage_changes;
        u32 freq_changes;
        u32 temp_warnings;
        u32 temp_critical;
        u32 cal_attempts;
        u32 cal_failures;
        ktime_t last_state_change;
    } stats;
};

/* Function prototypes */
struct wifi7_power_context *wifi7_power_alloc(struct wifi7_phy_dev *phy);
void wifi7_power_free(struct wifi7_power_context *power);

/* Power state management */
int wifi7_power_set_state(struct wifi7_power_context *power, u8 state);
int wifi7_power_set_profile(struct wifi7_power_context *power,
                           const struct wifi7_power_profile *profile);

/* Chain control */
int wifi7_power_enable_chain(struct wifi7_power_context *power,
                            u8 chain_idx,
                            bool enable);
int wifi7_power_set_chain_power(struct wifi7_power_context *power,
                               u8 chain_idx,
                               s8 power_dbm);

/* Calibration control */
int wifi7_power_start_cal(struct wifi7_power_context *power,
                         enum wifi7_cal_type type);
int wifi7_power_get_cal_results(struct wifi7_power_context *power,
                               enum wifi7_cal_type type,
                               struct wifi7_cal_data *data);

/* Temperature management */
int wifi7_power_set_thermal_params(struct wifi7_power_context *power,
                                  int normal_temp,
                                  int warning_temp,
                                  int critical_temp,
                                  int shutdown_temp);
void wifi7_power_handle_thermal_event(struct wifi7_power_context *power,
                                    int temp);

/* Status and debug */
void wifi7_power_dump_stats(struct wifi7_power_context *power);
void wifi7_power_dump_cal(struct wifi7_power_context *power);

#endif /* __WIFI7_POWER_H */ 