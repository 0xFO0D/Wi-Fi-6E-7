#ifndef __WIFI7_MLO_H
#define __WIFI7_MLO_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include "../core/wifi7_core.h"

#define MLO_MAX_LINKS 4
#define MLO_MAX_STR_LINKS 3
#define MLO_LINK_ID_MASK 0x0F
#define MLO_LINK_STATE_MASK 0xF0

struct wifi7_mlo_link {
    u8 link_id;
    u8 state;
    u16 freq;
    struct ieee80211_sta_ht_cap ht_cap;
    struct ieee80211_sta_vht_cap vht_cap;
    struct ieee80211_sta_he_cap he_cap;
    struct ieee80211_sta_eht_cap eht_cap;
};

struct wifi7_mlo_info {
    u8 n_links;
    struct wifi7_mlo_link links[MLO_MAX_LINKS];
    u16 tid_to_link_map[8];
    spinlock_t lock;
};

int wifi7_mlo_init(struct wifi7_dev *dev);
void wifi7_mlo_deinit(struct wifi7_dev *dev);
int wifi7_mlo_setup_link(struct wifi7_dev *dev, u8 link_id, u16 freq);
int wifi7_mlo_update_tid_map(struct wifi7_dev *dev, u8 tid, u16 link_mask);
int wifi7_mlo_get_link_state(struct wifi7_dev *dev, u8 link_id);
int wifi7_mlo_set_link_state(struct wifi7_dev *dev, u8 link_id, u8 state);

#endif /* __WIFI7_MLO_H */ 