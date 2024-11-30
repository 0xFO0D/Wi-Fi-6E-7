#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include "../../include/perf/rate_adapt.h"
#include "../../include/debug/debug.h"

#define RATE_MAX_RETRY       3
#define RATE_MIN_SUCCESS     20  /* Minimum success percentage */
#define RATE_SCALE_UP_TIME   50  /* ms */
#define RATE_SCALE_DOWN_TIME 10  /* ms */

struct wifi67_rate_stats {
    u32 attempts;
    u32 successes;
    u32 consecutive_failures;
    ktime_t last_attempt;
    u8 cur_rate_idx;
    bool probing;
};

static const struct wifi67_rate_info {
    u16 bitrate;     /* in 100kbps */
    u8 flags;        /* IEEE80211_RATE_* */
    u8 mcs_index;    /* HT/VHT MCS index */
    u8 nss;          /* Number of spatial streams */
    u8 bw;          /* Channel bandwidth */
} wifi67_rates[] = {
    /* Legacy rates */
    { 10,   0, 0, 1, 20},  /* 1 Mbps */
    { 20,   0, 1, 1, 20},  /* 2 Mbps */
    { 55,   0, 2, 1, 20},  /* 5.5 Mbps */
    { 110,  0, 3, 1, 20},  /* 11 Mbps */
    
    /* HT rates */
    { 150,  IEEE80211_RATE_SHORT_GI, 0, 1, 20},  /* MCS0 */
    { 300,  IEEE80211_RATE_SHORT_GI, 1, 1, 20},  /* MCS1 */
    { 450,  IEEE80211_RATE_SHORT_GI, 2, 1, 20},  /* MCS2 */
    { 600,  IEEE80211_RATE_SHORT_GI, 3, 1, 20},  /* MCS3 */
    
    /* VHT rates */
    { 866,  IEEE80211_RATE_SHORT_GI, 0, 1, 80},  /* MCS0 */
    { 1733, IEEE80211_RATE_SHORT_GI, 1, 1, 80},  /* MCS1 */
    { 2340, IEEE80211_RATE_SHORT_GI, 2, 1, 80},  /* MCS2 */
    { 3600, IEEE80211_RATE_SHORT_GI, 3, 2, 80},  /* MCS3 */
    
    /* HE rates */
    { 1147,  IEEE80211_RATE_SHORT_GI, 0, 1, 160}, /* MCS0 */
    { 2294,  IEEE80211_RATE_SHORT_GI, 1, 1, 160}, /* MCS1 */
    { 3441,  IEEE80211_RATE_SHORT_GI, 2, 1, 160}, /* MCS2 */
    { 4589,  IEEE80211_RATE_SHORT_GI, 3, 2, 160}, /* MCS3 */
};

static void wifi67_rate_init_stats(struct wifi67_rate_stats *stats)
{
    memset(stats, 0, sizeof(*stats));
    stats->cur_rate_idx = 0;
    stats->last_attempt = ktime_get();
}

static bool wifi67_rate_should_probe(struct wifi67_rate_stats *stats)
{
    ktime_t now = ktime_get();
    s64 delta_ms = ktime_ms_delta(now, stats->last_attempt);
    
    if (stats->probing)
        return false;
        
    if (stats->consecutive_failures > 0)
        return false;
        
    if (delta_ms < RATE_SCALE_UP_TIME)
        return false;
        
    return true;
}

static void wifi67_rate_update_stats(struct wifi67_rate_stats *stats,
                                   bool success, u8 retries)
{
    stats->attempts++;
    
    if (success) {
        stats->successes++;
        stats->consecutive_failures = 0;
        
        if (stats->probing) {
            /* Probing succeeded, make this our new base rate */
            stats->probing = false;
        }
    } else {
        stats->consecutive_failures++;
        
        if (stats->probing) {
            /* Probing failed, return to previous rate */
            stats->cur_rate_idx = max(0, stats->cur_rate_idx - 1);
            stats->probing = false;
        } else if (stats->consecutive_failures >= RATE_MAX_RETRY) {
            /* Too many failures, scale down */
            stats->cur_rate_idx = max(0, stats->cur_rate_idx - 1);
            stats->consecutive_failures = 0;
        }
    }
}

u16 wifi67_rate_get_next(struct wifi67_rate_stats *stats,
                        struct ieee80211_sta *sta,
                        struct ieee80211_hw *hw)
{
    const struct wifi67_rate_info *rate;
    bool can_probe = wifi67_rate_should_probe(stats);
    
    if (can_probe && stats->cur_rate_idx < ARRAY_SIZE(wifi67_rates) - 1) {
        /* Try next higher rate */
        stats->cur_rate_idx++;
        stats->probing = true;
    }
    
    rate = &wifi67_rates[stats->cur_rate_idx];
    
    /* Check if station supports this rate */
    if (sta && !ieee80211_rate_valid(sta, rate->flags)) {
        stats->cur_rate_idx = max(0, stats->cur_rate_idx - 1);
        rate = &wifi67_rates[stats->cur_rate_idx];
    }
    
    return rate->bitrate;
}

void wifi67_rate_tx_status(struct wifi67_rate_stats *stats,
                          struct ieee80211_tx_info *info)
{
    bool success = !!(info->flags & IEEE80211_TX_STAT_ACK);
    u8 retries = info->status.rates[0].count - 1;
    
    wifi67_rate_update_stats(stats, success, retries);
}

EXPORT_SYMBOL_GPL(wifi67_rate_init_stats);
EXPORT_SYMBOL_GPL(wifi67_rate_get_next);
EXPORT_SYMBOL_GPL(wifi67_rate_tx_status); 