#ifndef _WIFI67_DMA_CORE_H_
#define _WIFI67_DMA_CORE_H_

#include <linux/types.h>
#include <linux/dma-mapping.h>
#include "../core/wifi67_forward.h"
#include "../mac/mac_core.h"

/* DMA descriptor flags */
#define WIFI67_DMA_OWN        BIT(31)
#define WIFI67_DMA_SOP        BIT(30)
#define WIFI67_DMA_EOP        BIT(29)
#define WIFI67_DMA_INT_EN     BIT(28)

/* DMA ring sizes */
#define WIFI67_DMA_TX_RING_SIZE   256
#define WIFI67_DMA_RX_RING_SIZE   256

/* DMA descriptor structure */
struct wifi67_dma_desc {
    __le32 flags;
    __le32 buf_addr;
    __le16 buf_size;
    __le16 next_desc;
} __packed __aligned(4);

/* DMA ring structure */
struct wifi67_dma_ring {
    struct wifi67_dma_desc *desc;
    dma_addr_t desc_dma;
    u16 head;
    u16 tail;
    u16 size;
    spinlock_t lock;
};

/* Main DMA structure */
struct wifi67_dma {
    struct wifi67_dma_ring tx_ring[WIFI67_NUM_TX_QUEUES];
    struct wifi67_dma_ring rx_ring[WIFI67_NUM_RX_QUEUES];
    
    /* Hardware registers */
    void __iomem *regs;
    
    /* Statistics */
    u32 tx_completed;
    u32 rx_completed;
    u32 errors;
};

/* Function declarations */
int wifi67_dma_init(struct wifi67_priv *priv);
void wifi67_dma_deinit(struct wifi67_priv *priv);
int wifi67_dma_start(struct wifi67_priv *priv);
void wifi67_dma_stop(struct wifi67_priv *priv);
int wifi67_dma_tx(struct wifi67_priv *priv, struct sk_buff *skb, u8 queue);
void wifi67_dma_rx(struct wifi67_priv *priv, u8 queue);
irqreturn_t wifi67_dma_isr(struct wifi67_priv *priv);
void wifi67_dma_tx_cleanup(struct wifi67_priv *priv, u8 queue);

#endif /* _WIFI67_DMA_CORE_H_ */ 