#ifndef _WIFI67_REG_CORE_H_
#define _WIFI67_REG_CORE_H_

#include <linux/types.h>
#include <linux/workqueue.h>
#include <net/cfg80211.h>
#include "../core/wifi67_forward.h"

/* Helper macros */
#define MHZ_TO_KHZ(freq) ((freq) * 1000)
#define DBM_TO_MBM(gain) ((gain) * 100)

#define WIFI67_MAX_RULES 32

struct wifi67_reg_rule {
    u32 start_freq;
    u32 end_freq;
    u32 max_power;
    bool dfs_required;
};

struct wifi67_regulatory {
    struct wifi67_reg_rule rules[WIFI67_MAX_RULES];
    int n_rules;
    
    /* DFS related */
    bool dfs_enabled;
    struct delayed_work dfs_work;
    struct cfg80211_chan_def dfs_chan_def;
    bool cac_started;
    u32 cac_time_ms;
};

/* Function declarations */
int wifi67_regulatory_init(struct wifi67_priv *priv);
void wifi67_regulatory_deinit(struct wifi67_priv *priv);
void wifi67_reg_notifier(struct wiphy *wiphy,
                        struct regulatory_request *request);
int wifi67_reg_set_power(struct wifi67_priv *priv,
                        struct cfg80211_chan_def *chandef,
                        u32 power);
int wifi67_reg_get_power(struct wifi67_priv *priv,
                        struct cfg80211_chan_def *chandef,
                        u32 *power);
bool wifi67_reg_is_channel_allowed(struct wifi67_priv *priv,
                                 struct cfg80211_chan_def *chandef);
int wifi67_reg_start_cac(struct wifi67_priv *priv,
                        struct cfg80211_chan_def *chandef);
void wifi67_reg_stop_cac(struct wifi67_priv *priv);
void wifi67_reg_radar_detected(struct wifi67_priv *priv,
                             struct cfg80211_chan_def *chandef);

#endif /* _WIFI67_REG_CORE_H_ */ 