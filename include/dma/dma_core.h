#ifndef _WIFI67_DMA_CORE_H_
#define _WIFI67_DMA_CORE_H_

#include <linux/types.h>
#include <linux/dma-mapping.h>
#include <linux/skbuff.h>
#include "../core/wifi67_forward.h"

/* DMA Register definitions */
#define DMA_REG_BASE         0x2000
#define DMA_CTRL_CONFIG     (DMA_REG_BASE + 0x00)
#define DMA_INT_MASK        (DMA_REG_BASE + 0x04)
#define DMA_TX_BASE         (DMA_REG_BASE + 0x10)
#define DMA_TX_HEAD         (DMA_REG_BASE + 0x14)
#define DMA_TX_TAIL         (DMA_REG_BASE + 0x18)
#define DMA_RX_BASE         (DMA_REG_BASE + 0x20)
#define DMA_RX_HEAD         (DMA_REG_BASE + 0x24)
#define DMA_RX_TAIL         (DMA_REG_BASE + 0x28)

/* DMA Control bits */
#define DMA_CTRL_ENABLE     BIT(0)
#define DMA_CTRL_SG_EN      BIT(1)
#define DMA_CTRL_RESET      BIT(31)

/* DMA Interrupt bits */
#define DMA_INT_TX_DONE     BIT(0)
#define DMA_INT_RX_DONE     BIT(1)
#define DMA_INT_TX_ERR      BIT(2)
#define DMA_INT_RX_ERR      BIT(3)
#define DMA_INT_DESC_ERR    BIT(4)

/* Constants */
#define DMA_MAX_SEGS        4
#define DMA_RING_SIZE       256
#define DMA_MAX_BUFSZ       2048

/* Descriptor Control bits */
#define DESC_CTRL_OWN       BIT(31)
#define DESC_CTRL_SOF       BIT(29)
#define DESC_CTRL_EOF       BIT(28)
#define DESC_CTRL_INT       BIT(27)

struct dma_desc {
    __le32 ctrl;
    __le32 len;
    __le64 buf_addr;
    __le64 next_desc;
    struct sk_buff *skb;
    dma_addr_t dma_addr;
};

struct dma_ring {
    struct dma_desc *desc;
    struct sk_buff **bufs;
    dma_addr_t dma_addr;
    u32 size;
    u32 count;
    u32 head;
    u32 tail;
    spinlock_t lock;
};

struct wifi67_dma {
    struct dma_ring tx_ring[4];
    struct dma_ring rx_ring[4];
};

/* Function declarations - remove static keywords from implementation */
int wifi67_dma_alloc_ring(struct wifi67_priv *priv, struct dma_ring *ring, u32 count);
void wifi67_dma_free_ring(struct wifi67_priv *priv, struct dma_ring *ring);
void wifi67_dma_reset_desc(struct dma_desc *desc);
int wifi67_dma_init(struct wifi67_priv *priv);
void wifi67_dma_deinit(struct wifi67_priv *priv);
int wifi67_dma_tx(struct wifi67_priv *priv, struct sk_buff *skb, u8 queue);
void wifi67_dma_tx_cleanup(struct wifi67_priv *priv, u8 queue);
void wifi67_dma_rx_refill(struct wifi67_priv *priv, u8 queue);

#endif /* _WIFI67_DMA_CORE_H_ */ 