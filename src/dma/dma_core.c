#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include "../../include/dma/dma_regs.h"
#include "../../include/dma/dma_core.h"

static inline u32 dma_read32(struct wifi67_priv *priv, u32 reg)
{
    return readl(priv->mmio + reg);
}

static inline void dma_write32(struct wifi67_priv *priv, u32 reg, u32 val)
{
    writel(val, priv->mmio + reg);
}

static int wifi67_dma_alloc_ring(struct wifi67_priv *priv, struct dma_ring *ring,
                                u32 size)
{
    ring->desc = dma_alloc_coherent(&priv->pdev->dev,
                                   size * sizeof(struct dma_desc),
                                   &ring->dma, GFP_KERNEL);
    if (!ring->desc)
        return -ENOMEM;

    ring->buf = kcalloc(size, sizeof(*ring->buf), GFP_KERNEL);
    if (!ring->buf) {
        dma_free_coherent(&priv->pdev->dev,
                         size * sizeof(struct dma_desc),
                         ring->desc, ring->dma);
        return -ENOMEM;
    }

    ring->size = size;
    ring->count = 0;
    ring->head = ring->tail = 0;
    spin_lock_init(&ring->lock);

    return 0;
}

static void wifi67_dma_free_ring(struct wifi67_priv *priv, struct dma_ring *ring)
{
    if (ring->desc) {
        dma_free_coherent(&priv->pdev->dev,
                         ring->size * sizeof(struct dma_desc),
                         ring->desc, ring->dma);
    }
    kfree(ring->buf);
}

static void wifi67_dma_reset_desc(struct dma_desc *desc)
{
    desc->addr = 0;
    desc->ctrl = 0;
    desc->status = 0;
    desc->len = 0;
    desc->flags = 0;
    desc->next = 0;
}

static int wifi67_dma_map_skb(struct wifi67_priv *priv, struct sk_buff *skb,
                             struct dma_desc *desc)
{
    struct skb_shared_info *shi = skb_shinfo(skb);
    dma_addr_t mapping;
    int i;

    if (shi->nr_frags > DMA_MAX_SEGS)
        return -EINVAL;

    /* Map linear part */
    mapping = dma_map_single(&priv->pdev->dev, skb->data,
                            skb_headlen(skb), DMA_TO_DEVICE);
    if (dma_mapping_error(&priv->pdev->dev, mapping))
        return -ENOMEM;

    desc->addr = cpu_to_le64(mapping);
    desc->len = cpu_to_le16(skb_headlen(skb));
    desc->ctrl = DESC_CTRL_OWN | DESC_CTRL_SOP;

    /* Map fragments if any */
    for (i = 0; i < shi->nr_frags; i++) {
        skb_frag_t *frag = &shi->frags[i];
        desc++;

        mapping = skb_frag_dma_map(&priv->pdev->dev, frag, 0,
                                  skb_frag_size(frag), DMA_TO_DEVICE);
        if (dma_mapping_error(&priv->pdev->dev, mapping))
            goto unmap;

        desc->addr = cpu_to_le64(mapping);
        desc->len = cpu_to_le16(skb_frag_size(frag));
        desc->ctrl = DESC_CTRL_OWN;
        if (i == shi->nr_frags - 1)
            desc->ctrl |= DESC_CTRL_EOP | DESC_CTRL_INT;
    }

    return 0;

unmap:
    /* Unmap already mapped segments */
    while (i-- > 0) {
        desc--;
        dma_unmap_single(&priv->pdev->dev,
                        le64_to_cpu(desc->addr),
                        le16_to_cpu(desc->len),
                        DMA_TO_DEVICE);
    }
    return -ENOMEM;
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
    unsigned long flags;
    int ret;

    spin_lock_irqsave(&ring->lock, flags);

    if (((ring->tail + 1) % ring->size) == ring->head) {
        spin_unlock_irqrestore(&ring->lock, flags);
        return -ENOSPC;
    }

    desc = &ring->desc[ring->tail];
    wifi67_dma_reset_desc(desc);

    ret = wifi67_dma_map_skb(priv, skb, desc);
    if (ret) {
        spin_unlock_irqrestore(&ring->lock, flags);
        return ret;
    }

    ring->buf[ring->tail] = skb;
    ring->tail = (ring->tail + 1) % ring->size;
    ring->count++;

    /* Kick the DMA engine */
    dma_write32(priv, DMA_TX_TAIL + (queue * 16), ring->tail);

    spin_unlock_irqrestore(&ring->lock, flags);
    return 0;
}

static void wifi67_dma_tx_cleanup_one(struct wifi67_priv *priv,
                                     struct dma_ring *ring)
{
    struct dma_desc *desc;
    struct sk_buff *skb;
    unsigned long flags;
    int cleaned = 0;

    spin_lock_irqsave(&ring->lock, flags);

    while (ring->count > 0) {
        desc = &ring->desc[ring->head];
        if (!(desc->status & cpu_to_le32(DESC_STATUS_DONE)))
            break;

        skb = ring->buf[ring->head];
        if (skb) {
            dma_unmap_single(&priv->pdev->dev,
                            le64_to_cpu(desc->addr),
                            le16_to_cpu(desc->len),
                            DMA_TO_DEVICE);
            dev_kfree_skb_any(skb);
            ring->buf[ring->head] = NULL;
        }

        wifi67_dma_reset_desc(desc);
        ring->head = (ring->head + 1) % ring->size;
        ring->count--;
        cleaned++;
    }

    spin_unlock_irqrestore(&ring->lock, flags);

    if (cleaned)
        netif_wake_queue(priv->netdev);
}

void wifi67_dma_tx_cleanup(struct wifi67_priv *priv, u8 queue)
{
    wifi67_dma_tx_cleanup_one(priv, &priv->dma.tx_ring[queue]);
}

static void wifi67_dma_rx_fill_one(struct wifi67_priv *priv,
                                  struct dma_ring *ring)
{
    struct dma_desc *desc;
    struct sk_buff *skb;
    dma_addr_t mapping;
    unsigned long flags;

    spin_lock_irqsave(&ring->lock, flags);

    while (ring->count < ring->size) {
        desc = &ring->desc[ring->tail];

        skb = dev_alloc_skb(DMA_MAX_BUFSZ);
        if (!skb)
            break;

        mapping = dma_map_single(&priv->pdev->dev, skb->data,
                                DMA_MAX_BUFSZ, DMA_FROM_DEVICE);
        if (dma_mapping_error(&priv->pdev->dev, mapping)) {
            dev_kfree_skb_any(skb);
            break;
        }

        desc->addr = cpu_to_le64(mapping);
        desc->len = cpu_to_le16(DMA_MAX_BUFSZ);
        desc->ctrl = DESC_CTRL_OWN;
        ring->buf[ring->tail] = skb;

        ring->tail = (ring->tail + 1) % ring->size;
        ring->count++;
    }

    spin_unlock_irqrestore(&ring->lock, flags);
}

void wifi67_dma_rx_refill(struct wifi67_priv *priv, u8 queue)
{
    wifi67_dma_rx_fill_one(priv, &priv->dma.rx_ring[queue]);
} 