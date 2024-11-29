#ifndef _WIFI67_DMA_CORE_H_
#define _WIFI67_DMA_CORE_H_

#include <linux/types.h>
#include <linux/skbuff.h>
#include "../core/wifi67.h"

#define WIFI67_DMA_RING_SIZE 256
#define WIFI67_DMA_DESC_SIZE 16

struct wifi67_dma_desc {
    __le32 ctrl;
    __le32 len;
    __le32 addr;
    __le32 next;
};

struct wifi67_dma_buf {
    struct sk_buff *skb;
    dma_addr_t dma;
    u32 len;
};

struct wifi67_dma_ring {
    struct wifi67_dma_desc *desc;
    dma_addr_t desc_dma;
    struct wifi67_dma_buf *buf;
    u32 size;
    u32 head;
    u32 tail;
    spinlock_t lock;
};

int wifi67_dma_init(struct wifi67_priv *priv);
void wifi67_dma_deinit(struct wifi67_priv *priv);
int wifi67_dma_tx(struct wifi67_priv *priv, struct sk_buff *skb);
void wifi67_dma_rx(struct wifi67_priv *priv);

#endif /* _WIFI67_DMA_CORE_H_ */ 