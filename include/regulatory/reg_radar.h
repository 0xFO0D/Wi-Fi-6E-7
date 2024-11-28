#ifndef _WIFI67_REG_RADAR_H_
#define _WIFI67_REG_RADAR_H_

#include <linux/types.h>

/* Radar pattern definitions */
#define RADAR_TYPE_FCC       0
#define RADAR_TYPE_ETSI      1
#define RADAR_TYPE_JP        2
#define RADAR_TYPE_KR        3

struct radar_detector_specs {
    u32 min_pri;        /* Minimum Pulse Repetition Interval */
    u32 max_pri;        /* Maximum Pulse Repetition Interval */
    u32 min_width;      /* Minimum pulse width */
    u32 max_width;      /* Maximum pulse width */
    u32 min_bursts;     /* Minimum bursts */
    u32 min_chirp;      /* Minimum chirp width */
    u32 max_chirp;      /* Maximum chirp width */
    u32 pattern_count;  /* Required pattern count */
    u32 threshold;      /* Detection threshold */
};

struct radar_pulse {
    u64 timestamp;
    u32 width;
    u32 rssi;
    u32 chirp_width;
    u32 freq_delta;
};

struct radar_pattern {
    struct radar_pulse pulses[32];
    u32 num_pulses;
    u32 type;
    u32 pri;
    u32 width;
    u32 chirp;
    u32 score;
};

struct radar_statistics {
    u64 total_pulses;
    u32 false_positives;
    u32 pattern_matches[4];
    u32 pri_violations;
    u32 width_violations;
    u32 chirp_violations;
    u32 rssi_drops;
} __packed __aligned(8);

/* Function prototypes */
int wifi67_radar_init(struct wifi67_priv *priv);
void wifi67_radar_deinit(struct wifi67_priv *priv);
void wifi67_radar_process_pulse(struct wifi67_priv *priv, struct radar_pulse *pulse);
bool wifi67_radar_check_pattern(struct wifi67_priv *priv, struct radar_pattern *pattern);
void wifi67_radar_reset_detector(struct wifi67_priv *priv);

#endif /* _WIFI67_REG_RADAR_H_ */ 