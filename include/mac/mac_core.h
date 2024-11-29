#ifndef _WIFI67_MAC_CORE_H_
#define _WIFI67_MAC_CORE_H_

#include <linux/types.h>
#include "../core/wifi67.h"

#define WIFI67_MAC_MAX_QUEUES    4
#define WIFI67_MAC_BUFFER_SIZE   2048

struct wifi67_mac_queue {
    void __iomem *regs;
    u32 head;
    u32 tail;
    spinlock_t lock;
};

struct wifi67_mac {
    void __iomem *regs;
    struct wifi67_mac_queue queues[WIFI67_MAC_MAX_QUEUES];
    spinlock_t lock;
    u32 irq_mask;
};

struct wifi67_hw_info {
    void __iomem *membase;
    u32 mac_offset;
    u32 phy_offset;
    u32 reg_size;
};

int wifi67_mac_init(struct wifi67_priv *priv);
void wifi67_mac_deinit(struct wifi67_priv *priv);
int wifi67_mac_tx(struct wifi67_priv *priv, struct sk_buff *skb, u8 queue);
void wifi67_mac_rx(struct wifi67_priv *priv);
void wifi67_mac_irq_enable(struct wifi67_priv *priv);
void wifi67_mac_irq_disable(struct wifi67_priv *priv);

#endif /* _WIFI67_MAC_CORE_H_ */ 