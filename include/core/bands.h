#ifndef _WIFI67_BANDS_H_
#define _WIFI67_BANDS_H_

#include <linux/types.h>
#include <linux/ieee80211.h>
#include <linux/nl80211.h>

/* Band definitions */
struct wifi67_band {
    enum nl80211_band band_id;
    u32 flags;
    u32 n_channels;
    const struct ieee80211_channel *channels;
    u32 n_bitrates;
    const struct ieee80211_rate *bitrates;
    bool ht_supported;
    bool vht_supported;
    bool he_supported;
    bool eht_supported;
};

/* Export symbols */
extern struct wifi67_band wifi67_band_5ghz;
extern struct wifi67_band wifi67_band_6ghz;

#endif /* _WIFI67_BANDS_H_ */ 