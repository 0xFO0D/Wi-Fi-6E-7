#ifndef _WIFI67_RATE_CONTROL_H_
#define _WIFI67_RATE_CONTROL_H_

#include <linux/types.h>
#include <linux/ieee80211.h>
#include "../include/core/wifi67.h"

/* Rate control algorithm identifiers */
#define WIFI67_RATE_ALGO_MINSTREL     0
#define WIFI67_RATE_ALGO_PID          1
#define WIFI67_RATE_ALGO_ADAPTIVE     2

/* Rate scaling parameters */
#define WIFI67_RATE_SCALE_UP_THRESHOLD    85  /* 85% success rate */
#define WIFI67_RATE_SCALE_DOWN_THRESHOLD  60  /* 60% success rate */
#define WIFI67_RATE_MAX_RETRY_COUNT       3
#define WIFI67_RATE_PROBE_INTERVAL        100 /* ms */
#define WIFI67_RATE_UPDATE_INTERVAL       10  /* ms */

/* Maximum supported rates */
#define WIFI67_MAX_LEGACY_RATES      12
#define WIFI67_MAX_HT_RATES          32
#define WIFI67_MAX_VHT_RATES         20
#define WIFI67_MAX_HE_RATES          24
#define WIFI67_MAX_EHT_RATES         32

struct wifi67_rate_info {
    u16 bitrate;        /* in 100kbps */
    u16 flags;          /* IEEE80211_TX_RC_* */
    u8 mcs_index;       /* HT/VHT/HE MCS index */
    u8 nss;            /* Number of spatial streams */
    u8 bw;             /* Bandwidth in MHz */
    u8 gi;             /* Guard interval in ns */
    u8 he_gi;          /* HE specific guard interval */
    u8 he_ru;          /* HE resource unit size */
    u8 min_rssi;       /* Minimum RSSI required */
    u8 max_retries;    /* Maximum retry count */
};

/* Continue with more structs and function declarations... */

#endif /* _WIFI67_RATE_CONTROL_H_ */ 