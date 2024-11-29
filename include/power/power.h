#ifndef _WIFI67_POWER_H_
#define _WIFI67_POWER_H_

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>

/* Power states */
enum wifi67_power_state {
    WIFI67_POWER_STATE_OFF,
    WIFI67_POWER_STATE_ON,
    WIFI67_POWER_STATE_SLEEP,
    WIFI67_POWER_STATE_DEEP_SLEEP
};

/* Power management modes */
enum wifi67_power_mode {
    WIFI67_POWER_MODE_CAM,      /* Constantly Awake Mode */
    WIFI67_POWER_MODE_PSM,      /* Power Save Mode */
    WIFI67_POWER_MODE_UAPSD,    /* U-APSD Mode */
    WIFI67_POWER_MODE_WMM_PS    /* WMM Power Save */
};

/* Power management configuration */
struct wifi67_power_config {
    enum wifi67_power_mode mode;
    u32 sleep_period;           /* Sleep duration in ms */
    u32 listen_interval;        /* Listen interval in beacon intervals */
    bool dynamic_ps;            /* Dynamic power save enable */
    u32 dynamic_ps_timeout;     /* Timeout before entering power save (ms) */
    bool smart_ps;              /* Smart power save enable */
    u32 beacon_timeout;         /* Beacon timeout in ms */
    u32 rx_wake_timeout;        /* RX wake timeout in ms */
    bool proxy_arp;             /* Proxy ARP enable */
};

/* Power management statistics */
struct wifi67_power_stats {
    atomic_t sleep_count;
    atomic_t wake_count;
    u32 total_sleep_time;
    u32 total_wake_time;
    u32 last_sleep_duration;
    u32 last_wake_duration;
    u32 ps_timeouts;
    u32 beacon_timeouts;
};

/* Main power management structure */
struct wifi67_power_mgmt {
    struct delayed_work ps_work;
    spinlock_t lock;
    
    enum wifi67_power_state state;
    struct wifi67_power_config config;
    struct wifi67_power_stats stats;
    
    bool ps_enabled;
    bool initialized;
    ktime_t last_state_change;
};

/* Thermal thresholds */
#define WIFI67_THERMAL_WARNING   75
#define WIFI67_THERMAL_THROTTLE  85
#define WIFI67_THERMAL_CRITICAL  95

struct wifi67_dvfs_state {
    u32 current_freq;
    u32 current_voltage;
    u32 temperature;
    atomic_t active_clients;
};

struct wifi67_thermal_stats {
    u32 current_temp;
    u32 max_temp;
    u32 throttle_events;
    u32 emergency_shutdowns;
};

int wifi67_power_init(struct wifi67_priv *priv);
void wifi67_power_deinit(struct wifi67_priv *priv);
int wifi67_power_configure(struct wifi67_priv *priv, struct wifi67_power_config *config);
int wifi67_power_set_mode(struct wifi67_priv *priv, enum wifi67_power_mode mode);
int wifi67_power_sleep(struct wifi67_priv *priv);
int wifi67_power_wake(struct wifi67_priv *priv);
void wifi67_power_get_stats(struct wifi67_priv *priv, struct wifi67_power_stats *stats);
int wifi67_power_dvfs_init(struct wifi67_priv *priv);
int wifi67_power_set_frequency(struct wifi67_priv *priv, u32 freq);
void wifi67_hw_diag_thermal(struct wifi67_priv *priv);

#endif /* _WIFI67_POWER_H_ */ 