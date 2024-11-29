#ifndef _WIFI67_OPS_H_
#define _WIFI67_OPS_H_

#include <net/mac80211.h>

/* MAC80211 operation handlers */
void wifi67_mac80211_tx(struct ieee80211_hw *hw,
                       struct ieee80211_tx_control *control,
                       struct sk_buff *skb);
int wifi67_mac80211_start(struct ieee80211_hw *hw);
void wifi67_mac80211_stop(struct ieee80211_hw *hw, bool suspended);
int wifi67_config(struct ieee80211_hw *hw, u32 changed);
int wifi67_add_interface(struct ieee80211_hw *hw,
                        struct ieee80211_vif *vif);
void wifi67_remove_interface(struct ieee80211_hw *hw,
                           struct ieee80211_vif *vif);
void wifi67_configure_filter(struct ieee80211_hw *hw,
                           unsigned int changed_flags,
                           unsigned int *total_flags,
                           u64 multicast);
void wifi67_bss_info_changed(struct ieee80211_hw *hw,
                            struct ieee80211_vif *vif,
                            struct ieee80211_bss_conf *info,
                            u64 changed);
int wifi67_conf_tx(struct ieee80211_hw *hw,
                  struct ieee80211_vif *vif,
                  unsigned int link_id,
                  u16 queue,
                  const struct ieee80211_tx_queue_params *params);
int wifi67_set_rts_threshold(struct ieee80211_hw *hw, u32 value);
int wifi67_set_key(struct ieee80211_hw *hw,
                  enum set_key_cmd cmd,
                  struct ieee80211_vif *vif,
                  struct ieee80211_sta *sta,
                  struct ieee80211_key_conf *key);
void wifi67_set_default_unicast_key(struct ieee80211_hw *hw,
                                  struct ieee80211_vif *vif,
                                  int key_idx);

#endif /* _WIFI67_OPS_H_ */ 