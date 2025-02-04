#ifndef __WIFI7_POWER_H
#define __WIFI7_POWER_H

#include <linux/types.h>
#include <linux/thermal.h>
#include <linux/power_supply.h>
#include "../core/wifi7_core.h"

/* Power states - extensible for future use */
#define WIFI7_POWER_STATE_D0     0  /* Full power */
#define WIFI7_POWER_STATE_D1     1  /* Light sleep */
#define WIFI7_POWER_STATE_D2     2  /* Deep sleep */
#define WIFI7_POWER_STATE_D3     3  /* Off */
#define WIFI7_POWER_STATE_D4     4  /* Hibernate */
#define WIFI7_POWER_STATE_D5     5  /* Reserved */
#define WIFI7_POWER_STATE_D6     6  /* Reserved */
#define WIFI7_POWER_STATE_D7     7  /* Reserved */

/* Power domains */
#define WIFI7_POWER_DOMAIN_CORE  BIT(0)
#define WIFI7_POWER_DOMAIN_RF    BIT(1)
#define WIFI7_POWER_DOMAIN_BB    BIT(2)
#define WIFI7_POWER_DOMAIN_MAC   BIT(3)
#define WIFI7_POWER_DOMAIN_PLL   BIT(4)
#define WIFI7_POWER_DOMAIN_XTAL  BIT(5)
#define WIFI7_POWER_DOMAIN_ADC   BIT(6)
#define WIFI7_POWER_DOMAIN_DAC   BIT(7)
#define WIFI7_POWER_DOMAIN_PA    BIT(8)
#define WIFI7_POWER_DOMAIN_LNA   BIT(9)
#define WIFI7_POWER_DOMAIN_ALL   0x3FF

/* Thermal zones */
#define WIFI7_THERMAL_ZONE_RF    0
#define WIFI7_THERMAL_ZONE_BB    1
#define WIFI7_THERMAL_ZONE_PA    2
#define WIFI7_THERMAL_ZONE_MAX   3

/* Thermal thresholds in millicelsius */
#define WIFI7_TEMP_NORMAL        65000
#define WIFI7_TEMP_WARNING       85000
#define WIFI7_TEMP_CRITICAL      95000
#define WIFI7_TEMP_EMERGENCY     105000

/* Power profiles */
#define WIFI7_POWER_PROFILE_MAX_PERF     0
#define WIFI7_POWER_PROFILE_BALANCED     1
#define WIFI7_POWER_PROFILE_POWER_SAVE   2
#define WIFI7_POWER_PROFILE_ULTRA_SAVE   3
#define WIFI7_POWER_PROFILE_CUSTOM       4
#define WIFI7_POWER_PROFILE_MAX          5

/* DVFS operating points */
struct wifi7_dvfs_point {
    u32 freq_mhz;
    u32 voltage_mv;
    u32 current_ma;
    u32 temp_max;
    u32 power_mw;
};

/* Thermal sensor data */
struct wifi7_thermal_sensor {
    s32 temp;
    u32 update_time;
    u32 avg_temp;
    u32 max_temp;
    u32 min_temp;
    u32 trip_count;
    bool valid;
};

/* Power domain state */
struct wifi7_power_domain {
    u32 domain_mask;
    u32 voltage_mv;
    u32 current_ma;
    u32 power_mw;
    bool enabled;
    u32 transition_latency_us;
    u32 last_active_time;
    u32 total_active_time;
    u32 total_sleep_time;
};

/* Power profile configuration */
struct wifi7_power_profile {
    u8 profile_id;
    u32 max_freq_mhz;
    u32 min_freq_mhz;
    u32 target_temp;
    u32 power_limit_mw;
    u32 idle_timeout_ms;
    u32 sleep_timeout_ms;
    bool dynamic_freq;
    bool dynamic_voltage;
    bool thermal_throttle;
};

/* Power statistics and history */
struct wifi7_power_stats {
    u64 total_on_time_ms;
    u64 total_sleep_time_ms;
    u64 total_energy_mj;
    u32 avg_power_mw;
    u32 peak_power_mw;
    u32 temp_violations;
    u32 thermal_throttles;
    u32 voltage_drops;
    u32 current_spikes;
    u32 emergency_shutdowns;
};

/* Main power management structure */
struct wifi7_power {
    /* Current state */
    u8 power_state;
    u8 active_profile;
    u32 current_freq;
    u32 current_voltage;
    u32 current_power;
    
    /* Domain management */
    struct wifi7_power_domain domains[10];
    u32 active_domains;
    spinlock_t domain_lock;
    
    /* Thermal management */
    struct wifi7_thermal_sensor sensors[WIFI7_THERMAL_ZONE_MAX];
    struct thermal_zone_device *tzd[WIFI7_THERMAL_ZONE_MAX];
    struct thermal_cooling_device *cdev;
    u32 thermal_state;
    spinlock_t thermal_lock;
    
    /* DVFS management */
    struct wifi7_dvfs_point *dvfs_table;
    u32 n_dvfs_points;
    u32 current_dvfs_point;
    struct workqueue_struct *dvfs_wq;
    struct delayed_work dvfs_work;
    spinlock_t dvfs_lock;
    
    /* Profile management */
    struct wifi7_power_profile profiles[WIFI7_POWER_PROFILE_MAX];
    struct mutex profile_lock;
    
    /* Statistics and monitoring */
    struct wifi7_power_stats stats;
    struct delayed_work stats_work;
    spinlock_t stats_lock;
    
    /* Platform integration */
    struct power_supply *psy;
    struct notifier_block psy_nb;
    bool battery_present;
    int battery_capacity;
    
    /* Power saving features */
    bool ps_enabled;
    bool deep_sleep_enabled;
    bool ultra_sleep_enabled;
    u32 beacon_interval;
    u32 dtim_period;
    u32 listen_interval;
    
    /* Debugging */
    struct dentry *debugfs_dir;
    bool debug_enabled;
};

/* Function prototypes */
int wifi7_power_init(struct wifi7_dev *dev);
void wifi7_power_deinit(struct wifi7_dev *dev);

int wifi7_power_set_state(struct wifi7_dev *dev, u8 state);
int wifi7_power_set_profile(struct wifi7_dev *dev, u8 profile);
int wifi7_power_set_domain(struct wifi7_dev *dev, u32 domain, bool enable);

int wifi7_power_get_temperature(struct wifi7_dev *dev, u8 zone, s32 *temp);
int wifi7_power_get_stats(struct wifi7_dev *dev, struct wifi7_power_stats *stats);

int wifi7_power_enable_ps(struct wifi7_dev *dev, bool enable);
int wifi7_power_set_beacon_interval(struct wifi7_dev *dev, u32 interval);
int wifi7_power_set_dtim_period(struct wifi7_dev *dev, u32 period);

/* TODO: Implement ML-based thermal prediction */
int wifi7_power_predict_thermal(struct wifi7_dev *dev, u8 zone, 
                              s32 *predicted_temp);

/* TODO: Add support for additional PMIC interfaces */
int wifi7_power_register_pmic(struct wifi7_dev *dev, void *pmic_data);

/* TODO: Optimize wake-up latency for MLO devices */
int wifi7_power_set_wakeup_latency(struct wifi7_dev *dev, u32 latency_us);

#endif /* __WIFI7_POWER_H */ 