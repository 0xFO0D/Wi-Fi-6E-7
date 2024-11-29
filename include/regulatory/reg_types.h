#ifndef _WIFI67_REG_TYPES_H_
#define _WIFI67_REG_TYPES_H_

#include <linux/types.h>
#include <net/mac80211.h>

/* Regulatory domain structure */
struct wifi67_regulatory {
    /* Current regulatory domain */
    char alpha2[2];
    enum nl80211_dfs_regions dfs_region;
    
    /* Supported frequency ranges */
    struct {
        u32 start_freq_khz;
        u32 end_freq_khz;
        u32 max_bandwidth_khz;
    } freq_ranges[2];  /* 5GHz and 6GHz */
    
    /* Power limits */
    struct {
        s32 max_antenna_gain;
        s32 max_eirp;
    } power_limits[2];  /* 5GHz and 6GHz */
    
    /* Regulatory flags */
    u32 flags;
    
    /* Current channel context */
    struct {
        u32 center_freq;
        u32 bandwidth;
        bool radar_required;
    } current_channel;
    
    /* DFS state */
    struct {
        bool enabled;
        u32 cac_time_ms;
        u32 nop_time_ms;
    } dfs;
    
    /* Lock for regulatory updates */
    spinlock_t lock;
};

#endif /* _WIFI67_REG_TYPES_H_ */ 