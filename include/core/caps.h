#ifndef _WIFI67_CAPS_H_
#define _WIFI67_CAPS_H_

#include <linux/ieee80211.h>

void wifi67_setup_ht_cap(struct ieee80211_supported_band *sband);
void wifi67_setup_vht_cap(struct ieee80211_supported_band *sband);
void wifi67_setup_he_cap(struct ieee80211_supported_band *sband);
void wifi67_setup_eht_cap(struct ieee80211_supported_band *sband);

#endif /* _WIFI67_CAPS_H_ */ 