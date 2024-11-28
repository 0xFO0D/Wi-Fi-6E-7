#ifndef _WIFI67_MAC_CORE_H_
#define _WIFI67_MAC_CORE_H_

#include <linux/types.h>
#include <linux/skbuff.h>
#include "../core/wifi67.h"

#define MAX_TID_COUNT        16
#define MAX_BA_SESSIONS      32
#define MAX_AMPDU_LEN        64
#define MAX_MLO_LINKS        4

struct mac_ba_session {
    u16 ssn;
    u8 tid;
    u8 buf_size;
    u16 timeout;
    bool active;
    spinlock_t lock;
    struct sk_buff **reorder_buf;
    unsigned long *bitmap;
} __packed __aligned(8);

struct mac_mlo_link {
    u8 link_id;
    u8 state;
    u32 freq;
    struct mac_stats stats;
} __packed;

struct mac_stats {
    u32 tx_ampdu_ok;
    u32 tx_ampdu_fail;
    u32 rx_ampdu_ok;
    u32 rx_ampdu_fail;
    u32 ba_mismatches;
    u32 ps_transitions;
    u32 beacon_miss;
    u32 mlo_switches;
} __packed __aligned(8);

int wifi67_mac_init(struct wifi67_priv *priv);
void wifi67_mac_deinit(struct wifi67_priv *priv);
int wifi67_mac_start(struct wifi67_priv *priv);
void wifi67_mac_stop(struct wifi67_priv *priv);
int wifi67_mac_config(struct wifi67_priv *priv, u32 changed);
int wifi67_mac_add_ba_session(struct wifi67_priv *priv, u8 tid, u16 ssn);
void wifi67_mac_del_ba_session(struct wifi67_priv *priv, u8 tid);
int wifi67_mac_set_power_save(struct wifi67_priv *priv, bool enable);
int wifi67_mac_add_mlo_link(struct wifi67_priv *priv, u8 link_id, u32 freq);

#endif /* _WIFI67_MAC_CORE_H_ */ 