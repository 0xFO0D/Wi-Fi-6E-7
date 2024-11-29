#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include "../../include/hal/hardware.h"
#include "../../include/core/wifi67.h"
#include "../../include/dma/dma_core.h"

int wifi67_hw_init(struct wifi67_priv *priv)
{
    u32 reg_val;
    int ret;

    /* Reset the hardware */
    reg_val = ioread32(priv->mmio + WIFI67_REG_CONTROL);
    reg_val |= WIFI67_CTRL_RESET;
    iowrite32(reg_val, priv->mmio + WIFI67_REG_CONTROL);

    /* Wait for reset to complete */
    msleep(100);

    /* Initialize DMA subsystem */
    ret = wifi67_dma_init(priv);
    if (ret)
        return ret;

    /* Setup interrupt mask */
    reg_val = WIFI67_INT_RX_DONE | WIFI67_INT_TX_DONE | 
              WIFI67_INT_RX_ERROR | WIFI67_INT_TX_ERROR;
    iowrite32(reg_val, priv->mmio + WIFI67_REG_INT_MASK);

    /* Enable DMA engine */
    reg_val = ioread32(priv->mmio + WIFI67_REG_CONTROL);
    reg_val |= WIFI67_CTRL_RX_ENABLE | WIFI67_CTRL_TX_ENABLE;
    iowrite32(reg_val, priv->mmio + WIFI67_REG_CONTROL);

    /* Enable interrupts */
    reg_val = ioread32(priv->mmio + WIFI67_REG_CONTROL);
    reg_val |= WIFI67_CTRL_INT_ENABLE;
    iowrite32(reg_val, priv->mmio + WIFI67_REG_CONTROL);

    return 0;
}

void wifi67_hw_deinit(struct wifi67_priv *priv)
{
    u32 reg_val;

    /* Disable interrupts */
    reg_val = ioread32(priv->mmio + WIFI67_REG_CONTROL);
    reg_val &= ~WIFI67_CTRL_INT_ENABLE;
    iowrite32(reg_val, priv->mmio + WIFI67_REG_CONTROL);

    /* Disable DMA engine */
    reg_val = ioread32(priv->mmio + WIFI67_REG_CONTROL);
    reg_val &= ~(WIFI67_CTRL_RX_ENABLE | WIFI67_CTRL_TX_ENABLE);
    iowrite32(reg_val, priv->mmio + WIFI67_REG_CONTROL);

    /* Clear interrupt mask */
    iowrite32(0, priv->mmio + WIFI67_REG_INT_MASK);

    /* Cleanup DMA */
    wifi67_dma_deinit(priv);
}

void wifi67_hw_irq_enable(struct wifi67_priv *priv)
{
    u32 reg_val;
    reg_val = ioread32(priv->mmio + WIFI67_REG_CONTROL);
    reg_val |= WIFI67_CTRL_INT_ENABLE;
    iowrite32(reg_val, priv->mmio + WIFI67_REG_CONTROL);
}

void wifi67_hw_irq_disable(struct wifi67_priv *priv)
{
    u32 reg_val;
    reg_val = ioread32(priv->mmio + WIFI67_REG_CONTROL);
    reg_val &= ~WIFI67_CTRL_INT_ENABLE;
    iowrite32(reg_val, priv->mmio + WIFI67_REG_CONTROL);
}

irqreturn_t wifi67_hw_interrupt(int irq, void *dev)
{
    struct wifi67_priv *priv = dev;
    u32 status;

    /* Read interrupt status */
    status = ioread32(priv->mmio + WIFI67_REG_INT_STATUS);
    if (!status)
        return IRQ_NONE;

    /* Clear interrupts */
    iowrite32(status, priv->mmio + WIFI67_REG_INT_STATUS);

    /* Handle RX */
    if (status & WIFI67_INT_RX_DONE) {
        /* Process received packets */
    }

    /* Handle TX */
    if (status & WIFI67_INT_TX_DONE) {
        /* Process transmitted packets */
        wifi67_dma_tx_cleanup(priv);
    }

    /* Handle errors */
    if (status & (WIFI67_INT_RX_ERROR | WIFI67_INT_TX_ERROR)) {
        /* Handle error conditions */
    }

    return IRQ_HANDLED;
} 