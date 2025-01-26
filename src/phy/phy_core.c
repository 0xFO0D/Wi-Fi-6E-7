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

    /* Clear reset and enable PHY with advanced features */
    val = WIFI67_PHY_CTRL_ENABLE | WIFI67_PHY_CTRL_AGC_EN | 
          WIFI67_PHY_CTRL_CALIB_EN | WIFI67_PHY_CTRL_4K_QAM_EN |
          WIFI67_PHY_CTRL_320M_EN;
    writel(val, phy->regs + WIFI67_PHY_REG_CTRL);

    /* Wait for PHY to be ready */
    while (retry--) {
        val = readl(phy->regs + WIFI67_PHY_REG_STATUS);
        if ((val & WIFI67_PHY_STATUS_READY) && 
            (val & WIFI67_PHY_STATUS_4K_READY) &&
            (val & WIFI67_PHY_STATUS_320M_READY))
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
    writel(WIFI67_CHAN_WIDTH_160, phy->regs + WIFI67_PHY_REG_CHAN_WIDTH);
    phy->current_channel = 36;
    phy->current_band = NL80211_BAND_5GHZ;
    phy->chan_width = WIFI67_CHAN_WIDTH_160;

    /* Enable antenna diversity */
    writel(0x3, phy->regs + WIFI67_PHY_REG_ANTENNA);

    /* Configure QAM mode to auto */
    writel(WIFI67_QAM_MODE_AUTO, phy->regs + WIFI67_PHY_REG_QAM_CTRL);
    phy->qam_mode = WIFI67_QAM_MODE_AUTO;

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
    phy->mlo_enabled = false;
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

    /* Disable PHY and all features */
    writel(0, phy->regs + WIFI67_PHY_REG_CTRL);
    writel(0, phy->regs + WIFI67_PHY_REG_QAM_CTRL);
    writel(0, phy->regs + WIFI67_PHY_REG_MLO_CTRL);

    /* Clear any pending calibration */
    writel(0, phy->regs + WIFI67_PHY_REG_CALIBRATION);

    phy->enabled = false;
    phy->mlo_enabled = false;

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
    val |= WIFI67_PHY_CTRL_ENABLE | WIFI67_PHY_CTRL_4K_QAM_EN |
           WIFI67_PHY_CTRL_320M_EN;
    
    if (phy->mlo_enabled)
        val |= WIFI67_PHY_CTRL_MLO_EN;
        
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
    val &= ~(WIFI67_PHY_CTRL_ENABLE | WIFI67_PHY_CTRL_AGC_EN |
             WIFI67_PHY_CTRL_4K_QAM_EN | WIFI67_PHY_CTRL_320M_EN |
             WIFI67_PHY_CTRL_MLO_EN);
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

    /* Configure 6GHz specific settings if needed */
    if (band == NL80211_BAND_6GHZ) {
        val = readl(phy->regs + WIFI67_PHY_REG_6G_CTRL);
        val |= BIT(0); /* Enable 6GHz mode */
        writel(val, phy->regs + WIFI67_PHY_REG_6G_CTRL);
    }

    /* Trigger recalibration */
    val = readl(phy->regs + WIFI67_PHY_REG_CTRL);
    val |= WIFI67_PHY_CTRL_CALIB_EN;
    writel(val, phy->regs + WIFI67_PHY_REG_CTRL);

    spin_unlock_irqrestore(&phy->lock, flags);

    return 0;
}

int wifi67_phy_set_bandwidth(struct wifi67_priv *priv, u32 width)
{
    struct wifi67_phy *phy = priv->phy_dev;
    unsigned long flags;
    u32 val;

    if (!phy || !phy->enabled)
        return -EINVAL;

    if (width > WIFI67_CHAN_WIDTH_320)
        return -EINVAL;

    spin_lock_irqsave(&phy->lock, flags);

    /* Set channel width */
    writel(width, phy->regs + WIFI67_PHY_REG_CHAN_WIDTH);
    phy->chan_width = width;

    /* Enable 320MHz mode if needed */
    val = readl(phy->regs + WIFI67_PHY_REG_CTRL);
    if (width == WIFI67_CHAN_WIDTH_320)
        val |= WIFI67_PHY_CTRL_320M_EN;
    else
        val &= ~WIFI67_PHY_CTRL_320M_EN;
    writel(val, phy->regs + WIFI67_PHY_REG_CTRL);

    /* Trigger recalibration */
    val |= WIFI67_PHY_CTRL_CALIB_EN;
    writel(val, phy->regs + WIFI67_PHY_REG_CTRL);

    spin_unlock_irqrestore(&phy->lock, flags);

    return 0;
}

int wifi67_phy_set_qam_mode(struct wifi67_priv *priv, u32 mode)
{
    struct wifi67_phy *phy = priv->phy_dev;
    unsigned long flags;
    u32 val;

    if (!phy || !phy->enabled)
        return -EINVAL;

    if (mode > WIFI67_QAM_MODE_4096)
        return -EINVAL;

    spin_lock_irqsave(&phy->lock, flags);

    /* Set QAM mode */
    writel(mode, phy->regs + WIFI67_PHY_REG_QAM_CTRL);
    phy->qam_mode = mode;

    /* Enable 4K QAM if needed */
    val = readl(phy->regs + WIFI67_PHY_REG_CTRL);
    if (mode == WIFI67_QAM_MODE_4096)
        val |= WIFI67_PHY_CTRL_4K_QAM_EN;
    else
        val &= ~WIFI67_PHY_CTRL_4K_QAM_EN;
    writel(val, phy->regs + WIFI67_PHY_REG_CTRL);

    spin_unlock_irqrestore(&phy->lock, flags);

    return 0;
}

int wifi67_phy_enable_mlo(struct wifi67_priv *priv, bool enable)
{
    struct wifi67_phy *phy = priv->phy_dev;
    unsigned long flags;
    u32 val;

    if (!phy || !phy->enabled)
        return -EINVAL;

    spin_lock_irqsave(&phy->lock, flags);

    val = readl(phy->regs + WIFI67_PHY_REG_CTRL);
    if (enable)
        val |= WIFI67_PHY_CTRL_MLO_EN;
    else
        val &= ~WIFI67_PHY_CTRL_MLO_EN;
    writel(val, phy->regs + WIFI67_PHY_REG_CTRL);

    /* Configure MLO specific settings */
    val = enable ? BIT(0) : 0; /* Enable/disable MLO mode */
    writel(val, phy->regs + WIFI67_PHY_REG_MLO_CTRL);

    phy->mlo_enabled = enable;

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
EXPORT_SYMBOL_GPL(wifi67_phy_set_bandwidth);
EXPORT_SYMBOL_GPL(wifi67_phy_set_qam_mode);
EXPORT_SYMBOL_GPL(wifi67_phy_enable_mlo); 