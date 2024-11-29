#ifndef _WIFI67_BANDS_H_
#define _WIFI67_BANDS_H_

#include <net/mac80211.h>

#define WIFI67_NUM_2GHZ_CHANNELS 14
#define WIFI67_NUM_5GHZ_CHANNELS 25
#define WIFI67_NUM_6GHZ_CHANNELS 59

#define WIFI67_NUM_2GHZ_RATES 12
#define WIFI67_NUM_5GHZ_RATES 8
#define WIFI67_NUM_6GHZ_RATES 12

extern struct ieee80211_channel wifi67_2ghz_channels[WIFI67_NUM_2GHZ_CHANNELS];
extern struct ieee80211_rate wifi67_2ghz_rates[WIFI67_NUM_2GHZ_RATES];
extern struct ieee80211_channel wifi67_5ghz_channels[WIFI67_NUM_5GHZ_CHANNELS];
extern struct ieee80211_rate wifi67_5ghz_rates[WIFI67_NUM_5GHZ_RATES];
extern struct ieee80211_channel wifi67_6ghz_channels[WIFI67_NUM_6GHZ_CHANNELS];
extern struct ieee80211_rate wifi67_6ghz_rates[WIFI67_NUM_6GHZ_RATES];

extern struct ieee80211_sta_ht_cap wifi67_2ghz_ht_cap;
extern struct ieee80211_sta_vht_cap wifi67_2ghz_vht_cap;
extern struct ieee80211_sta_ht_cap wifi67_5ghz_ht_cap;
extern struct ieee80211_sta_vht_cap wifi67_5ghz_vht_cap;
extern struct ieee80211_sta_ht_cap wifi67_6ghz_ht_cap;
extern struct ieee80211_sta_vht_cap wifi67_6ghz_vht_cap;

#endif /* _WIFI67_BANDS_H_ */ 