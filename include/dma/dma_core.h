#ifndef _WIFI67_DMA_CORE_H_
#define _WIFI67_DMA_CORE_H_

#include <linux/types.h>
#include <linux/dma-mapping.h>
#include "../core/wifi67.h"

#define DMA_RING_SIZE       256
#define DMA_MAX_BUFSZ      (16 * 1024)
#define DMA_MAX_SEGS       16

struct dma_ring {
    struct dma_desc *desc;
    dma_addr_t dma;
    u32 size;
    u32 count;
    u16 head;
    u16 tail;
    spinlock_t lock ____cacheline_aligned;
    struct sk_buff **buf;
};

struct dma_stats {
    u64 tx_packets;
    u64 rx_packets;
    u64 tx_bytes;
    u64 rx_bytes;
    u32 tx_errors;
    u32 rx_errors;
    u32 tx_timeouts;
    u32 rx_dropped;
    u32 desc_errors;
    u32 sg_errors;
} __packed __aligned(8);

int wifi67_dma_init(struct wifi67_priv *priv);
void wifi67_dma_deinit(struct wifi67_priv *priv);
int wifi67_dma_start(struct wifi67_priv *priv);
void wifi67_dma_stop(struct wifi67_priv *priv);
int wifi67_dma_tx(struct wifi67_priv *priv, struct sk_buff *skb, u8 queue);
void wifi67_dma_tx_cleanup(struct wifi67_priv *priv, u8 queue);
void wifi67_dma_rx_refill(struct wifi67_priv *priv, u8 queue);

#endif /* _WIFI67_DMA_CORE_H_ */ 