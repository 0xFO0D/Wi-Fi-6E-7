#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/iopoll.h>

#include "../../include/hal/hardware.h"
#include "../../include/core/wifi67.h"
#include "../../include/dma/dma_core.h"

/* Hardware version constants */
#define WIFI67_HW_VERSION_ID      0x67670001
#define WIFI67_HW_VERSION_MASK    0xFFFF0000

bool wifi67_hw_check_version(struct wifi67_priv *priv)
{
    struct wifi67_hw *hw = &priv->hal;
    u32 version;

    version = ioread32(hw->regs + WIFI67_HW_VERSION);
    hw->version = version;

    if ((version & WIFI67_HW_VERSION_MASK) != 
        (WIFI67_HW_VERSION_ID & WIFI67_HW_VERSION_MASK)) {
        dev_err(&priv->pdev->dev, 
                "Invalid hardware version: %08x\n", version);
        return false;
    }

    return true;
}

static void wifi67_hw_detect_caps(struct wifi67_priv *priv)
{
    struct wifi67_hw *hw = &priv->hal;
    u32 caps;

    caps = ioread32(hw->regs + WIFI67_HW_CONFIG);
    hw->capabilities = caps;

    dev_info(&priv->pdev->dev, 
             "Hardware capabilities: %08x\n", caps);
}

int wifi67_hw_init(struct wifi67_priv *priv)
{
    struct wifi67_hw *hw = &priv->hal;

    /* Initialize locks */
    spin_lock_init(&hw->irq_lock);

    /* Map hardware registers */
    hw->regs = priv->mmio;
    
    /* Check hardware version */
    if (!wifi67_hw_check_version(priv))
        return -ENODEV;

    /* Detect capabilities */
    wifi67_hw_detect_caps(priv);

    /* Initialize state */
    hw->state = WIFI67_HW_OFF;
    hw->irq_mask = 0;

    return 0;
}

void wifi67_hw_deinit(struct wifi67_priv *priv)
{
    struct wifi67_hw *hw = &priv->hal;

    /* Disable all interrupts */
    iowrite32(0, hw->regs + WIFI67_HW_INT_MASK);
    
    /* Clear any pending interrupts */
    iowrite32(0xFFFFFFFF, hw->regs + WIFI67_HW_INT_STATUS);

    hw->state = WIFI67_HW_OFF;
}

int wifi67_hw_start(struct wifi67_priv *priv)
{
    struct wifi67_hw *hw = &priv->hal;
    u32 reg;
    int ret;

    hw->state = WIFI67_HW_BOOTING;

    /* Enable hardware */
    reg = ioread32(hw->regs + WIFI67_HW_CONFIG);
    reg |= BIT(0); /* Enable bit */
    iowrite32(reg, hw->regs + WIFI67_HW_CONFIG);

    /* Wait for hardware ready */
    ret = readl_poll_timeout_atomic(hw->regs + WIFI67_HW_STATUS,
                                  reg, reg & BIT(0),
                                  1000, 1000000);
    if (ret) {
        dev_err(&priv->pdev->dev, "Hardware failed to start\n");
        return ret;
    }

    hw->state = WIFI67_HW_READY;

    /* Enable interrupts */
    wifi67_hw_irq_enable(priv);

    hw->state = WIFI67_HW_RUNNING;

    return 0;
}

void wifi67_hw_stop(struct wifi67_priv *priv)
{
    struct wifi67_hw *hw = &priv->hal;
    u32 reg;

    /* Disable interrupts */
    wifi67_hw_irq_disable(priv);

    /* Disable hardware */
    reg = ioread32(hw->regs + WIFI67_HW_CONFIG);
    reg &= ~BIT(0); /* Clear enable bit */
    iowrite32(reg, hw->regs + WIFI67_HW_CONFIG);

    hw->state = WIFI67_HW_OFF;
}

void wifi67_hw_irq_enable(struct wifi67_priv *priv)
{
    struct wifi67_hw *hw = &priv->hal;
    unsigned long flags;

    spin_lock_irqsave(&hw->irq_lock, flags);
    
    /* Set interrupt mask */
    hw->irq_mask = 0xFFFFFFFF; /* Enable all interrupts */
    iowrite32(hw->irq_mask, hw->regs + WIFI67_HW_INT_MASK);
    
    spin_unlock_irqrestore(&hw->irq_lock, flags);
}

void wifi67_hw_irq_disable(struct wifi67_priv *priv)
{
    struct wifi67_hw *hw = &priv->hal;
    unsigned long flags;

    spin_lock_irqsave(&hw->irq_lock, flags);
    
    /* Clear interrupt mask */
    hw->irq_mask = 0;
    iowrite32(0, hw->regs + WIFI67_HW_INT_MASK);
    
    spin_unlock_irqrestore(&hw->irq_lock, flags);
}

irqreturn_t wifi67_hw_interrupt(int irq, void *data)
{
    struct wifi67_priv *priv = data;
    struct wifi67_hw *hw = &priv->hal;
    u32 status;
    irqreturn_t ret = IRQ_NONE;

    /* Read interrupt status */
    status = ioread32(hw->regs + WIFI67_HW_INT_STATUS);
    if (!status)
        return IRQ_NONE;

    /* Clear interrupts */
    iowrite32(status, hw->regs + WIFI67_HW_INT_STATUS);

    /* Handle DMA interrupts */
    if (status & 0xFF000000) {
        ret = wifi67_dma_isr(priv);
        if (ret == IRQ_HANDLED)
            return ret;
    }

    return IRQ_HANDLED;
} 