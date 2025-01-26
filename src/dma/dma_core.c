#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include "../../include/core/wifi67.h"
#include "../../include/dma/dma_core.h"

static int wifi67_dma_ring_alloc(struct wifi67_priv *priv,
                                struct wifi67_dma_ring *ring)
{
    ring->desc = dma_alloc_coherent(priv->dma_dev->dev,
                                   WIFI67_DMA_RING_SIZE * sizeof(*ring->desc),
                                   &ring->desc_dma, GFP_KERNEL);
    if (!ring->desc)
        return -ENOMEM;

    ring->buf_addr = kcalloc(WIFI67_DMA_RING_SIZE, sizeof(*ring->buf_addr),
                            GFP_KERNEL);
    if (!ring->buf_addr)
        goto err_free_desc;

    ring->buf_dma = kcalloc(WIFI67_DMA_RING_SIZE, sizeof(*ring->buf_dma),
                           GFP_KERNEL);
    if (!ring->buf_dma)
        goto err_free_buf;

    ring->size = WIFI67_DMA_RING_SIZE;
    ring->head = 0;
    ring->tail = 0;
    ring->enabled = false;
    spin_lock_init(&ring->lock);

    return 0;

err_free_buf:
    kfree(ring->buf_addr);
err_free_desc:
    dma_free_coherent(priv->dma_dev->dev,
                      WIFI67_DMA_RING_SIZE * sizeof(*ring->desc),
                      ring->desc, ring->desc_dma);
    return -ENOMEM;
}

static void wifi67_dma_ring_free(struct wifi67_priv *priv,
                                struct wifi67_dma_ring *ring)
{
    kfree(ring->buf_dma);
    kfree(ring->buf_addr);
    dma_free_coherent(priv->dma_dev->dev,
                      WIFI67_DMA_RING_SIZE * sizeof(*ring->desc),
                      ring->desc, ring->desc_dma);
}

static void wifi67_dma_hw_init(struct wifi67_dma *dma)
{
    u32 val;
    int i;

    /* Reset DMA engine */
    writel(WIFI67_DMA_CTRL_RESET, dma->regs + WIFI67_DMA_REG_CTRL);
    udelay(100);

    /* Enable DMA with advanced features */
    val = WIFI67_DMA_CTRL_ENABLE | WIFI67_DMA_CTRL_TX_EN |
          WIFI67_DMA_CTRL_RX_EN | WIFI67_DMA_CTRL_BURST_EN |
          WIFI67_DMA_CTRL_CHAIN_EN | WIFI67_DMA_CTRL_PERF_EN;
    writel(val, dma->regs + WIFI67_DMA_REG_CTRL);

    /* Initialize all channels */
    for (i = 0; i < dma->num_channels; i++) {
        struct wifi67_dma_channel *chan = &dma->channels[i];
        
        /* Set channel base registers */
        chan->regs = dma->regs + 0x100 + (i * 0x40);
        
        /* Configure channel control */
        writel(WIFI67_DMA_RING_ENABLE | WIFI67_DMA_RING_CHAIN,
               chan->regs + WIFI67_DMA_REG_RING_CTRL);
    }

    /* Clear all interrupts */
    writel(0xFFFFFFFF, dma->regs + WIFI67_DMA_REG_INT_STATUS);

    /* Enable all interrupts */
    writel(0xFFFFFFFF, dma->regs + WIFI67_DMA_REG_INT_MASK);

    /* Set default burst size */
    writel(WIFI67_DMA_MAX_BURST_SIZE, dma->regs + WIFI67_DMA_REG_DESC_CTRL);
}

int wifi67_dma_init(struct wifi67_priv *priv)
{
    struct wifi67_dma *dma;
    int ret;

    dma = kzalloc(sizeof(*dma), GFP_KERNEL);
    if (!dma)
        return -ENOMEM;

    priv->dma_dev = dma;
    dma->dev = priv->dev;
    spin_lock_init(&dma->lock);

    /* Set base registers */
    dma->regs = priv->hw_info->membase + 0x5000; /* DMA base offset */
    dma->num_channels = WIFI67_DMA_MAX_CHANNELS;

    /* Initialize hardware */
    wifi67_dma_hw_init(dma);

    dma->enabled = true;
    memset(&dma->stats, 0, sizeof(dma->stats));

    return 0;
}

void wifi67_dma_deinit(struct wifi67_priv *priv)
{
    struct wifi67_dma *dma = priv->dma_dev;
    int i;

    if (!dma)
        return;

    /* Stop all channels */
    for (i = 0; i < dma->num_channels; i++)
        wifi67_dma_channel_stop(priv, i);

    /* Disable DMA engine */
    writel(0, dma->regs + WIFI67_DMA_REG_CTRL);

    /* Disable interrupts */
    writel(0, dma->regs + WIFI67_DMA_REG_INT_MASK);

    /* Clear any pending interrupts */
    writel(0xFFFFFFFF, dma->regs + WIFI67_DMA_REG_INT_STATUS);

    kfree(dma);
    priv->dma_dev = NULL;
}

int wifi67_dma_channel_init(struct wifi67_priv *priv, u32 channel_id)
{
    struct wifi67_dma *dma = priv->dma_dev;
    struct wifi67_dma_channel *chan;
    int ret;

    if (!dma || channel_id >= dma->num_channels)
        return -EINVAL;

    chan = &dma->channels[channel_id];
    chan->channel_id = channel_id;

    /* Initialize TX ring */
    ret = wifi67_dma_ring_alloc(priv, &chan->tx_ring);
    if (ret)
        return ret;

    /* Initialize RX ring */
    ret = wifi67_dma_ring_alloc(priv, &chan->rx_ring);
    if (ret) {
        wifi67_dma_ring_free(priv, &chan->tx_ring);
        return ret;
    }

    chan->enabled = true;
    return 0;
}

void wifi67_dma_channel_deinit(struct wifi67_priv *priv, u32 channel_id)
{
    struct wifi67_dma *dma = priv->dma_dev;
    struct wifi67_dma_channel *chan;

    if (!dma || channel_id >= dma->num_channels)
        return;

    chan = &dma->channels[channel_id];
    
    /* Stop channel first */
    wifi67_dma_channel_stop(priv, channel_id);

    /* Free rings */
    wifi67_dma_ring_free(priv, &chan->tx_ring);
    wifi67_dma_ring_free(priv, &chan->rx_ring);

    chan->enabled = false;
}

int wifi67_dma_channel_start(struct wifi67_priv *priv, u32 channel_id)
{
    struct wifi67_dma *dma = priv->dma_dev;
    struct wifi67_dma_channel *chan;
    u32 val;

    if (!dma || channel_id >= dma->num_channels)
        return -EINVAL;

    chan = &dma->channels[channel_id];
    if (!chan->enabled)
        return -EINVAL;

    /* Enable channel rings */
    val = readl(chan->regs + WIFI67_DMA_REG_RING_CTRL);
    val |= WIFI67_DMA_RING_ENABLE;
    writel(val, chan->regs + WIFI67_DMA_REG_RING_CTRL);

    chan->tx_ring.enabled = true;
    chan->rx_ring.enabled = true;

    return 0;
}

void wifi67_dma_channel_stop(struct wifi67_priv *priv, u32 channel_id)
{
    struct wifi67_dma *dma = priv->dma_dev;
    struct wifi67_dma_channel *chan;

    if (!dma || channel_id >= dma->num_channels)
        return;

    chan = &dma->channels[channel_id];

    /* Disable channel rings */
    writel(0, chan->regs + WIFI67_DMA_REG_RING_CTRL);

    chan->tx_ring.enabled = false;
    chan->rx_ring.enabled = false;
}

int wifi67_dma_ring_add_buffer(struct wifi67_priv *priv, u32 channel_id,
                              bool is_tx, void *buf, u32 len)
{
    struct wifi67_dma *dma = priv->dma_dev;
    struct wifi67_dma_channel *chan;
    struct wifi67_dma_ring *ring;
    struct wifi67_dma_desc *desc;
    unsigned long flags;
    dma_addr_t dma_addr;
    u32 next;

    if (!dma || channel_id >= dma->num_channels || !buf || !len)
        return -EINVAL;

    chan = &dma->channels[channel_id];
    ring = is_tx ? &chan->tx_ring : &chan->rx_ring;

    if (!ring->enabled)
        return -EINVAL;

    spin_lock_irqsave(&ring->lock, flags);

    /* Check if ring is full */
    next = (ring->head + 1) % ring->size;
    if (next == ring->tail) {
        dma->stats.ring_full++;
        spin_unlock_irqrestore(&ring->lock, flags);
        return -ENOSPC;
    }

    /* Map buffer */
    dma_addr = dma_map_single(dma->dev, buf, len, 
                             is_tx ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
    if (dma_mapping_error(dma->dev, dma_addr)) {
        dma->stats.buf_errors++;
        spin_unlock_irqrestore(&ring->lock, flags);
        return -ENOMEM;
    }

    /* Setup descriptor */
    desc = &ring->desc[ring->head];
    desc->flags = cpu_to_le32(WIFI67_DMA_DESC_OWN | WIFI67_DMA_DESC_SOP |
                             WIFI67_DMA_DESC_EOP | WIFI67_DMA_DESC_INT);
    desc->buf_addr = cpu_to_le32(dma_addr);
    desc->buf_len = cpu_to_le16(len);
    desc->next_desc = cpu_to_le16(next);
    desc->status = 0;
    desc->timestamp = cpu_to_le32(jiffies);

    /* Store buffer info */
    ring->buf_addr[ring->head] = buf;
    ring->buf_dma[ring->head] = dma_addr;

    /* Update ring state */
    ring->head = next;
    if (is_tx)
        dma->stats.tx_bytes += len;
    else
        dma->stats.rx_bytes += len;

    spin_unlock_irqrestore(&ring->lock, flags);

    return 0;
}

void *wifi67_dma_ring_get_buffer(struct wifi67_priv *priv, u32 channel_id,
                                bool is_tx, u32 *len)
{
    struct wifi67_dma *dma = priv->dma_dev;
    struct wifi67_dma_channel *chan;
    struct wifi67_dma_ring *ring;
    struct wifi67_dma_desc *desc;
    unsigned long flags;
    void *buf;

    if (!dma || channel_id >= dma->num_channels || !len)
        return NULL;

    chan = &dma->channels[channel_id];
    ring = is_tx ? &chan->tx_ring : &chan->rx_ring;

    if (!ring->enabled)
        return NULL;

    spin_lock_irqsave(&ring->lock, flags);

    /* Check if ring is empty */
    if (ring->tail == ring->head) {
        spin_unlock_irqrestore(&ring->lock, flags);
        return NULL;
    }

    /* Get descriptor */
    desc = &ring->desc[ring->tail];
    if (desc->flags & cpu_to_le32(WIFI67_DMA_DESC_OWN)) {
        spin_unlock_irqrestore(&ring->lock, flags);
        return NULL;
    }

    /* Get buffer */
    buf = ring->buf_addr[ring->tail];
    *len = le16_to_cpu(desc->buf_len);

    /* Unmap buffer */
    dma_unmap_single(dma->dev, ring->buf_dma[ring->tail], *len,
                     is_tx ? DMA_TO_DEVICE : DMA_FROM_DEVICE);

    /* Update ring state */
    ring->tail = (ring->tail + 1) % ring->size;
    if (is_tx)
        dma->stats.tx_packets++;
    else
        dma->stats.rx_packets++;

    spin_unlock_irqrestore(&ring->lock, flags);

    return buf;
}

int wifi67_dma_get_stats(struct wifi67_priv *priv, struct wifi67_dma_stats *stats)
{
    struct wifi67_dma *dma = priv->dma_dev;
    unsigned long flags;

    if (!dma || !stats)
        return -EINVAL;

    spin_lock_irqsave(&dma->lock, flags);
    memcpy(stats, &dma->stats, sizeof(*stats));
    spin_unlock_irqrestore(&dma->lock, flags);

    return 0;
}

void wifi67_dma_clear_stats(struct wifi67_priv *priv)
{
    struct wifi67_dma *dma = priv->dma_dev;
    unsigned long flags;

    if (!dma)
        return;

    spin_lock_irqsave(&dma->lock, flags);
    memset(&dma->stats, 0, sizeof(dma->stats));
    spin_unlock_irqrestore(&dma->lock, flags);
}

int wifi67_dma_set_burst_size(struct wifi67_priv *priv, u32 size)
{
    struct wifi67_dma *dma = priv->dma_dev;
    unsigned long flags;

    if (!dma || size > WIFI67_DMA_MAX_BURST_SIZE)
        return -EINVAL;

    spin_lock_irqsave(&dma->lock, flags);
    writel(size, dma->regs + WIFI67_DMA_REG_DESC_CTRL);
    spin_unlock_irqrestore(&dma->lock, flags);

    return 0;
}

EXPORT_SYMBOL_GPL(wifi67_dma_init);
EXPORT_SYMBOL_GPL(wifi67_dma_deinit);
EXPORT_SYMBOL_GPL(wifi67_dma_channel_init);
EXPORT_SYMBOL_GPL(wifi67_dma_channel_deinit);
EXPORT_SYMBOL_GPL(wifi67_dma_channel_start);
EXPORT_SYMBOL_GPL(wifi67_dma_channel_stop);
EXPORT_SYMBOL_GPL(wifi67_dma_ring_add_buffer);
EXPORT_SYMBOL_GPL(wifi67_dma_ring_get_buffer);
EXPORT_SYMBOL_GPL(wifi67_dma_get_stats);
EXPORT_SYMBOL_GPL(wifi67_dma_clear_stats);
EXPORT_SYMBOL_GPL(wifi67_dma_set_burst_size);

