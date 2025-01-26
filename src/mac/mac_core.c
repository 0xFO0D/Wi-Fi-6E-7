#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include "../../include/mac/mac_core.h"
#include "../../include/core/wifi67.h"

#define WIFI67_MAC_REG_CTRL      0x0000
#define WIFI67_MAC_REG_STATUS    0x0004
#define WIFI67_MAC_REG_INT_MASK  0x0008
#define WIFI67_MAC_REG_INT_STATUS 0x000C

#define WIFI67_MAC_CTRL_ENABLE   BIT(0)
#define WIFI67_MAC_CTRL_RX_EN    BIT(1)
#define WIFI67_MAC_CTRL_TX_EN    BIT(2)

static void wifi67_mac_queue_init(struct wifi67_mac_queue *queue, void __iomem *regs)
{
    queue->regs = regs;
    queue->head = 0;
    queue->tail = 0;
    queue->size = WIFI67_MAC_BUFFER_SIZE;
    queue->agg_size = WIFI67_MAC_MAX_AGG_SIZE;
    memset(&queue->agg_stats, 0, sizeof(queue->agg_stats));
    spin_lock_init(&queue->lock);
}

static void wifi67_mac_link_init(struct wifi67_mac_link *link, u32 link_id)
{
    link->link_id = link_id;
    link->state = 0;
    link->capabilities = 0;
    link->enabled = false;
    link->primary = false;
}

static void wifi67_mac_hw_init(struct wifi67_mac *mac)
{
    u32 val;
    int i;

    /* Reset MAC */
    writel(0, mac->regs + WIFI67_MAC_REG_CTRL);
    udelay(100);

    /* Enable MAC with RX/TX and advanced features */
    val = WIFI67_MAC_CTRL_ENABLE | WIFI67_MAC_CTRL_RX_EN | 
          WIFI67_MAC_CTRL_TX_EN | WIFI67_MAC_CTRL_AGG_EN;
    writel(val, mac->regs + WIFI67_MAC_REG_CTRL);

    /* Initialize MLO control */
    writel(0, mac->regs + WIFI67_MAC_REG_MLO_CTRL);

    /* Configure aggregation control */
    writel(WIFI67_MAC_AGG_MODE_HYBRID << 4, mac->regs + WIFI67_MAC_REG_AGG_CTRL);

    /* Initialize link control */
    for (i = 0; i < WIFI67_MAC_MAX_LINKS; i++) {
        writel(0, mac->regs + WIFI67_MAC_REG_LINK_CTRL + (i * 4));
    }

    /* Clear interrupts */
    writel(0xFFFFFFFF, mac->regs + WIFI67_MAC_REG_INT_STATUS);

    /* Set initial interrupt mask */
    mac->irq_mask = 0xFFFFFFFF;
    writel(mac->irq_mask, mac->regs + WIFI67_MAC_REG_INT_MASK);

    /* Wait for MAC to be ready */
    for (i = 0; i < 100; i++) {
        val = readl(mac->regs + WIFI67_MAC_REG_STATUS);
        if ((val & WIFI67_MAC_STATUS_READY) && 
            (val & WIFI67_MAC_STATUS_AGG_READY))
            break;
        udelay(100);
    }
}

int wifi67_mac_init(struct wifi67_priv *priv)
{
    struct wifi67_mac *mac;
    int i;

    mac = kzalloc(sizeof(*mac), GFP_KERNEL);
    if (!mac)
        return -ENOMEM;

    priv->mac_dev = mac;
    spin_lock_init(&mac->lock);

    /* Initialize hardware info if not already done */
    if (!priv->hw_info) {
        priv->hw_info = kzalloc(sizeof(*priv->hw_info), GFP_KERNEL);
        if (!priv->hw_info) {
            kfree(mac);
            return -ENOMEM;
        }
        priv->hw_info->membase = priv->mmio;
        priv->hw_info->mac_offset = 0x3000;
        priv->hw_info->phy_offset = 0x4000;
        priv->hw_info->reg_size = 0x1000;
    }

    mac->regs = priv->hw_info->membase + priv->hw_info->mac_offset;

    /* Initialize queues */
    for (i = 0; i < WIFI67_MAC_MAX_QUEUES; i++) {
        wifi67_mac_queue_init(&mac->queues[i], 
                            mac->regs + 0x100 + (i * 0x40));
    }

    /* Initialize links */
    for (i = 0; i < WIFI67_MAC_MAX_LINKS; i++) {
        wifi67_mac_link_init(&mac->links[i], i);
    }

    mac->active_links = 0;
    mac->agg_mode = WIFI67_MAC_AGG_MODE_HYBRID;
    mac->mlo_enabled = false;

    wifi67_mac_hw_init(mac);

    return 0;
}

void wifi67_mac_deinit(struct wifi67_priv *priv)
{
    struct wifi67_mac *mac = priv->mac_dev;
    
    if (!mac)
        return;

    /* Disable MAC and all features */
    writel(0, mac->regs + WIFI67_MAC_REG_CTRL);
    writel(0, mac->regs + WIFI67_MAC_REG_MLO_CTRL);
    writel(0, mac->regs + WIFI67_MAC_REG_AGG_CTRL);

    /* Disable interrupts */
    writel(0, mac->regs + WIFI67_MAC_REG_INT_MASK);

    /* Clear any pending interrupts */
    writel(0xFFFFFFFF, mac->regs + WIFI67_MAC_REG_INT_STATUS);

    kfree(mac);
    priv->mac_dev = NULL;
}

int wifi67_mac_enable_mlo(struct wifi67_priv *priv, bool enable)
{
    struct wifi67_mac *mac = priv->mac_dev;
    unsigned long flags;
    u32 val;

    if (!mac)
        return -EINVAL;

    spin_lock_irqsave(&mac->lock, flags);

    val = readl(mac->regs + WIFI67_MAC_REG_CTRL);
    if (enable)
        val |= WIFI67_MAC_CTRL_MLO_EN;
    else
        val &= ~WIFI67_MAC_CTRL_MLO_EN;
    writel(val, mac->regs + WIFI67_MAC_REG_CTRL);

    /* Configure MLO sync */
    val = readl(mac->regs + WIFI67_MAC_REG_MLO_CTRL);
    if (enable)
        val |= WIFI67_MAC_MLO_SYNC_EN;
    else
        val &= ~WIFI67_MAC_MLO_SYNC_EN;
    writel(val, mac->regs + WIFI67_MAC_REG_MLO_CTRL);

    mac->mlo_enabled = enable;

    spin_unlock_irqrestore(&mac->lock, flags);

    return 0;
}

int wifi67_mac_add_link(struct wifi67_priv *priv, u32 link_id, bool primary)
{
    struct wifi67_mac *mac = priv->mac_dev;
    unsigned long flags;
    u32 val;

    if (!mac || link_id >= WIFI67_MAC_MAX_LINKS)
        return -EINVAL;

    spin_lock_irqsave(&mac->lock, flags);

    /* Configure link control register */
    val = readl(mac->regs + WIFI67_MAC_REG_LINK_CTRL + (link_id * 4));
    val |= BIT(0); /* Enable link */
    if (primary)
        val |= BIT(1); /* Set as primary */
    writel(val, mac->regs + WIFI67_MAC_REG_LINK_CTRL + (link_id * 4));

    /* Update link structure */
    mac->links[link_id].enabled = true;
    mac->links[link_id].primary = primary;
    mac->active_links++;

    spin_unlock_irqrestore(&mac->lock, flags);

    return 0;
}

int wifi67_mac_remove_link(struct wifi67_priv *priv, u32 link_id)
{
    struct wifi67_mac *mac = priv->mac_dev;
    unsigned long flags;

    if (!mac || link_id >= WIFI67_MAC_MAX_LINKS)
        return -EINVAL;

    spin_lock_irqsave(&mac->lock, flags);

    /* Disable link in hardware */
    writel(0, mac->regs + WIFI67_MAC_REG_LINK_CTRL + (link_id * 4));

    /* Update link structure */
    mac->links[link_id].enabled = false;
    mac->links[link_id].primary = false;
    mac->active_links--;

    spin_unlock_irqrestore(&mac->lock, flags);

    return 0;
}

int wifi67_mac_set_link_state(struct wifi67_priv *priv, u32 link_id, u32 state)
{
    struct wifi67_mac *mac = priv->mac_dev;
    unsigned long flags;
    u32 val;

    if (!mac || link_id >= WIFI67_MAC_MAX_LINKS)
        return -EINVAL;

    spin_lock_irqsave(&mac->lock, flags);

    /* Update link state in hardware */
    val = readl(mac->regs + WIFI67_MAC_REG_LINK_CTRL + (link_id * 4));
    val &= ~(0xFF << 8); /* Clear state bits */
    val |= (state & 0xFF) << 8; /* Set new state */
    writel(val, mac->regs + WIFI67_MAC_REG_LINK_CTRL + (link_id * 4));

    /* Update link structure */
    mac->links[link_id].state = state;

    spin_unlock_irqrestore(&mac->lock, flags);

    return 0;
}

int wifi67_mac_set_agg_mode(struct wifi67_priv *priv, u32 mode)
{
    struct wifi67_mac *mac = priv->mac_dev;
    unsigned long flags;
    u32 val;

    if (!mac || mode > WIFI67_MAC_AGG_MODE_HYBRID)
        return -EINVAL;

    spin_lock_irqsave(&mac->lock, flags);

    val = readl(mac->regs + WIFI67_MAC_REG_AGG_CTRL);
    val &= ~(0xF << 4); /* Clear mode bits */
    val |= (mode << 4); /* Set new mode */
    writel(val, mac->regs + WIFI67_MAC_REG_AGG_CTRL);

    mac->agg_mode = mode;

    spin_unlock_irqrestore(&mac->lock, flags);

    return 0;
}

int wifi67_mac_set_agg_size(struct wifi67_priv *priv, u8 queue, u32 size)
{
    struct wifi67_mac *mac = priv->mac_dev;
    struct wifi67_mac_queue *q;
    unsigned long flags;

    if (!mac || queue >= WIFI67_MAC_MAX_QUEUES || 
        size > WIFI67_MAC_MAX_AGG_SIZE)
        return -EINVAL;

    q = &mac->queues[queue];

    spin_lock_irqsave(&q->lock, flags);
    q->agg_size = size;
    spin_unlock_irqrestore(&q->lock, flags);

    return 0;
}

int wifi67_mac_get_agg_stats(struct wifi67_priv *priv, u8 queue, 
                            struct wifi67_mac_agg_stats *stats)
{
    struct wifi67_mac *mac = priv->mac_dev;
    struct wifi67_mac_queue *q;
    unsigned long flags;

    if (!mac || queue >= WIFI67_MAC_MAX_QUEUES || !stats)
        return -EINVAL;

    q = &mac->queues[queue];

    spin_lock_irqsave(&q->lock, flags);
    memcpy(stats, &q->agg_stats, sizeof(*stats));
    spin_unlock_irqrestore(&q->lock, flags);

    return 0;
}

int wifi67_mac_tx(struct wifi67_priv *priv, struct sk_buff *skb, u8 queue)
{
    struct wifi67_mac *mac = priv->mac_dev;
    struct wifi67_mac_queue *q;
    unsigned long flags;
    int ret = 0;

    if (queue >= WIFI67_MAC_MAX_QUEUES)
        return -EINVAL;

    q = &mac->queues[queue];

    spin_lock_irqsave(&q->lock, flags);

    /* Add TX implementation here */
    /* This is just a skeleton - real implementation would handle DMA */
    /* and frame aggregation based on mac->agg_mode */

    spin_unlock_irqrestore(&q->lock, flags);

    return ret;
}

void wifi67_mac_rx(struct wifi67_priv *priv)
{
    struct wifi67_mac *mac = priv->mac_dev;
    struct wifi67_mac_queue *q = &mac->queues[0];
    unsigned long flags;

    spin_lock_irqsave(&q->lock, flags);

    /* Add RX implementation here */
    /* This is just a skeleton - real implementation would handle DMA */
    /* and frame de-aggregation based on mac->agg_mode */

    spin_unlock_irqrestore(&q->lock, flags);
}

void wifi67_mac_irq_enable(struct wifi67_priv *priv)
{
    struct wifi67_mac *mac = priv->mac_dev;
    writel(mac->irq_mask, mac->regs + WIFI67_MAC_REG_INT_MASK);
}

void wifi67_mac_irq_disable(struct wifi67_priv *priv)
{
    struct wifi67_mac *mac = priv->mac_dev;
    writel(0, mac->regs + WIFI67_MAC_REG_INT_MASK);
}

EXPORT_SYMBOL_GPL(wifi67_mac_init);
EXPORT_SYMBOL_GPL(wifi67_mac_deinit);
EXPORT_SYMBOL_GPL(wifi67_mac_tx);
EXPORT_SYMBOL_GPL(wifi67_mac_rx);
EXPORT_SYMBOL_GPL(wifi67_mac_irq_enable);
EXPORT_SYMBOL_GPL(wifi67_mac_irq_disable);
EXPORT_SYMBOL_GPL(wifi67_mac_enable_mlo);
EXPORT_SYMBOL_GPL(wifi67_mac_add_link);
EXPORT_SYMBOL_GPL(wifi67_mac_remove_link);
EXPORT_SYMBOL_GPL(wifi67_mac_set_link_state);
EXPORT_SYMBOL_GPL(wifi67_mac_set_agg_mode);
EXPORT_SYMBOL_GPL(wifi67_mac_set_agg_size);
EXPORT_SYMBOL_GPL(wifi67_mac_get_agg_stats);

