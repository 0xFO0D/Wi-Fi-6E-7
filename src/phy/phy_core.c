#include <linux/delay.h>
#include <linux/kernel.h>
#include "../../include/phy/phy_core.h"
#include "../../include/core/wifi67.h"

/* Forward declaration of wifi67_priv structure */
struct wifi67_priv;

static inline u32 phy_read32(struct wifi67_priv *priv, u32 reg)
{
    if (!priv || !priv->mmio)
        return 0;
    return readl(priv->mmio + reg);
}

static inline void phy_write32(struct wifi67_priv *priv, u32 reg, u32 val)
{
    if (!priv || !priv->mmio)
        return;
    writel(val, priv->mmio + reg);
}

static int wifi67_phy_wait_for_bit(struct wifi67_priv *priv, u32 reg,
                                  u32 bit, bool set, int timeout)
{
    unsigned long end = jiffies + msecs_to_jiffies(timeout);
    u32 val;

    do {
        val = phy_read32(priv, reg);
        if (set) {
            if (val & bit)
                return 0;
        } else {
            if (!(val & bit))
                return 0;
        }
        udelay(10);
    } while (time_before(jiffies, end));

    return -ETIMEDOUT;
}

int wifi67_phy_calibrate(struct wifi67_priv *priv)
{
    struct phy_calibration cal;
    int ret, i;

    /* Start calibration */
    phy_write32(priv, PHY_CONTROL, PHY_CTRL_CALIB);

    /* Wait for calibration to complete */
    ret = wifi67_phy_wait_for_bit(priv, PHY_STATUS, PHY_STATUS_CAL, false, 1000);
    if (ret)
        return ret;

    /* Read calibration data for each chain */
    for (i = 0; i < 4; i++) {
        cal.per_chain[i].tx_iq_cal = phy_read32(priv, PHY_CALIBRATION + 0x4 + (i * 0x20));
        cal.per_chain[i].rx_iq_cal = phy_read32(priv, PHY_CALIBRATION + 0x8 + (i * 0x20));
        cal.per_chain[i].tx_dc_offset = phy_read32(priv, PHY_CALIBRATION + 0xC + (i * 0x20));
        cal.per_chain[i].rx_dc_offset = phy_read32(priv, PHY_CALIBRATION + 0x10 + (i * 0x20));
        cal.per_chain[i].pll_cal = phy_read32(priv, PHY_CALIBRATION + 0x14 + (i * 0x20));
        cal.per_chain[i].temp_comp = phy_read32(priv, PHY_CALIBRATION + 0x18 + (i * 0x20));
    }

    cal.last_cal_time = jiffies;
    cal.cal_flags = PHY_CAL_VALID;

    /* Store calibration data */
    memcpy(&priv->phy.cal, &cal, sizeof(cal));

    return 0;
}

int wifi67_phy_init(struct wifi67_priv *priv)
{
    if (!priv)
        return -EINVAL;

    spin_lock_init(&priv->phy.lock);

    /* Initialize PHY registers */
    phy_write32(priv, PHY_CONTROL, PHY_CTRL_RESET);
    msleep(10);
    phy_write32(priv, PHY_CONTROL, 0);

    /* Perform initial calibration */
    return wifi67_phy_calibrate(priv);
}

void wifi67_phy_deinit(struct wifi67_priv *priv)
{
    if (!priv)
        return;

    /* Put PHY in reset state */
    phy_write32(priv, PHY_CONTROL, PHY_CTRL_RESET);
}

int wifi67_phy_start(struct wifi67_priv *priv)
{
    u32 val;
    int ret;

    if (!priv)
        return -EINVAL;

    /* Enable PHY */
    val = PHY_CTRL_ENABLE | PHY_CTRL_TX | PHY_CTRL_RX;
    phy_write32(priv, PHY_CONTROL, val);

    /* Wait for PHY to be ready */
    ret = wifi67_phy_wait_for_bit(priv, PHY_STATUS, PHY_STATUS_READY, true, 1000);
    if (ret)
        return ret;

    return 0;
}

void wifi67_phy_stop(struct wifi67_priv *priv)
{
    if (!priv)
        return;

    /* Disable PHY */
    phy_write32(priv, PHY_CONTROL, 0);
}

int wifi67_phy_config_channel(struct wifi67_priv *priv, u32 freq, u32 bandwidth)
{
    if (!priv)
        return -EINVAL;

    spin_lock(&priv->phy.lock);
    priv->phy.current_channel = freq;
    priv->phy.current_bandwidth = bandwidth;
    spin_unlock(&priv->phy.lock);

    /* Configure PHY for new channel */
    phy_write32(priv, PHY_CHANNEL, freq);
    
    return 0;
}

int wifi67_phy_set_txpower(struct wifi67_priv *priv, int dbm)
{
    if (!priv)
        return -EINVAL;

    if (dbm < 0 || dbm > 30)
        return -EINVAL;

    spin_lock(&priv->phy.lock);
    priv->phy.current_txpower = dbm;
    spin_unlock(&priv->phy.lock);

    /* Set TX power */
    phy_write32(priv, PHY_TXPOWER, dbm);

    return 0;
}

int wifi67_phy_get_rssi(struct wifi67_priv *priv)
{
    u32 val;

    if (!priv)
        return -EINVAL;

    val = phy_read32(priv, PHY_AGC);
    return -(val & 0xFF); /* Convert to negative dBm */
} 