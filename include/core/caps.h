#ifndef _WIFI67_CAPS_H_
#define _WIFI67_CAPS_H_

#include <linux/ieee80211.h>
#include "wifi67.h"

void wifi67_setup_hw_caps(struct wifi67_priv *priv);
void wifi67_setup_band_rates(struct ieee80211_supported_band *sband,
                           struct ieee80211_rate *rates,
                           int n_rates);
void wifi67_setup_band_channels(struct ieee80211_supported_band *sband,
                              struct ieee80211_channel *channels,
                              int n_channels);
void wifi67_setup_band_capabilities(struct ieee80211_supported_band *sband);
void wifi67_setup_he_cap(struct ieee80211_sta_he_cap *he_cap);
void wifi67_setup_eht_cap(struct ieee80211_sta_eht_cap *eht_cap);

#endif /* _WIFI67_CAPS_H_ */ 