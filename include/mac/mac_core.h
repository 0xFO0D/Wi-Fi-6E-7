#ifndef _WIFI67_MAC_CORE_H_
#define _WIFI67_MAC_CORE_H_

#include <linux/types.h>
#include <linux/skbuff.h>
#include "../core/wifi67_types.h"
#include "../core/wifi67_stats.h"

/* MAC Constants */
#define MAX_TID_COUNT           8
#define MAX_MLO_LINKS          4
#define WIFI67_MAX_TX_QUEUES   4
#define WIFI67_MAX_RX_QUEUES   4

/* MAC Register definitions */
#define MAC_REG_BASE         0x2000
#define MAC_REG_CTRL         (MAC_REG_BASE + 0x00)
#define MAC_REG_STATUS       (MAC_REG_BASE + 0x04)
#define MAC_REG_INT_STATUS   (MAC_REG_BASE + 0x08)
#define MAC_REG_INT_MASK     (MAC_REG_BASE + 0x0C)
#define MAC_REG_AMPDU_CTRL   (MAC_REG_BASE + 0x10)

/* AMPDU Control Register bits */
#define MAX_AMPDU_LEN           0x3F
#define AMPDU_MAX_LEN_SHIFT     0
#define AMPDU_MIN_SPACING       0x07
#define AMPDU_MIN_SPACE_SHIFT   8
#define MAX_AMPDU_SUBFRAMES     0x3F
#define AMPDU_MAX_SUBFRAMES_SHIFT 16
#define AMPDU_TIMEOUT           0xFF
#define AMPDU_TIMEOUT_SHIFT     24
#define AMPDU_ENABLE            BIT(31)

/* Block ACK Session */
struct mac_ba_session {
    u8 tid;
    u8 buf_size;
    u16 ssn;
    u16 head;
    u16 tail;
    bool active;
    u32 timeout;
    struct sk_buff **reorder_buf;
    unsigned long *bitmap;
    spinlock_t lock;
};

/* MLO Link State */
#define MLO_LINK_DISABLED    0
#define MLO_LINK_ACTIVE      1
#define MLO_LINK_STANDBY     2

/* MLO Link */
struct mac_mlo_link {
    u8 link_id;
    u8 state;
    bool enabled;
    u32 freq;
    u32 bandwidth;
    u8 mac_addr[ETH_ALEN];
};

struct wifi67_mac {
    /* MAC state */
    bool enabled;
    u8 mac_addr[ETH_ALEN];
    u8 bssid[ETH_ALEN];
    u16 aid;
    u32 beacon_interval;
    u32 dtim_period;
    bool associated;
    bool powersave;
    
    /* MAC statistics */
    struct mac_stats stats;
    
    /* Block ACK sessions */
    struct mac_ba_session *ba_sessions[MAX_TID_COUNT];
    
    /* MLO links */
    struct mac_mlo_link *mlo_links[MAX_MLO_LINKS];
    
    /* Queues */
    struct sk_buff_head tx_queue[WIFI67_MAX_TX_QUEUES];
    struct sk_buff_head rx_queue[WIFI67_MAX_RX_QUEUES];
    
    /* Sequence numbers */
    u16 sequence_number;
    spinlock_t seqlock;
    
    /* Protection */
    bool use_rts;
    bool use_cts;
    u16 rts_threshold;
    u16 frag_threshold;
    
    /* Rate control */
    u32 fixed_rate;
    bool fixed_rate_set;
    
    /* General lock */
    spinlock_t lock;
    
    /* Work items */
    struct work_struct tx_work;
    struct delayed_work beacon_work;
} __packed __aligned(8);

/* Function declarations */
int wifi67_mac_init(struct wifi67_priv *priv);
void wifi67_mac_deinit(struct wifi67_priv *priv);
int wifi67_mac_start(struct wifi67_priv *priv);
void wifi67_mac_stop(struct wifi67_priv *priv);
int wifi67_mac_tx(struct wifi67_priv *priv, struct sk_buff *skb);
void wifi67_mac_rx(struct wifi67_priv *priv, struct sk_buff *skb);
int wifi67_mac_configure_ampdu(struct wifi67_priv *priv);
int wifi67_mac_add_ba_session(struct wifi67_priv *priv, u8 tid, u16 ssn);
void wifi67_mac_del_ba_session(struct wifi67_priv *priv, u8 tid);
int wifi67_mac_set_power_save(struct wifi67_priv *priv, bool enable);
int wifi67_mac_add_mlo_link(struct wifi67_priv *priv, u8 link_id, u32 freq);

#endif /* _WIFI67_MAC_CORE_H_ */ 