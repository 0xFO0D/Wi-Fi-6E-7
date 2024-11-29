#include <linux/delay.h>
#include <linux/jiffies.h>
#include "../../include/hal/hardware.h"
#include "../../include/core/wifi67.h"

static inline u32 hw_read32(struct wifi67_priv *priv, u32 reg)
{
    return readl(priv->mmio + reg);
}

static inline void hw_write32(struct wifi67_priv *priv, u32 reg, u32 val)
{
    writel(val, priv->mmio + reg);
}

int wifi67_hw_wait_for_bit(struct wifi67_priv *priv, u32 reg,
                          u32 bit, int timeout)
{
    unsigned long end = jiffies + msecs_to_jiffies(timeout);
    u32 val;

    do {
        val = hw_read32(priv, reg);
        if (val & bit)
            return 0;
        udelay(10);
    } while (time_before(jiffies, end));

    return -ETIMEDOUT;
}

int wifi67_hw_init(struct wifi67_priv *priv)
{
    u32 val;

    /* Check hardware version */
    val = hw_read32(priv, HW_REG_VERSION);
    if (val != 0x67670001) {
        pr_err("wifi67: Invalid hardware version 0x%08x\n", val);
        return -ENODEV;
    }

    /* Reset hardware */
    if (wifi67_hw_reset(priv))
        return -EIO;

    /* Enable interrupts */
    hw_write32(priv, HW_REG_INT_MASK, 0);
    hw_write32(priv, HW_REG_CONTROL, HW_CTRL_INT_EN);

    return 0;
}

void wifi67_hw_deinit(struct wifi67_priv *priv)
{
    /* Disable all interrupts */
    hw_write32(priv, HW_REG_INT_MASK, 0xFFFFFFFF);
    hw_write32(priv, HW_REG_CONTROL, 0);

    /* Put hardware into reset */
    hw_write32(priv, HW_REG_RESET, HW_CTRL_RESET);
}

int wifi67_hw_start(struct wifi67_priv *priv)
{
    u32 val;

    /* Enable RX/TX */
    val = hw_read32(priv, HW_REG_CONTROL);
    val |= HW_CTRL_ENABLE | HW_CTRL_RX_EN | HW_CTRL_TX_EN;
    hw_write32(priv, HW_REG_CONTROL, val);

    /* Wait for ready */
    return wifi67_hw_wait_for_bit(priv, HW_REG_STATUS, 
                                 HW_STATUS_READY, 1000);
}

void wifi67_hw_stop(struct wifi67_priv *priv)
{
    u32 val;

    /* Disable RX/TX */
    val = hw_read32(priv, HW_REG_CONTROL);
    val &= ~(HW_CTRL_RX_EN | HW_CTRL_TX_EN);
    hw_write32(priv, HW_REG_CONTROL, val);

    /* Wait for idle */
    wifi67_hw_wait_for_bit(priv, HW_REG_STATUS,
                          HW_STATUS_RX_ACTIVE | HW_STATUS_TX_ACTIVE, 1000);
}

int wifi67_hw_reset(struct wifi67_priv *priv)
{
    /* Assert reset */
    hw_write32(priv, HW_REG_RESET, HW_CTRL_RESET);
    udelay(100);

    /* Deassert reset */
    hw_write32(priv, HW_REG_RESET, 0);
    udelay(100);

    return wifi67_hw_wait_for_bit(priv, HW_REG_STATUS, 
                                 HW_STATUS_READY, 1000);
}

int wifi67_hw_set_power(struct wifi67_priv *priv, bool enable)
{
    u32 val;

    val = hw_read32(priv, HW_REG_CONTROL);
    if (enable)
        val &= ~HW_CTRL_SLEEP;
    else
        val |= HW_CTRL_SLEEP;
    hw_write32(priv, HW_REG_CONTROL, val);

    return wifi67_hw_wait_for_bit(priv, HW_REG_STATUS,
                                 HW_STATUS_SLEEP, 1000);
}

u32 wifi67_hw_get_status(struct wifi67_priv *priv)
{
    return hw_read32(priv, HW_REG_STATUS);
}

void wifi67_hw_enable_interrupts(struct wifi67_priv *priv)
{
    /* Enable all interrupts */
    hw_write32(priv, HW_REG_INT_MASK, 0);
}

void wifi67_hw_disable_interrupts(struct wifi67_priv *priv)
{
    /* Disable all interrupts */
    hw_write32(priv, HW_REG_INT_MASK, 0xFFFFFFFF);
}

void wifi67_hw_handle_interrupt(struct wifi67_priv *priv)
{
    u32 status = hw_read32(priv, HW_REG_INT_STATUS);
    u32 handled = 0;

    if (status & HW_INT_RX_DONE) {
        /* Handle RX completion */
        handled |= HW_INT_RX_DONE;
    }

    if (status & HW_INT_TX_DONE) {
        /* Handle TX completion */
        handled |= HW_INT_TX_DONE;
    }

    if (status & HW_INT_RX_ERROR) {
        /* Handle RX error */
        handled |= HW_INT_RX_ERROR;
    }

    if (status & HW_INT_TX_ERROR) {
        /* Handle TX error */
        handled |= HW_INT_TX_ERROR;
    }

    if (status & HW_INT_TEMP_WARNING) {
        /* Handle temperature warning */
        handled |= HW_INT_TEMP_WARNING;
    }

    if (status & HW_INT_RADAR_DETECT) {
        /* Handle radar detection */
        handled |= HW_INT_RADAR_DETECT;
    }

    /* Clear handled interrupts */
    hw_write32(priv, HW_REG_INT_STATUS, handled);
} 