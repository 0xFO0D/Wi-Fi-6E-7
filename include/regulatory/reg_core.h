#ifndef _WIFI67_REG_CORE_H_
#define _WIFI67_REG_CORE_H_

#include <linux/types.h>
#include <net/mac80211.h>
#include "reg_types.h"

struct wifi67_priv;

/* Regulatory functions */
int wifi67_reg_init(struct wifi67_priv *priv);
void wifi67_reg_deinit(struct wifi67_priv *priv);
void wifi67_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request);

/* Helper macros */
#define MHZ_TO_KHZ(freq) ((freq) * 1000)
#define DBM_TO_MBM(gain) ((gain) * 100)
#define DBI_TO_MBI(gain) ((gain) * 100)

/* External references to band definitions */
extern struct ieee80211_supported_band wifi67_band_5ghz;
extern struct ieee80211_supported_band wifi67_band_6ghz;

#endif /* _WIFI67_REG_CORE_H_ */ 