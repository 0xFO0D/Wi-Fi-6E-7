#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/dma-mapping.h>
#include "../../include/dma/dma_core.h"
#include "../../include/core/wifi67.h"

static inline u32 dma_read32(struct wifi67_priv *priv, u32 reg)
{
    return readl(priv->mmio + reg);
}

static inline void dma_write32(struct wifi67_priv *priv, u32 reg, u32 val)
{
    writel(val, priv->mmio + reg);
}

int wifi67_dma_alloc_ring(struct wifi67_priv *priv, struct dma_ring *ring, u32 count)
{
    size_t size = count * sizeof(struct dma_desc);
    
    ring->desc = dma_alloc_coherent(&priv->pdev->dev, size,
                                   &ring->dma_addr, GFP_KERNEL);
    if (!ring->desc)
        return -ENOMEM;

    ring->bufs = kcalloc(count, sizeof(struct sk_buff *), GFP_KERNEL);
    if (!ring->bufs) {
        dma_free_coherent(&priv->pdev->dev, size,
                         ring->desc, ring->dma_addr);
        return -ENOMEM;
    }

    ring->size = count;
    ring->head = ring->tail = 0;
    spin_lock_init(&ring->lock);

    return 0;
}

void wifi67_dma_free_ring(struct wifi67_priv *priv, struct dma_ring *ring)
{
    size_t size = ring->size * sizeof(struct dma_desc);
    
    dma_free_coherent(&priv->pdev->dev, size,
                      ring->desc, ring->dma_addr);
    kfree(ring->bufs);
}

void wifi67_dma_reset_desc(struct dma_desc *desc)
{
    desc->ctrl = 0;
    desc->len = 0;
    desc->buf_addr = 0;
    desc->next_desc = 0;
    desc->skb = NULL;
    desc->dma_addr = 0;
}

static int wifi67_dma_map_skb(struct wifi67_priv *priv, struct dma_desc *desc,
                             struct sk_buff *skb, bool first, bool last)
{
    dma_addr_t mapping;
    u32 len;

    len = skb_headlen(skb);
    mapping = dma_map_single(&priv->pdev->dev, skb->data, len, DMA_TO_DEVICE);
    if (dma_mapping_error(&priv->pdev->dev, mapping)) {
        dev_err(&priv->pdev->dev, "Failed to map TX buffer\n");
        return -ENOMEM;
    }

    desc->buf_addr = cpu_to_le64(mapping);
    desc->len = cpu_to_le32(len);
    desc->ctrl = DESC_CTRL_OWN | DESC_CTRL_SOF;

    if (last) {
        desc->ctrl |= DESC_CTRL_EOF | DESC_CTRL_INT;
    }

    return 0;
}

int wifi67_dma_init(struct wifi67_priv *priv)
{
    int ret, i;

    /* Allocate TX rings */
    for (i = 0; i < WIFI67_MAX_TX_QUEUES; i++) {
        ret = wifi67_dma_alloc_ring(priv, &priv->dma.tx_ring[i],
                                   DMA_RING_SIZE);
        if (ret)
            goto err_free_tx;
    }

    /* Allocate RX rings */
    for (i = 0; i < WIFI67_MAX_RX_QUEUES; i++) {
        ret = wifi67_dma_alloc_ring(priv, &priv->dma.rx_ring[i],
                                   DMA_RING_SIZE);
        if (ret)
            goto err_free_rx;
    }

    /* Configure DMA engine */
    dma_write32(priv, DMA_CTRL_CONFIG,
                DMA_CTRL_ENABLE | DMA_CTRL_SG_EN);

    /* Enable interrupts */
    dma_write32(priv, DMA_INT_MASK,
                DMA_INT_TX_DONE | DMA_INT_RX_DONE |
                DMA_INT_TX_ERR | DMA_INT_RX_ERR |
                DMA_INT_DESC_ERR);

    return 0;

err_free_rx:
    while (i--)
        wifi67_dma_free_ring(priv, &priv->dma.rx_ring[i]);
    i = WIFI67_MAX_TX_QUEUES;
err_free_tx:
    while (i--)
        wifi67_dma_free_ring(priv, &priv->dma.tx_ring[i]);
    return ret;
}

void wifi67_dma_deinit(struct wifi67_priv *priv)
{
    int i;

    /* Disable DMA and interrupts */
    dma_write32(priv, DMA_CTRL_CONFIG, 0);
    dma_write32(priv, DMA_INT_MASK, 0);

    /* Free rings */
    for (i = 0; i < WIFI67_MAX_TX_QUEUES; i++)
        wifi67_dma_free_ring(priv, &priv->dma.tx_ring[i]);
    for (i = 0; i < WIFI67_MAX_RX_QUEUES; i++)
        wifi67_dma_free_ring(priv, &priv->dma.rx_ring[i]);
}

int wifi67_dma_tx(struct wifi67_priv *priv, struct sk_buff *skb, u8 queue)
{
    struct dma_ring *ring = &priv->dma.tx_ring[queue];
    struct dma_desc *desc;
    int ret;

    spin_lock_bh(&ring->lock);

    if (((ring->tail + 1) % ring->size) == ring->head) {
        spin_unlock_bh(&ring->lock);
        return -ENOSPC;
    }

    desc = &ring->desc[ring->tail];
    ret = wifi67_dma_map_skb(priv, desc, skb, true, true);
    if (ret) {
        spin_unlock_bh(&ring->lock);
        return ret;
    }

    ring->bufs[ring->tail] = skb;
    ring->tail = (ring->tail + 1) % ring->size;

    dma_write32(priv, DMA_TX_TAIL + (queue * 16), ring->tail);

    spin_unlock_bh(&ring->lock);
    return 0;
}

static void wifi67_dma_tx_cleanup_one(struct wifi67_priv *priv, struct dma_ring *ring)
{
    struct dma_desc *desc;
    struct sk_buff *skb;
    unsigned long flags;

    spin_lock_irqsave(&ring->lock, flags);

    while (ring->head != ring->tail) {
        desc = &ring->desc[ring->head];

        if (!(desc->ctrl & DESC_CTRL_OWN))
            break;

        skb = ring->bufs[ring->head];
        if (skb) {
            dma_unmap_single(&priv->pdev->dev,
                            le64_to_cpu(desc->buf_addr),
                            skb->len, DMA_TO_DEVICE);
            dev_kfree_skb_any(skb);
            ring->bufs[ring->head] = NULL;
        }

        wifi67_dma_reset_desc(desc);
        ring->head = (ring->head + 1) % ring->size;
    }

    spin_unlock_irqrestore(&ring->lock, flags);
}

void wifi67_dma_tx_cleanup(struct wifi67_priv *priv, u8 queue)
{
    wifi67_dma_tx_cleanup_one(priv, &priv->dma.tx_ring[queue]);
}

static int wifi67_dma_rx_fill_one(struct wifi67_priv *priv, struct dma_ring *ring)
{
    struct dma_desc *desc;
    struct sk_buff *skb;
    dma_addr_t mapping;
    unsigned long flags;

    spin_lock_irqsave(&ring->lock, flags);

    if (((ring->tail + 1) % ring->size) == ring->head) {
        spin_unlock_irqrestore(&ring->lock, flags);
        return -ENOSPC;
    }

    desc = &ring->desc[ring->tail];

    skb = dev_alloc_skb(DMA_MAX_BUFSZ);
    if (!skb) {
        spin_unlock_irqrestore(&ring->lock, flags);
        return -ENOMEM;
    }

    mapping = dma_map_single(&priv->pdev->dev, skb->data,
                            DMA_MAX_BUFSZ, DMA_FROM_DEVICE);
    if (dma_mapping_error(&priv->pdev->dev, mapping)) {
        dev_kfree_skb_any(skb);
        spin_unlock_irqrestore(&ring->lock, flags);
        return -ENOMEM;
    }

    desc->buf_addr = cpu_to_le64(mapping);
    desc->len = cpu_to_le32(DMA_MAX_BUFSZ);
    desc->ctrl = DESC_CTRL_OWN;

    ring->bufs[ring->tail] = skb;
    ring->tail = (ring->tail + 1) % ring->size;

    spin_unlock_irqrestore(&ring->lock, flags);
    return 0;
}

void wifi67_dma_rx_refill(struct wifi67_priv *priv, u8 queue)
{
    wifi67_dma_rx_fill_one(priv, &priv->dma.rx_ring[queue]);
} 