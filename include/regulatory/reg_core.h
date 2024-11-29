#ifndef _WIFI67_REG_CORE_H_
#define _WIFI67_REG_CORE_H_

#include <linux/types.h>
#include <net/cfg80211.h>
#include "../core/wifi67.h"

#define WIFI67_REG_MAX_RULES 10
#define WIFI67_REG_MAX_CHANNELS 100

int wifi67_regulatory_init(struct wifi67_priv *priv);
void wifi67_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request);
void wifi67_update_channel_flags(struct wifi67_priv *priv, const char *alpha2);

#endif /* _WIFI67_REG_CORE_H_ */ 