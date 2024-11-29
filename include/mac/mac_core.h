#ifndef _WIFI67_MAC_CORE_H_
#define _WIFI67_MAC_CORE_H_

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/atomic.h>
#include <net/mac80211.h>
#include "mac_defs.h"
#include "mac_types.h"

struct wifi67_mac_queue {
    struct sk_buff_head skbs;
    spinlock_t lock;
    bool stopped;
    u32 flags;
    u32 hw_queue;
};

struct wifi67_mac {
    /* Hardware registers */
    void __iomem *regs;
    
    /* MAC address */
    u8 addr[ETH_ALEN];
    
    /* Configuration */
    struct wifi67_mac_config config;
    struct wifi67_mac_stats stats;
    
    /* State management */
    atomic_t state;
    
    /* TX/RX queues */
    struct wifi67_mac_queue tx_queues[WIFI67_NUM_TX_QUEUES];
    struct wifi67_mac_queue rx_queues[WIFI67_NUM_RX_QUEUES];
    
    /* Work items */
    struct work_struct tx_work;
    
    /* Locks */
    spinlock_t lock;
};

/* Forward declaration of wifi67_priv */
struct wifi67_priv;

/* Function declarations */
int wifi67_mac_init(struct wifi67_priv *priv);
void wifi67_mac_deinit(struct wifi67_priv *priv);
int wifi67_mac_start(struct wifi67_priv *priv);
void wifi67_mac_stop(struct wifi67_priv *priv);
int wifi67_mac_tx(struct wifi67_priv *priv, struct sk_buff *skb,
                 struct ieee80211_tx_control *control);
void wifi67_mac_rx(struct wifi67_priv *priv, struct sk_buff *skb);
int wifi67_mac_set_key(struct wifi67_priv *priv, enum set_key_cmd cmd,
                     struct ieee80211_vif *vif, struct ieee80211_sta *sta,
                     struct ieee80211_key_conf *key);
int wifi67_mac_config(struct wifi67_priv *priv, u32 changed);

/* Internal functions - moved to source file */

#endif /* _WIFI67_MAC_CORE_H_ */ 