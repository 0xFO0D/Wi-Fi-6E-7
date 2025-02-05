/*
 * WiFi 7 Power Management
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 *
 * Power management functionality for WiFi 7 including:
 * - Power save modes (PSM, UAPSD, WMM-PS)
 * - Target wake time (TWT)
 * - Multi-link power save
 * - Dynamic power control
 */

#ifndef __WIFI7_POWER_H
#define __WIFI7_POWER_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include "../core/wifi7_core.h"

/* Power management capabilities */
#define WIFI7_PM_CAP_PSM           BIT(0)  /* Legacy power save */
#define WIFI7_PM_CAP_UAPSD        BIT(1)  /* U-APSD */
#define WIFI7_PM_CAP_WMM_PS       BIT(2)  /* WMM power save */
#define WIFI7_PM_CAP_TWT          BIT(3)  /* Target wake time */
#define WIFI7_PM_CAP_MLO_PS       BIT(4)  /* Multi-link PS */
#define WIFI7_PM_CAP_DYNAMIC      BIT(5)  /* Dynamic power control */
#define WIFI7_PM_CAP_ADAPTIVE     BIT(6)  /* Adaptive power save */
#define WIFI7_PM_CAP_DEEP_SLEEP   BIT(7)  /* Deep sleep */
#define WIFI7_PM_CAP_DOZE         BIT(8)  /* Doze mode */
#define WIFI7_PM_CAP_AWAKE        BIT(9)  /* Awake mode */
#define WIFI7_PM_CAP_POWER_SAVE   BIT(10) /* Power save */
#define WIFI7_PM_CAP_POWER_DOWN   BIT(11) /* Power down */

/* Maximum values */
#define WIFI7_PM_MAX_TWT_FLOWS    8   /* Maximum TWT flows */
#define WIFI7_PM_MAX_PS_QUEUES    8   /* Maximum PS queues */
#define WIFI7_PM_MAX_SLEEP_TIME   10000  /* Maximum sleep time (ms) */
#define WIFI7_PM_MAX_LISTEN_INT   10   /* Maximum listen interval */
#define WIFI7_PM_MAX_DTIM_PERIOD  10   /* Maximum DTIM period */
#define WIFI7_PM_MAX_RETRY        8    /* Maximum retry count */

/* Power management flags */
#define WIFI7_PM_FLAG_PSM         BIT(0)  /* Legacy PS enabled */
#define WIFI7_PM_FLAG_UAPSD      BIT(1)  /* U-APSD enabled */
#define WIFI7_PM_FLAG_WMM_PS     BIT(2)  /* WMM-PS enabled */
#define WIFI7_PM_FLAG_TWT        BIT(3)  /* TWT enabled */
#define WIFI7_PM_FLAG_MLO_PS     BIT(4)  /* MLO PS enabled */
#define WIFI7_PM_FLAG_DYNAMIC    BIT(5)  /* Dynamic enabled */
#define WIFI7_PM_FLAG_ADAPTIVE   BIT(6)  /* Adaptive enabled */
#define WIFI7_PM_FLAG_DEEP_SLEEP BIT(7)  /* Deep sleep enabled */
#define WIFI7_PM_FLAG_DOZE       BIT(8)  /* Doze enabled */
#define WIFI7_PM_FLAG_AWAKE      BIT(9)  /* Awake enabled */
#define WIFI7_PM_FLAG_POWER_SAVE BIT(10) /* Power save enabled */
#define WIFI7_PM_FLAG_POWER_DOWN BIT(11) /* Power down enabled */

/* Power states */
#define WIFI7_PM_STATE_AWAKE      0  /* Fully awake */
#define WIFI7_PM_STATE_DOZE       1  /* Doze state */
#define WIFI7_PM_STATE_SLEEP      2  /* Sleep state */
#define WIFI7_PM_STATE_DEEP_SLEEP 3  /* Deep sleep */
#define WIFI7_PM_STATE_POWER_DOWN 4  /* Powered down */

/* TWT flow info */
struct wifi7_pm_twt_flow {
    u8 flow_id;                /* Flow identifier */
    u8 negotiation_type;      /* Negotiation type */
    u8 wake_interval_exp;     /* Wake interval exponent */
    u16 wake_interval_mantissa; /* Wake interval mantissa */
    u8 protection;            /* Protection required */
    u8 implicit;              /* Implicit TWT */
    u8 flow_type;            /* Flow type */
    u8 trigger_type;         /* Trigger type */
    u32 target_wake_time;    /* Target wake time */
    u16 wake_duration;       /* Wake duration */
    bool active;             /* Flow active */
    u32 flags;               /* Flow flags */
};

/* Power save queue info */
struct wifi7_pm_queue {
    u8 queue_id;             /* Queue identifier */
    u8 tid;                  /* Traffic ID */
    u8 ac;                   /* Access category */
    bool uapsd;              /* U-APSD enabled */
    bool wmm_ps;             /* WMM-PS enabled */
    u16 service_period;      /* Service period */
    u16 max_sp_length;       /* Max SP length */
    u32 trigger_enabled;     /* Trigger enabled */
    struct sk_buff_head skb_queue; /* Frame queue */
    spinlock_t lock;         /* Queue lock */
};

/* Power timing info */
struct wifi7_pm_timing {
    u32 beacon_interval;     /* Beacon interval */
    u32 dtim_period;        /* DTIM period */
    u32 listen_interval;     /* Listen interval */
    u32 awake_window;       /* Awake window */
    u32 sleep_duration;     /* Sleep duration */
    u32 transition_time;    /* State transition time */
    ktime_t last_beacon;    /* Last beacon time */
    ktime_t next_beacon;    /* Next beacon time */
    ktime_t last_activity;  /* Last activity time */
};

/* Power consumption info */
struct wifi7_pm_power {
    u32 current_power;       /* Current power level */
    u32 target_power;       /* Target power level */
    u32 min_power;          /* Minimum power level */
    u32 max_power;          /* Maximum power level */
    u32 power_save;         /* Power save level */
    u32 power_threshold;    /* Power threshold */
    bool power_save_enabled; /* Power save enabled */
    u32 power_mode;         /* Power mode */
};

/* Power statistics */
struct wifi7_pm_stats {
    /* State transitions */
    u32 awake_count;        /* Awake transitions */
    u32 doze_count;         /* Doze transitions */
    u32 sleep_count;        /* Sleep transitions */
    u32 deep_sleep_count;   /* Deep sleep transitions */
    
    /* Timing */
    u32 awake_time;         /* Total awake time */
    u32 doze_time;          /* Total doze time */
    u32 sleep_time;         /* Total sleep time */
    u32 deep_sleep_time;    /* Total deep sleep time */
    
    /* Power */
    u32 power_consumption;  /* Power consumption */
    u32 power_saved;        /* Power saved */
    u32 power_transitions;  /* Power transitions */
    
    /* TWT */
    u32 twt_sessions;       /* TWT sessions */
    u32 twt_suspensions;    /* TWT suspensions */
    u32 twt_resumptions;    /* TWT resumptions */
    
    /* Errors */
    u32 beacon_misses;      /* Beacon misses */
    u32 ps_failures;        /* PS failures */
    u32 transition_failures; /* Transition failures */
};

/* Power management device info */
struct wifi7_pm {
    /* Capabilities */
    u32 capabilities;       /* Supported features */
    u32 flags;              /* Enabled features */
    
    /* State */
    u8 state;               /* Current power state */
    u8 target_state;        /* Target power state */
    bool ps_enabled;        /* Power save enabled */
    spinlock_t state_lock;  /* State lock */
    
    /* TWT flows */
    struct wifi7_pm_twt_flow twt_flows[WIFI7_PM_MAX_TWT_FLOWS];
    u8 num_twt_flows;       /* Number of TWT flows */
    spinlock_t twt_lock;    /* TWT lock */
    
    /* PS queues */
    struct wifi7_pm_queue queues[WIFI7_PM_MAX_PS_QUEUES];
    u8 num_queues;          /* Number of queues */
    spinlock_t queue_lock;  /* Queue lock */
    
    /* Timing */
    struct wifi7_pm_timing timing;
    spinlock_t timing_lock; /* Timing lock */
    
    /* Power */
    struct wifi7_pm_power power;
    spinlock_t power_lock;  /* Power lock */
    
    /* Statistics */
    struct wifi7_pm_stats stats;
    
    /* Work items */
    struct workqueue_struct *wq;
    struct delayed_work ps_work;
    struct delayed_work twt_work;
    struct delayed_work monitor_work;
    
    /* Debugging */
    struct dentry *debugfs_dir;
    bool debug_enabled;
};

/* Function prototypes */
int wifi7_pm_init(struct wifi7_dev *dev);
void wifi7_pm_deinit(struct wifi7_dev *dev);

int wifi7_pm_start(struct wifi7_dev *dev);
void wifi7_pm_stop(struct wifi7_dev *dev);

int wifi7_pm_set_state(struct wifi7_dev *dev, u8 state);
int wifi7_pm_get_state(struct wifi7_dev *dev, u8 *state);

int wifi7_pm_add_twt_flow(struct wifi7_dev *dev,
                         struct wifi7_pm_twt_flow *flow);
int wifi7_pm_del_twt_flow(struct wifi7_dev *dev, u8 flow_id);

int wifi7_pm_queue_init(struct wifi7_dev *dev, u8 queue_id);
void wifi7_pm_queue_deinit(struct wifi7_dev *dev, u8 queue_id);

int wifi7_pm_set_timing(struct wifi7_dev *dev,
                       struct wifi7_pm_timing *timing);
int wifi7_pm_get_timing(struct wifi7_dev *dev,
                       struct wifi7_pm_timing *timing);

int wifi7_pm_set_power(struct wifi7_dev *dev,
                      struct wifi7_pm_power *power);
int wifi7_pm_get_power(struct wifi7_dev *dev,
                      struct wifi7_pm_power *power);

int wifi7_pm_get_stats(struct wifi7_dev *dev,
                      struct wifi7_pm_stats *stats);
int wifi7_pm_clear_stats(struct wifi7_dev *dev);

#endif /* __WIFI7_POWER_H */ 