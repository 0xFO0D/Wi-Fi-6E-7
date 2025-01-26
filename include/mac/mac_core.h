#ifndef _WIFI67_MAC_CORE_H_
#define _WIFI67_MAC_CORE_H_

#include <linux/types.h>
#include "../core/wifi67.h"

#define WIFI67_MAC_MAX_QUEUES        8
#define WIFI67_MAC_BUFFER_SIZE       8192
#define WIFI67_MAC_MAX_LINKS         4
#define WIFI67_MAC_MAX_AGG_SIZE      256
#define WIFI67_MAC_MAX_AGG_SUBFRAMES 64

/* MAC Register offsets */
#define WIFI67_MAC_REG_CTRL          0x0000
#define WIFI67_MAC_REG_STATUS        0x0004
#define WIFI67_MAC_REG_INT_MASK      0x0008
#define WIFI67_MAC_REG_INT_STATUS    0x000C
#define WIFI67_MAC_REG_MLO_CTRL      0x0010
#define WIFI67_MAC_REG_AGG_CTRL      0x0014
#define WIFI67_MAC_REG_LINK_CTRL     0x0018
#define WIFI67_MAC_REG_LINK_STATUS   0x001C

/* MAC Control Register bits */
#define WIFI67_MAC_CTRL_ENABLE       BIT(0)
#define WIFI67_MAC_CTRL_RX_EN        BIT(1)
#define WIFI67_MAC_CTRL_TX_EN        BIT(2)
#define WIFI67_MAC_CTRL_MLO_EN       BIT(3)
#define WIFI67_MAC_CTRL_AGG_EN       BIT(4)
#define WIFI67_MAC_CTRL_LINK_CTRL    BIT(5)

/* MAC Status Register bits */
#define WIFI67_MAC_STATUS_READY      BIT(0)
#define WIFI67_MAC_STATUS_MLO_READY  BIT(1)
#define WIFI67_MAC_STATUS_AGG_READY  BIT(2)
#define WIFI67_MAC_STATUS_LINK_UP    BIT(3)

/* MLO Control Register bits */
#define WIFI67_MAC_MLO_PRIMARY       BIT(0)
#define WIFI67_MAC_MLO_SECONDARY     BIT(1)
#define WIFI67_MAC_MLO_SYNC_EN       BIT(2)

/* Aggregation modes */
#define WIFI67_MAC_AGG_MODE_NONE     0
#define WIFI67_MAC_AGG_MODE_AMPDU    1
#define WIFI67_MAC_AGG_MODE_MMPDU    2
#define WIFI67_MAC_AGG_MODE_HYBRID   3

struct wifi67_mac_link {
    u32 link_id;
    u32 state;
    u32 capabilities;
    bool enabled;
    bool primary;
};

struct wifi67_mac_agg_stats {
    u32 agg_frames;
    u32 agg_bytes;
    u32 agg_errors;
    u32 agg_retries;
    u32 agg_mode;
};

struct wifi67_mac_queue {
    void __iomem *regs;
    u32 head;
    u32 tail;
    u32 size;
    u32 agg_size;
    struct wifi67_mac_agg_stats agg_stats;
    spinlock_t lock;
};

struct wifi67_mac {
    void __iomem *regs;
    struct wifi67_mac_queue queues[WIFI67_MAC_MAX_QUEUES];
    struct wifi67_mac_link links[WIFI67_MAC_MAX_LINKS];
    u32 active_links;
    u32 agg_mode;
    spinlock_t lock;
    u32 irq_mask;
    bool mlo_enabled;
};

struct wifi67_hw_info {
    void __iomem *membase;
    u32 mac_offset;
    u32 phy_offset;
    u32 reg_size;
};

/* Core MAC functions */
int wifi67_mac_init(struct wifi67_priv *priv);
void wifi67_mac_deinit(struct wifi67_priv *priv);
int wifi67_mac_tx(struct wifi67_priv *priv, struct sk_buff *skb, u8 queue);
void wifi67_mac_rx(struct wifi67_priv *priv);
void wifi67_mac_irq_enable(struct wifi67_priv *priv);
void wifi67_mac_irq_disable(struct wifi67_priv *priv);

/* MLO and Link Management functions */
int wifi67_mac_enable_mlo(struct wifi67_priv *priv, bool enable);
int wifi67_mac_add_link(struct wifi67_priv *priv, u32 link_id, bool primary);
int wifi67_mac_remove_link(struct wifi67_priv *priv, u32 link_id);
int wifi67_mac_set_link_state(struct wifi67_priv *priv, u32 link_id, u32 state);

/* Aggregation control functions */
int wifi67_mac_set_agg_mode(struct wifi67_priv *priv, u32 mode);
int wifi67_mac_set_agg_size(struct wifi67_priv *priv, u8 queue, u32 size);
int wifi67_mac_get_agg_stats(struct wifi67_priv *priv, u8 queue, 
                            struct wifi67_mac_agg_stats *stats);

#endif /* _WIFI67_MAC_CORE_H_ */ 