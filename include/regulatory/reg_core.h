#ifndef _WIFI67_REG_CORE_H_
#define _WIFI67_REG_CORE_H_

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <net/regulatory.h>

#define MAX_DFS_CHANNELS 256
#define MAX_POWER_RULES 32
#define REG_RULE_FLAGS (NL80211_RRF_AUTO_BW | NL80211_RRF_DFS)

struct wifi67_reg_rule {
    u32 start_freq;
    u32 end_freq;
    u32 bandwidth;
    u32 max_power;
    u32 flags;
    struct rb_node node;
};

struct wifi67_dfs_state {
    u32 channel;
    u32 freq;
    u8 state;
    u8 radar_detected;
    unsigned long cac_start;
    unsigned long cac_end;
    spinlock_t lock;
};

struct wifi67_regulatory {
    struct rb_root rule_tree;
    struct mutex reg_mutex;
    char alpha2[3];
    u32 dfs_region;
    u32 current_freq;
    u32 current_bandwidth;
    u32 current_power;
    
    struct wifi67_dfs_state dfs_states[MAX_DFS_CHANNELS];
    u32 num_dfs_states;
    
    struct delayed_work dfs_work;
    struct workqueue_struct *dfs_wq;
    
    /* Hardware specific */
    void __iomem *reg_base;
    u32 reg_caps;
    u32 tx_power_limit;
    u32 antenna_gain;
    
    /* Statistics */
    u32 radar_detected_count;
    u32 cac_completed_count;
    u32 cac_failed_count;
    u64 channel_time[MAX_DFS_CHANNELS];
};

/* Function prototypes */
int wifi67_regulatory_init(struct wifi67_priv *priv);
void wifi67_regulatory_deinit(struct wifi67_priv *priv);
int wifi67_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request);
int wifi67_reg_set_power(struct wifi67_priv *priv, u32 freq, u32 power);
int wifi67_reg_get_power(struct wifi67_priv *priv, u32 freq);
bool wifi67_reg_is_channel_allowed(struct wifi67_priv *priv, u32 freq);
int wifi67_reg_start_cac(struct wifi67_priv *priv, u32 freq);
void wifi67_reg_stop_cac(struct wifi67_priv *priv);
void wifi67_reg_radar_detected(struct wifi67_priv *priv, u32 freq);

#endif /* _WIFI67_REG_CORE_H_ */ 