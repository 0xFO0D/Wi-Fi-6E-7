#include <linux/module.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>

#include "../../include/dma/dma_core.h"
#include "../../include/core/wifi67.h"

/* Register definitions */
#define WIFI67_DMA_CTRL           0x0000
#define WIFI67_DMA_STATUS         0x0004
#define WIFI67_DMA_TX_BASE        0x0010
#define WIFI67_DMA_RX_BASE        0x0020

/* Control register bits */
#define DMA_CTRL_ENABLE           BIT(0)
#define DMA_CTRL_TX_EN            BIT(1)
#define DMA_CTRL_RX_EN            BIT(2)

static int wifi67_dma_alloc_ring(struct wifi67_priv *priv,
                                struct wifi67_dma_ring *ring,
                                u16 size)
{
    ring->desc = dma_alloc_coherent(&priv->pdev->dev,
                                   size * sizeof(struct wifi67_dma_desc),
                                   &ring->desc_dma, GFP_KERNEL);
    if (!ring->desc)
        return -ENOMEM;

    ring->size = size;
    ring->head = 0;
    ring->tail = 0;
    spin_lock_init(&ring->lock);

    return 0;
}

static void wifi67_dma_free_ring(struct wifi67_priv *priv,
                                struct wifi67_dma_ring *ring)
{
    if (ring->desc) {
        dma_free_coherent(&priv->pdev->dev,
                         ring->size * sizeof(struct wifi67_dma_desc),
                         ring->desc, ring->desc_dma);
        ring->desc = NULL;
    }
}

int wifi67_dma_init(struct wifi67_priv *priv)
{
    struct wifi67_dma *dma = &priv->dma;
    int i, ret;

    /* Map DMA registers */
    dma->regs = priv->mmio + 0x4000;  /* DMA register offset */

    /* Initialize TX rings */
    for (i = 0; i < WIFI67_NUM_TX_QUEUES; i++) {
        ret = wifi67_dma_alloc_ring(priv, &dma->tx_ring[i],
                                   WIFI67_DMA_TX_RING_SIZE);
        if (ret)
            goto err_free_tx;
    }

    /* Initialize RX rings */
    for (i = 0; i < WIFI67_NUM_RX_QUEUES; i++) {
        ret = wifi67_dma_alloc_ring(priv, &dma->rx_ring[i],
                                   WIFI67_DMA_RX_RING_SIZE);
        if (ret)
            goto err_free_rx;
    }

    return 0;

err_free_rx:
    for (i--; i >= 0; i--)
        wifi67_dma_free_ring(priv, &dma->rx_ring[i]);
    i = WIFI67_NUM_TX_QUEUES;
err_free_tx:
    for (i--; i >= 0; i--)
        wifi67_dma_free_ring(priv, &dma->tx_ring[i]);
    return ret;
}

void wifi67_dma_deinit(struct wifi67_priv *priv)
{
    struct wifi67_dma *dma = &priv->dma;
    int i;

    /* Free TX rings */
    for (i = 0; i < WIFI67_NUM_TX_QUEUES; i++)
        wifi67_dma_free_ring(priv, &dma->tx_ring[i]);

    /* Free RX rings */
    for (i = 0; i < WIFI67_NUM_RX_QUEUES; i++)
        wifi67_dma_free_ring(priv, &dma->rx_ring[i]);

    /* Disable DMA */
    iowrite32(0, dma->regs + WIFI67_DMA_CTRL);
}

int wifi67_dma_start(struct wifi67_priv *priv)
{
    struct wifi67_dma *dma = &priv->dma;
    u32 reg;
    int i;

    /* Set up base addresses for all rings */
    for (i = 0; i < WIFI67_NUM_TX_QUEUES; i++) {
        iowrite32(dma->tx_ring[i].desc_dma,
                 dma->regs + WIFI67_DMA_TX_BASE + (i * 8));
    }

    for (i = 0; i < WIFI67_NUM_RX_QUEUES; i++) {
        iowrite32(dma->rx_ring[i].desc_dma,
                 dma->regs + WIFI67_DMA_RX_BASE + (i * 8));
    }

    /* Enable DMA engine */
    reg = DMA_CTRL_ENABLE | DMA_CTRL_TX_EN | DMA_CTRL_RX_EN;
    iowrite32(reg, dma->regs + WIFI67_DMA_CTRL);

    return 0;
}

void wifi67_dma_stop(struct wifi67_priv *priv)
{
    struct wifi67_dma *dma = &priv->dma;

    /* Disable DMA engine */
    iowrite32(0, dma->regs + WIFI67_DMA_CTRL);

    /* Wait for pending transactions to complete */
    msleep(100);
}

static int wifi67_dma_map_skb(struct wifi67_priv *priv,
                             struct sk_buff *skb,
                             struct wifi67_dma_desc *desc)
{
    dma_addr_t dma_addr;

    dma_addr = dma_map_single(&priv->pdev->dev, skb->data,
                             skb->len, DMA_TO_DEVICE);
    if (dma_mapping_error(&priv->pdev->dev, dma_addr))
        return -ENOMEM;

    desc->buf_addr = cpu_to_le32(dma_addr);
    desc->buf_size = cpu_to_le16(skb->len);
    desc->flags = cpu_to_le32(WIFI67_DMA_OWN | WIFI67_DMA_SOP |
                             WIFI67_DMA_EOP | WIFI67_DMA_INT_EN);

    return 0;
}

int wifi67_dma_tx(struct wifi67_priv *priv, struct sk_buff *skb, u8 queue)
{
    struct wifi67_dma *dma = &priv->dma;
    struct wifi67_dma_ring *ring;
    struct wifi67_dma_desc *desc;
    unsigned long flags;
    int ret;

    if (queue >= WIFI67_NUM_TX_QUEUES)
        return -EINVAL;

    ring = &dma->tx_ring[queue];

    spin_lock_irqsave(&ring->lock, flags);

    /* Check if ring is full */
    if (((ring->tail + 1) % ring->size) == ring->head) {
        spin_unlock_irqrestore(&ring->lock, flags);
        return -EBUSY;
    }

    desc = &ring->desc[ring->tail];

    /* Map SKB */
    ret = wifi67_dma_map_skb(priv, skb, desc);
    if (ret) {
        spin_unlock_irqrestore(&ring->lock, flags);
        return ret;
    }

    /* Update tail pointer */
    ring->tail = (ring->tail + 1) % ring->size;

    /* Notify hardware */
    iowrite32(ring->tail, dma->regs + WIFI67_DMA_TX_BASE + (queue * 8) + 4);

    spin_unlock_irqrestore(&ring->lock, flags);

    return 0;
}

static void wifi67_dma_unmap_desc(struct wifi67_priv *priv,
                                 struct wifi67_dma_desc *desc,
                                 enum dma_data_direction dir)
{
    dma_addr_t dma_addr = le32_to_cpu(desc->buf_addr);
    u16 buf_size = le16_to_cpu(desc->buf_size);

    if (dma_addr)
        dma_unmap_single(&priv->pdev->dev, dma_addr, buf_size, dir);
}

void wifi67_dma_tx_cleanup(struct wifi67_priv *priv, u8 queue)
{
    struct wifi67_dma *dma = &priv->dma;
    struct wifi67_dma_ring *ring = &dma->tx_ring[queue];
    struct wifi67_dma_desc *desc;
    unsigned long flags;

    spin_lock_irqsave(&ring->lock, flags);

    while (ring->head != ring->tail) {
        desc = &ring->desc[ring->head];

        /* Check if hardware is done with this descriptor */
        if (le32_to_cpu(desc->flags) & WIFI67_DMA_OWN)
            break;

        /* Unmap DMA */
        wifi67_dma_unmap_desc(priv, desc, DMA_TO_DEVICE);

        /* Clear descriptor */
        memset(desc, 0, sizeof(*desc));

        /* Update head pointer */
        ring->head = (ring->head + 1) % ring->size;
        dma->tx_completed++;
    }

    spin_unlock_irqrestore(&ring->lock, flags);
}

void wifi67_dma_rx(struct wifi67_priv *priv, u8 queue)
{
    struct wifi67_dma *dma = &priv->dma;
    struct wifi67_dma_ring *ring = &dma->rx_ring[queue];
    struct wifi67_dma_desc *desc;
    struct sk_buff *skb;
    unsigned long flags;
    dma_addr_t dma_addr;
    u16 buf_size;

    spin_lock_irqsave(&ring->lock, flags);

    while (1) {
        desc = &ring->desc[ring->head];

        /* Check if hardware is done with this descriptor */
        if (le32_to_cpu(desc->flags) & WIFI67_DMA_OWN)
            break;

        /* Get buffer details */
        dma_addr = le32_to_cpu(desc->buf_addr);
        buf_size = le16_to_cpu(desc->buf_size);

        /* Unmap DMA */
        dma_unmap_single(&priv->pdev->dev, dma_addr, buf_size, DMA_FROM_DEVICE);

        /* Allocate SKB and copy data */
        skb = dev_alloc_skb(buf_size);
        if (skb) {
            memcpy(skb_put(skb, buf_size), phys_to_virt(dma_addr), buf_size);
            wifi67_mac_rx(priv, skb);
            dma->rx_completed++;
        } else {
            dma->errors++;
        }

        /* Clear descriptor */
        memset(desc, 0, sizeof(*desc));

        /* Update head pointer */
        ring->head = (ring->head + 1) % ring->size;
    }

    spin_unlock_irqrestore(&ring->lock, flags);
}

irqreturn_t wifi67_dma_isr(struct wifi67_priv *priv)
{
    struct wifi67_dma *dma = &priv->dma;
    u32 status;
    int i;
    bool handled = false;

    /* Read DMA interrupt status */
    status = ioread32(dma->regs + WIFI67_DMA_STATUS);
    if (!status)
        return IRQ_NONE;

    /* Clear DMA interrupts */
    iowrite32(status, dma->regs + WIFI67_DMA_STATUS);

    /* Handle TX completions */
    if (status & 0xFF) {
        for (i = 0; i < WIFI67_NUM_TX_QUEUES; i++) {
            if (status & BIT(i)) {
                wifi67_dma_tx_cleanup(priv, i);
                handled = true;
            }
        }
    }

    /* Handle RX */
    if (status & 0xFF00) {
        for (i = 0; i < WIFI67_NUM_RX_QUEUES; i++) {
            if (status & BIT(i + 8)) {
                wifi67_dma_rx(priv, i);
                handled = true;
            }
        }
    }

    return handled ? IRQ_HANDLED : IRQ_NONE;
}

