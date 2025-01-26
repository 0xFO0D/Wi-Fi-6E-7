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

static void wifi67_dma_channel_recover(struct wifi67_priv *priv,
                                   struct wifi67_dma_channel *chan,
                                   u32 error_type)
{
    struct dma_monitor_stats *stats = &monitor_ctx.channel_stats[chan->channel_id];
    unsigned long flags;
    u32 val;

    atomic64_inc(&stats->recovery_attempts);

    /* Stop channel */
    wifi67_dma_channel_stop(priv, chan->channel_id);

    /* Reset channel hardware */
    val = readl(chan->regs + WIFI67_DMA_REG_CTRL);
    val |= WIFI67_DMA_CTRL_RESET;
    writel(val, chan->regs + WIFI67_DMA_REG_CTRL);
    udelay(100);

    /* Clear error status */
    writel(0xFFFFFFFF, chan->regs + WIFI67_DMA_REG_ERR_STATUS);

    /* Reinitialize rings if necessary */
    if (error_type & (DMA_ERR_DESC_ERROR | DMA_ERR_RING_FULL)) {
        spin_lock_irqsave(&chan->tx_ring.lock, flags);
        chan->tx_ring.head = chan->tx_ring.tail = 0;
        spin_unlock_irqrestore(&chan->tx_ring.lock, flags);

        spin_lock_irqsave(&chan->rx_ring.lock, flags);
        chan->rx_ring.head = chan->rx_ring.tail = 0;
        spin_unlock_irqrestore(&chan->rx_ring.lock, flags);
    }

    /* Restart channel */
    wifi67_dma_channel_start(priv, chan->channel_id);

    atomic64_inc(&stats->successful_recoveries);
}

static void wifi67_dma_handle_error_locked(struct wifi67_priv *priv,
                                         struct wifi67_dma_channel *chan,
                                         u32 error_type)
{
    struct dma_monitor_stats *stats = &monitor_ctx.channel_stats[chan->channel_id];
    unsigned long flags;

    spin_lock_irqsave(&stats->lock, flags);
    stats->error_count_window++;
    stats->last_error = ktime_get();
    spin_unlock_irqrestore(&stats->lock, flags);

    if (error_type & DMA_ERR_FATAL) {
        pr_err("Fatal DMA error on channel %d: 0x%08x\n",
               chan->channel_id, error_type);
        return;
    }

    /* Attempt recovery based on error type */
    wifi67_dma_channel_recover(priv, chan, error_type);
}

static int wifi67_dma_ring_check_errors(struct wifi67_priv *priv,
                                      struct wifi67_dma_channel *chan,
                                      struct wifi67_dma_ring *ring,
                                      bool is_tx)
{
    u32 val = readl(chan->regs + WIFI67_DMA_REG_ERR_STATUS);
    u32 error_type = DMA_ERR_NONE;

    if (!val)
        return 0;

    if (val & BIT(0))
        error_type |= DMA_ERR_DESC_OWNERSHIP;
    if (val & BIT(1))
        error_type |= DMA_ERR_INVALID_LEN;
    if (val & BIT(2))
        error_type |= DMA_ERR_RING_FULL;
    if (val & BIT(3))
        error_type |= DMA_ERR_INVALID_ADDR;
    if (val & BIT(4))
        error_type |= DMA_ERR_BUS_ERROR;
    if (val & BIT(5))
        error_type |= DMA_ERR_DATA_ERROR;
    if (val & BIT(6))
        error_type |= DMA_ERR_DESC_ERROR;
    if (val & BIT(7))
        error_type |= DMA_ERR_FIFO_ERROR;
    if (val & BIT(8))
        error_type |= DMA_ERR_HARDWARE;
    if (val & BIT(31))
        error_type |= DMA_ERR_FATAL;

    if (error_type != DMA_ERR_NONE)
        wifi67_dma_handle_error_locked(priv, chan, error_type);

    return error_type ? -EIO : 0;
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

    /* Initialize monitoring system */
    ret = wifi67_dma_monitor_init(priv);
    if (ret)
        goto err_free_dma;

    /* Initialize hardware */
    wifi67_dma_hw_init(dma);

    dma->enabled = true;
    memset(&dma->stats, 0, sizeof(dma->stats));

    return 0;

err_free_dma:
    kfree(dma);
    priv->dma_dev = NULL;
    return ret;
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

    /* Deinitialize monitoring system */
    wifi67_dma_monitor_deinit(priv);

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

    /* Initialize channel registers */
    writel(WIFI67_DMA_CTRL_RESET, chan->regs + WIFI67_DMA_REG_CTRL);
    udelay(100);

    /* Configure error detection and reporting */
    writel(0xFFFFFFFF, chan->regs + WIFI67_DMA_REG_ERR_STATUS); /* Clear errors */
    writel(0xFFFFFFFF, chan->regs + WIFI67_DMA_REG_INT_MASK);   /* Enable all interrupts */

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

    /* Disable interrupts */
    writel(0, chan->regs + WIFI67_DMA_REG_INT_MASK);

    /* Clear any pending errors */
    writel(0xFFFFFFFF, chan->regs + WIFI67_DMA_REG_ERR_STATUS);

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
    int ret;

    if (!dma || channel_id >= dma->num_channels || !buf || !len)
        return -EINVAL;

    chan = &dma->channels[channel_id];
    ring = is_tx ? &chan->tx_ring : &chan->rx_ring;

    if (!ring->enabled)
        return -EINVAL;

    spin_lock_irqsave(&ring->lock, flags);

    /* Check for errors before proceeding */
    ret = wifi67_dma_ring_check_errors(priv, chan, ring, is_tx);
    if (ret)
        goto unlock;

    /* Check if ring is full */
    next = (ring->head + 1) % ring->size;
    if (next == ring->tail) {
        wifi67_dma_monitor_ring_full(priv, channel_id);
        ret = -ENOSPC;
        goto unlock;
    }

    /* Map buffer */
    dma_addr = dma_map_single(dma->dev, buf, len,
                             is_tx ? DMA_TO_DEVICE : DMA_FROM_DEVICE);
    if (dma_mapping_error(dma->dev, dma_addr)) {
        ret = -ENOMEM;
        goto unlock;
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

    /* Notify hardware of new descriptor */
    writel(ring->head, chan->regs + (is_tx ? WIFI67_DMA_REG_TX_HEAD :
                                            WIFI67_DMA_REG_RX_HEAD));

unlock:
    spin_unlock_irqrestore(&ring->lock, flags);
    return ret;
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
    int ret;

    if (!dma || channel_id >= dma->num_channels || !len)
        return NULL;

    chan = &dma->channels[channel_id];
    ring = is_tx ? &chan->tx_ring : &chan->rx_ring;

    if (!ring->enabled)
        return NULL;

    spin_lock_irqsave(&ring->lock, flags);

    /* Check for errors */
    ret = wifi67_dma_ring_check_errors(priv, chan, ring, is_tx);
    if (ret)
        goto unlock;

    /* Check if ring is empty */
    if (ring->tail == ring->head)
        goto unlock;

    /* Get descriptor */
    desc = &ring->desc[ring->tail];
    if (desc->flags & cpu_to_le32(WIFI67_DMA_DESC_OWN))
        goto unlock;

    /* Check for descriptor errors */
    if (desc->status & cpu_to_le32(0xFF000000)) {
        wifi67_dma_handle_error_locked(priv, chan, DMA_ERR_DESC_ERROR);
        goto unlock;
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

    /* Update hardware tail pointer */
    writel(ring->tail, chan->regs + (is_tx ? WIFI67_DMA_REG_TX_TAIL :
                                            WIFI67_DMA_REG_RX_TAIL));

    spin_unlock_irqrestore(&ring->lock, flags);
    return buf;

unlock:
    spin_unlock_irqrestore(&ring->lock, flags);
    return NULL;
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

