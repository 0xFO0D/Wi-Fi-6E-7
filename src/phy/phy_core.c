#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include "../../include/phy/phy_core.h"
#include "../../include/mac/mac_core.h"

#define WIFI67_PHY_MIN_RSSI -100
#define WIFI67_PHY_MAX_RSSI -10
#define WIFI67_PHY_MIN_POWER 0
#define WIFI67_PHY_MAX_POWER 30

static int wifi67_phy_hw_init(struct wifi67_priv *priv)
{
    struct wifi67_phy *phy = priv->phy_dev;
    u32 val;
    int retry = 100;

    /* Reset PHY */
    writel(WIFI67_PHY_CTRL_RESET, phy->regs + WIFI67_PHY_REG_CTRL);
    udelay(100);

    /* Clear reset and enable PHY */
    val = WIFI67_PHY_CTRL_ENABLE | WIFI67_PHY_CTRL_AGC_EN | WIFI67_PHY_CTRL_CALIB_EN;
    writel(val, phy->regs + WIFI67_PHY_REG_CTRL);

    /* Wait for PHY to be ready */
    while (retry--) {
        val = readl(phy->regs + WIFI67_PHY_REG_STATUS);
        if (val & WIFI67_PHY_STATUS_READY)
            break;
        udelay(100);
    }

    if (retry < 0) {
        pr_err("PHY failed to become ready\n");
        return -ETIMEDOUT;
    }

    /* Initialize AGC */
    writel(0x50, phy->regs + WIFI67_PHY_REG_AGC);
    phy->agc_gain = 0x50;

    /* Set default power level */
    writel(0x1F, phy->regs + WIFI67_PHY_REG_POWER);
    phy->current_power = 0x1F;

    /* Set default channel and bandwidth */
    writel(36, phy->regs + WIFI67_PHY_REG_CHANNEL);
    writel(0, phy->regs + WIFI67_PHY_REG_BANDWIDTH);
    phy->current_channel = 36;
    phy->current_band = NL80211_BAND_5GHZ;

    /* Enable antenna diversity */
    writel(0x3, phy->regs + WIFI67_PHY_REG_ANTENNA);

    /* Start calibration */
    val = readl(phy->regs + WIFI67_PHY_REG_CTRL);
    val |= WIFI67_PHY_CTRL_CALIB_EN;
    writel(val, phy->regs + WIFI67_PHY_REG_CTRL);

    /* Wait for calibration to complete */
    retry = 100;
    while (retry--) {
        val = readl(phy->regs + WIFI67_PHY_REG_STATUS);
        if (val & WIFI67_PHY_STATUS_CALIB_DONE)
            break;
        udelay(100);
    }

    if (retry < 0) {
        pr_err("PHY calibration failed\n");
        return -ETIMEDOUT;
    }

    phy->enabled = true;
    return 0;
}

int wifi67_phy_init(struct wifi67_priv *priv)
{
    struct wifi67_phy *phy;
    int ret;

    phy = kzalloc(sizeof(*phy), GFP_KERNEL);
    if (!phy)
        return -ENOMEM;

    priv->phy_dev = phy;
    spin_lock_init(&phy->lock);

    phy->regs = priv->hw_info->membase + priv->hw_info->phy_offset;

    ret = wifi67_phy_hw_init(priv);
    if (ret)
        goto err_free_phy;

    return 0;

err_free_phy:
    kfree(phy);
    priv->phy_dev = NULL;
    return ret;
}

void wifi67_phy_deinit(struct wifi67_priv *priv)
{
    struct wifi67_phy *phy = priv->phy_dev;
    unsigned long flags;

    if (!phy)
        return;

    spin_lock_irqsave(&phy->lock, flags);

    /* Disable PHY */
    writel(0, phy->regs + WIFI67_PHY_REG_CTRL);

    /* Clear any pending calibration */
    writel(0, phy->regs + WIFI67_PHY_REG_CALIBRATION);

    phy->enabled = false;

    spin_unlock_irqrestore(&phy->lock, flags);

    kfree(phy);
    priv->phy_dev = NULL;
}

int wifi67_phy_start(struct wifi67_priv *priv)
{
    struct wifi67_phy *phy = priv->phy_dev;
    unsigned long flags;
    u32 val;

    if (!phy)
        return -EINVAL;

    spin_lock_irqsave(&phy->lock, flags);

    val = readl(phy->regs + WIFI67_PHY_REG_CTRL);
    val |= WIFI67_PHY_CTRL_ENABLE;
    writel(val, phy->regs + WIFI67_PHY_REG_CTRL);

    /* Enable AGC */
    val |= WIFI67_PHY_CTRL_AGC_EN;
    writel(val, phy->regs + WIFI67_PHY_REG_CTRL);

    phy->enabled = true;

    spin_unlock_irqrestore(&phy->lock, flags);

    return 0;
}

void wifi67_phy_stop(struct wifi67_priv *priv)
{
    struct wifi67_phy *phy = priv->phy_dev;
    unsigned long flags;
    u32 val;

    if (!phy)
        return;

    spin_lock_irqsave(&phy->lock, flags);

    val = readl(phy->regs + WIFI67_PHY_REG_CTRL);
    val &= ~(WIFI67_PHY_CTRL_ENABLE | WIFI67_PHY_CTRL_AGC_EN);
    writel(val, phy->regs + WIFI67_PHY_REG_CTRL);

    phy->enabled = false;

    spin_unlock_irqrestore(&phy->lock, flags);
}

int wifi67_phy_config(struct wifi67_priv *priv, u32 channel, u32 band)
{
    struct wifi67_phy *phy = priv->phy_dev;
    unsigned long flags;
    u32 val;

    if (!phy || !phy->enabled)
        return -EINVAL;

    spin_lock_irqsave(&phy->lock, flags);

    /* Set channel */
    writel(channel, phy->regs + WIFI67_PHY_REG_CHANNEL);
    phy->current_channel = channel;
    phy->current_band = band;

    /* Trigger recalibration */
    val = readl(phy->regs + WIFI67_PHY_REG_CTRL);
    val |= WIFI67_PHY_CTRL_CALIB_EN;
    writel(val, phy->regs + WIFI67_PHY_REG_CTRL);

    spin_unlock_irqrestore(&phy->lock, flags);

    return 0;
}

int wifi67_phy_set_power(struct wifi67_priv *priv, u32 power_level)
{
    struct wifi67_phy *phy = priv->phy_dev;
    unsigned long flags;

    if (!phy || !phy->enabled)
        return -EINVAL;

    if (power_level > WIFI67_PHY_MAX_POWER)
        return -EINVAL;

    spin_lock_irqsave(&phy->lock, flags);

    writel(power_level, phy->regs + WIFI67_PHY_REG_POWER);
    phy->current_power = power_level;

    spin_unlock_irqrestore(&phy->lock, flags);

    return 0;
}

int wifi67_phy_get_rssi(struct wifi67_priv *priv)
{
    struct wifi67_phy *phy = priv->phy_dev;
    unsigned long flags;
    s32 rssi;

    if (!phy || !phy->enabled)
        return WIFI67_PHY_MIN_RSSI;

    spin_lock_irqsave(&phy->lock, flags);

    rssi = readl(phy->regs + WIFI67_PHY_REG_RSSI);
    /* Convert raw RSSI to dBm */
    rssi = -90 + ((rssi & 0xFF) / 2);

    spin_unlock_irqrestore(&phy->lock, flags);

    return rssi;
}

EXPORT_SYMBOL_GPL(wifi67_phy_init);
EXPORT_SYMBOL_GPL(wifi67_phy_deinit);
EXPORT_SYMBOL_GPL(wifi67_phy_start);
EXPORT_SYMBOL_GPL(wifi67_phy_stop);
EXPORT_SYMBOL_GPL(wifi67_phy_config);
EXPORT_SYMBOL_GPL(wifi67_phy_set_power);
EXPORT_SYMBOL_GPL(wifi67_phy_get_rssi); 