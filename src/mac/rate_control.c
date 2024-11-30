#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <net/mac80211.h>
#include "../../include/mac/rate_control.h"
#include "../../include/debug/debug.h"

/* Rate control algorithm states */
enum wifi67_rate_algo_state {
    RATE_ALGO_NORMAL,
    RATE_ALGO_PROBING,
    RATE_ALGO_RECOVERY,
    RATE_ALGO_FALLBACK
};

/* Per-station rate control context */
struct wifi67_rate_sta_info {
    /* Current state */
    enum wifi67_rate_algo_state state;
    u8 current_rate_idx;
    u8 last_rate_idx;
    u8 probe_rate_idx;
    u8 lowest_rate_idx;
    u8 highest_rate_idx;
    
    /* Rate statistics */
    struct {
        u32 attempts;
        u32 successes;
        u32 failures;
        u32 retries;
        u32 total_bytes;
        u32 throughput;  /* bytes/sec */
        ktime_t last_update;
    } stats[WIFI67_MAX_RATES];
    
    /* Probing state */
    bool probing;
    u32 probe_interval;
    u32 probe_timeout;
    ktime_t last_probe;
    u8 probe_count;
    u8 probe_success_thresh;
    
    /* Channel state */
    s8 last_rssi;
    u8 last_snr;
    u8 channel_busy;
    u32 interference_time;
    
    /* Aggregation state */
    u16 ampdu_len;
    u8 ampdu_density;
    bool ampdu_enabled;
    
    /* MU-MIMO state */
    bool mu_mimo_capable;
    u8 mu_group_id;
    u8 spatial_streams;
    
    /* Rate scaling parameters */
    struct {
        u32 up_threshold;     /* Success % to scale up */
        u32 down_threshold;   /* Success % to scale down */
        u32 probe_interval;   /* ms between probes */
        u32 scale_interval;   /* ms between rate changes */
        u8 max_retry;        /* Max retries before scaling down */
        u8 min_probes;       /* Min successful probes to scale up */
    } config;
    
    /* Historical data */
    struct {
        u8 rate_history[WIFI67_RATE_HISTORY_SIZE];
        u32 throughput_history[WIFI67_RATE_HISTORY_SIZE];
        s8 rssi_history[WIFI67_RATE_HISTORY_SIZE];
        u8 history_pos;
    } history;
    
    /* Debugfs entries */
    struct dentry *debugfs_dir;
    struct dentry *stats_file;
    struct dentry *config_file;
};

/* Global rate control context */
struct wifi67_rate_control {
    /* Rate tables */
    struct wifi67_rate_info rate_table[WIFI67_MAX_RATES];
    u32 n_rates;
    
    /* Per-band rate info */
    struct {
        u8 min_rate_idx;
        u8 max_rate_idx;
        u8 default_rate_idx;
        u8 probe_rate_idx;
    } band_rates[NUM_NL80211_BANDS];
    
    /* Global statistics */
    atomic_t total_packets;
    atomic_t total_retries;
    atomic_t total_failures;
    
    /* Station management */
    struct wifi67_rate_sta_info *stations[IEEE80211_MAX_STATIONS];
    spinlock_t lock;
    
    /* Work queue for periodic updates */
    struct delayed_work update_work;
    bool running;
    
    /* Configuration */
    struct {
        u32 update_interval;    /* ms between stat updates */
        u32 probe_interval;     /* ms between probes */
        u32 scale_interval;     /* ms between rate changes */
        u32 recovery_interval;  /* ms in recovery before probing */
        u8 max_retry;          /* max retries before scaling down */
        u8 min_probes;         /* min successful probes to scale up */
        bool ampdu_enabled;     /* enable A-MPDU aggregation */
        bool mu_mimo_enabled;   /* enable MU-MIMO */
    } config;
    
    /* Debugfs root */
    struct dentry *debugfs_dir;
};

/* Rate selection helper functions */
static bool wifi67_rate_supported(struct wifi67_rate_sta_info *rsi,
                                const struct wifi67_rate_info *rate)
{
    if (!rsi->mu_mimo_capable && (rate->flags & IEEE80211_TX_RC_MU_MIMO))
        return false;
        
    if (rate->nss > rsi->spatial_streams)
        return false;
        
    if (rate->min_rssi > rsi->last_rssi)
        return false;
        
    return true;
}

/* Rate table definitions */
static const struct wifi67_rate_info wifi67_legacy_rates[] = {
    /* 802.11b */
    { .bitrate = 10,  .flags = 0, .mcs_index = 0, .nss = 1, .min_rssi = -90 },
    { .bitrate = 20,  .flags = 0, .mcs_index = 1, .nss = 1, .min_rssi = -88 },
    { .bitrate = 55,  .flags = 0, .mcs_index = 2, .nss = 1, .min_rssi = -86 },
    { .bitrate = 110, .flags = 0, .mcs_index = 3, .nss = 1, .min_rssi = -84 },
    
    /* 802.11a/g */
    { .bitrate = 60,  .flags = IEEE80211_TX_RC_OFDM, .mcs_index = 0, .nss = 1, .min_rssi = -82 },
    { .bitrate = 90,  .flags = IEEE80211_TX_RC_OFDM, .mcs_index = 1, .nss = 1, .min_rssi = -81 },
    { .bitrate = 120, .flags = IEEE80211_TX_RC_OFDM, .mcs_index = 2, .nss = 1, .min_rssi = -79 },
    { .bitrate = 180, .flags = IEEE80211_TX_RC_OFDM, .mcs_index = 3, .nss = 1, .min_rssi = -77 },
    { .bitrate = 240, .flags = IEEE80211_TX_RC_OFDM, .mcs_index = 4, .nss = 1, .min_rssi = -74 },
    { .bitrate = 360, .flags = IEEE80211_TX_RC_OFDM, .mcs_index = 5, .nss = 1, .min_rssi = -70 },
    { .bitrate = 480, .flags = IEEE80211_TX_RC_OFDM, .mcs_index = 6, .nss = 1, .min_rssi = -66 },
    { .bitrate = 540, .flags = IEEE80211_TX_RC_OFDM, .mcs_index = 7, .nss = 1, .min_rssi = -65 },
};

/* Rate selection and update algorithms */
static void wifi67_rate_update_stats(struct wifi67_rate_sta_info *rsi,
                                   const struct wifi67_rate_info *rate,
                                   bool success, u8 retries)
{
    struct wifi67_rate_stats *stats = &rsi->stats[rsi->current_rate_idx];
    ktime_t now = ktime_get();
    u32 delta_ms;

    stats->attempts++;
    stats->total_bytes += skb_len;
    
    if (success) {
        stats->successes++;
        rsi->consecutive_failures = 0;
        
        if (rsi->state == RATE_ALGO_PROBING) {
            rsi->probe_count++;
            if (rsi->probe_count >= rsi->config.min_probes) {
                /* Probing successful, make this our new base rate */
                rsi->last_rate_idx = rsi->current_rate_idx;
                rsi->state = RATE_ALGO_NORMAL;
            }
        }
    } else {
        stats->failures++;
        stats->retries += retries;
        rsi->consecutive_failures++;
        
        if (rsi->state == RATE_ALGO_PROBING) {
            /* Probing failed, return to previous rate */
            rsi->current_rate_idx = rsi->last_rate_idx;
            rsi->state = RATE_ALGO_RECOVERY;
            rsi->probe_count = 0;
        } else if (rsi->consecutive_failures >= rsi->config.max_retry) {
            /* Too many failures, fall back */
            wifi67_rate_fallback(rsi);
        }
    }

    /* Update throughput statistics */
    delta_ms = ktime_ms_delta(now, stats->last_update);
    if (delta_ms >= 1000) {
        stats->throughput = (stats->total_bytes * 1000) / delta_ms;
        stats->total_bytes = 0;
        stats->last_update = now;
    }

    /* Update historical data */
    rsi->history.rate_history[rsi->history.history_pos] = rsi->current_rate_idx;
    rsi->history.throughput_history[rsi->history.history_pos] = stats->throughput;
    rsi->history.rssi_history[rsi->history.history_pos] = rsi->last_rssi;
    rsi->history.history_pos = (rsi->history.history_pos + 1) % WIFI67_RATE_HISTORY_SIZE;
}

static void wifi67_rate_fallback(struct wifi67_rate_sta_info *rsi)
{
    u8 new_idx;
    
    /* Find next lower supported rate */
    new_idx = rsi->current_rate_idx;
    while (new_idx > rsi->lowest_rate_idx) {
        new_idx--;
        if (wifi67_rate_supported(rsi, &wifi67_rates[new_idx]))
            break;
    }
    
    rsi->current_rate_idx = new_idx;
    rsi->state = RATE_ALGO_FALLBACK;
    rsi->consecutive_failures = 0;
    rsi->probe_count = 0;
}

static bool wifi67_should_probe(struct wifi67_rate_sta_info *rsi)
{
    struct wifi67_rate_stats *stats = &rsi->stats[rsi->current_rate_idx];
    ktime_t now = ktime_get();
    u32 delta_ms;
    u32 success_ratio;
    
    if (rsi->state != RATE_ALGO_NORMAL)
        return false;
        
    if (rsi->consecutive_failures > 0)
        return false;
        
    delta_ms = ktime_ms_delta(now, rsi->last_probe);
    if (delta_ms < rsi->config.probe_interval)
        return false;
        
    /* Check if current rate is performing well */
    success_ratio = (stats->successes * 100) / max(1U, stats->attempts);
    if (success_ratio < rsi->config.up_threshold)
        return false;
        
    /* Check if we have a higher rate to probe */
    if (rsi->current_rate_idx >= rsi->highest_rate_idx)
        return false;
        
    return true;
}

static void wifi67_rate_probe(struct wifi67_rate_sta_info *rsi)
{
    u8 probe_idx = rsi->current_rate_idx;
    
    /* Find next higher supported rate */
    while (probe_idx < rsi->highest_rate_idx) {
        probe_idx++;
        if (wifi67_rate_supported(rsi, &wifi67_rates[probe_idx])) {
            rsi->probe_rate_idx = probe_idx;
            rsi->current_rate_idx = probe_idx;
            rsi->state = RATE_ALGO_PROBING;
            rsi->probe_count = 0;
            rsi->last_probe = ktime_get();
            break;
        }
    }
}

/* Main rate selection function */
u16 wifi67_rate_get_next(struct wifi67_rate_sta_info *rsi,
                        struct ieee80211_sta *sta,
                        struct sk_buff *skb)
{
    const struct wifi67_rate_info *rate;
    
    /* Check if we should try probing a higher rate */
    if (wifi67_should_probe(rsi)) {
        wifi67_rate_probe(rsi);
    }
    
    /* Get current rate info */
    rate = &wifi67_rates[rsi->current_rate_idx];
    
    /* Update MCS index and flags based on capabilities */
    if (sta && sta->ht_cap.ht_supported) {
        rate->flags |= IEEE80211_TX_RC_MCS;
        if (sta->ht_cap.cap & IEEE80211_HT_CAP_SGI_20)
            rate->flags |= IEEE80211_TX_RC_SHORT_GI;
    }
    
    return rate->bitrate;
}

/* Rate adaptation algorithms */
static void wifi67_rate_adapt_minstrel(struct wifi67_rate_sta_info *rsi)
{
    struct wifi67_rate_stats *stats = &rsi->stats[rsi->current_rate_idx];
    u32 success_prob, cur_tp, max_tp = 0;
    u8 max_tp_rate = rsi->current_rate_idx;
    int i;

    /* Calculate throughput for each rate */
    for (i = rsi->lowest_rate_idx; i <= rsi->highest_rate_idx; i++) {
        struct wifi67_rate_stats *cur_stats = &rsi->stats[i];
        const struct wifi67_rate_info *rate = &wifi67_rates[i];
        
        if (cur_stats->attempts == 0)
            continue;
            
        success_prob = (cur_stats->successes * 100) / cur_stats->attempts;
        cur_tp = (success_prob * rate->bitrate) / 100;
        
        if (cur_tp > max_tp) {
            max_tp = cur_tp;
            max_tp_rate = i;
        }
    }
    
    /* Update current rate if better one found */
    if (max_tp_rate != rsi->current_rate_idx) {
        rsi->last_rate_idx = rsi->current_rate_idx;
        rsi->current_rate_idx = max_tp_rate;
        rsi->state = RATE_ALGO_NORMAL;
    }
}

static void wifi67_rate_adapt_pid(struct wifi67_rate_sta_info *rsi)
{
    struct wifi67_rate_stats *stats = &rsi->stats[rsi->current_rate_idx];
    s32 error, delta_error;
    static s32 last_error = 0;
    s32 adjustment;
    
    /* Calculate error from target success rate */
    error = WIFI67_RATE_SCALE_UP_THRESHOLD - 
            ((stats->successes * 100) / max(1U, stats->attempts));
            
    /* PID control */
    delta_error = error - last_error;
    adjustment = (error / 2) + (delta_error / 4);
    last_error = error;
    
    /* Apply rate adjustment */
    if (adjustment < -10 && rsi->current_rate_idx < rsi->highest_rate_idx) {
        rsi->current_rate_idx++;
    } else if (adjustment > 10 && rsi->current_rate_idx > rsi->lowest_rate_idx) {
        rsi->current_rate_idx--;
    }
}

static void wifi67_rate_adapt_adaptive(struct wifi67_rate_sta_info *rsi)
{
    struct wifi67_rate_stats *stats = &rsi->stats[rsi->current_rate_idx];
    u32 success_ratio;
    bool should_scale = false;
    
    success_ratio = (stats->successes * 100) / max(1U, stats->attempts);
    
    if (success_ratio > rsi->config.up_threshold) {
        if (rsi->current_rate_idx < rsi->highest_rate_idx) {
            rsi->current_rate_idx++;
            should_scale = true;
        }
    } else if (success_ratio < rsi->config.down_threshold) {
        if (rsi->current_rate_idx > rsi->lowest_rate_idx) {
            rsi->current_rate_idx--;
            should_scale = true;
        }
    }
    
    if (should_scale) {
        rsi->state = RATE_ALGO_NORMAL;
        stats->attempts = 0;
        stats->successes = 0;
    }
}

/* Main rate adaptation work function */
static void wifi67_rate_adapt_work(struct work_struct *work)
{
    struct wifi67_rate_control *rc = container_of(work, struct wifi67_rate_control,
                                                update_work.work);
    struct wifi67_rate_sta_info *rsi;
    unsigned long flags;
    int i;
    
    spin_lock_irqsave(&rc->lock, flags);
    
    for (i = 0; i < IEEE80211_MAX_STATIONS; i++) {
        rsi = rc->stations[i];
        if (!rsi)
            continue;
            
        switch (rc->config.algorithm) {
        case WIFI67_RATE_ALGO_MINSTREL:
            wifi67_rate_adapt_minstrel(rsi);
            break;
        case WIFI67_RATE_ALGO_PID:
            wifi67_rate_adapt_pid(rsi);
            break;
        case WIFI67_RATE_ALGO_ADAPTIVE:
            wifi67_rate_adapt_adaptive(rsi);
            break;
        }
    }
    
    spin_unlock_irqrestore(&rc->lock, flags);
    
    if (rc->running) {
        schedule_delayed_work(&rc->update_work,
                            msecs_to_jiffies(rc->config.update_interval));
    }
}

/* Cleanup functions */
static void wifi67_rate_free_sta(struct wifi67_rate_control *rc,
                                struct wifi67_rate_sta_info *rsi)
{
    debugfs_remove_recursive(rsi->debugfs_dir);
    kfree(rsi);
}

void wifi67_rate_control_deinit(struct wifi67_priv *priv)
{
    struct wifi67_rate_control *rc = priv->rate_control;
    int i;

    if (!rc)
        return;

    rc->running = false;
    cancel_delayed_work_sync(&rc->update_work);

    /* Free station contexts */
    for (i = 0; i < IEEE80211_MAX_STATIONS; i++) {
        if (rc->stations[i]) {
            wifi67_rate_free_sta(rc, rc->stations[i]);
            rc->stations[i] = NULL;
        }
    }

    debugfs_remove_recursive(rc->debugfs_dir);
    kfree(rc);
    priv->rate_control = NULL;
}

/* MAC80211 rate control interface */
static void wifi67_rate_init_sta_rates(struct ieee80211_sta *sta,
                                     struct wifi67_rate_sta_info *rsi)
{
    /* Set supported rates based on station capabilities */
    if (sta->ht_cap.ht_supported) {
        rsi->highest_rate_idx = WIFI67_MAX_HT_RATES - 1;
        if (sta->vht_cap.vht_supported)
            rsi->highest_rate_idx = WIFI67_MAX_VHT_RATES - 1;
        if (sta->he_cap.has_he)
            rsi->highest_rate_idx = WIFI67_MAX_HE_RATES - 1;
    }

    /* Configure MU-MIMO if supported */
    rsi->mu_mimo_capable = !!(sta->vht_cap.vht_supported &&
                             (sta->vht_cap.cap & IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE));
    rsi->spatial_streams = sta->rx_nss;
}

/* Export symbols for MAC80211 */
EXPORT_SYMBOL_GPL(wifi67_rate_control_init);
EXPORT_SYMBOL_GPL(wifi67_rate_control_deinit);
EXPORT_SYMBOL_GPL(wifi67_rate_get_next);
EXPORT_SYMBOL_GPL(wifi67_rate_update_stats);

