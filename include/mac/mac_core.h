#ifndef _WIFI67_MAC_CORE_H_
#define _WIFI67_MAC_CORE_H_

#include <linux/types.h>
#include <linux/skbuff.h>
#include "../core/wifi67_forward.h"

/* MAC capabilities */
#define WIFI67_MAC_CAP_MLO         BIT(0)
#define WIFI67_MAC_CAP_MU_MIMO     BIT(1)
#define WIFI67_MAC_CAP_OFDMA       BIT(2)
#define WIFI67_MAC_CAP_TWT         BIT(3)
#define WIFI67_MAC_CAP_BSR         BIT(4)

/* Queue definitions */
#define WIFI67_NUM_TX_QUEUES       4
#define WIFI67_NUM_RX_QUEUES       4

/* MAC states */
enum wifi67_mac_state {
    WIFI67_MAC_OFF,
    WIFI67_MAC_STARTING,
    WIFI67_MAC_READY,
    WIFI67_MAC_SCANNING,
    WIFI67_MAC_CONNECTED,
};

/* Queue structure */
struct wifi67_mac_queue {
    struct sk_buff_head skbs;
    spinlock_t lock;
    bool stopped;
    u32 flags;
};

/* MAC statistics */
struct wifi67_mac_stats {
    u64 tx_packets;
    u64 rx_packets;
    u64 tx_bytes;
    u64 rx_bytes;
    u32 tx_dropped;
    u32 rx_dropped;
    u32 tx_retries;
    u32 rx_errors;
};

/* Main MAC structure */
struct wifi67_mac {
    struct wifi67_mac_queue tx_queues[WIFI67_NUM_TX_QUEUES];
    struct wifi67_mac_queue rx_queues[WIFI67_NUM_RX_QUEUES];
    struct wifi67_mac_stats stats;
    enum wifi67_mac_state state;
    
    /* Hardware registers */
    void __iomem *regs;
    
    /* Capabilities */
    u32 capabilities;
    
    /* MAC addresses */
    u8 addr[ETH_ALEN];
    u8 bssid[ETH_ALEN];
    
    /* Lock for MAC access */
    spinlock_t lock;
    
    /* Work items */
    struct work_struct tx_work;
};

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
void wifi67_mac_configure(struct wifi67_priv *priv);

#endif /* _WIFI67_MAC_CORE_H_ */ 