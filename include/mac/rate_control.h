#ifndef _WIFI67_RATE_CONTROL_H_
#define _WIFI67_RATE_CONTROL_H_

#include <linux/types.h>
#include <linux/ieee80211.h>
#include "../core/wifi67.h"

/* Rate control algorithm identifiers and parameters already defined */
/* Reference existing definitions from lines 8-25 */

/* Rate information structure already defined */
/* Reference existing structure from lines 27-38 */

/* Function declarations */
int wifi67_rate_control_init(struct wifi67_priv *priv);
void wifi67_rate_control_deinit(struct wifi67_priv *priv);

/* Rate selection and update functions */
u16 wifi67_rate_get_next(struct wifi67_rate_sta_info *rsi,
                        struct ieee80211_sta *sta,
                        struct sk_buff *skb);
void wifi67_rate_update_stats(struct wifi67_rate_sta_info *rsi,
                            const struct wifi67_rate_info *rate,
                            bool success, u8 retries);

/* Station management */
int wifi67_rate_init_sta(struct wifi67_rate_control *rc,
                        struct ieee80211_sta *sta);
void wifi67_rate_free_sta(struct wifi67_rate_control *rc,
                         struct wifi67_rate_sta_info *rsi);

/* Debug interface */
int wifi67_rate_debugfs_init(struct wifi67_rate_control *rc);
void wifi67_rate_debugfs_remove(struct wifi67_rate_control *rc);

#endif /* _WIFI67_RATE_CONTROL_H_ */ 