#include <linux/module.h>
#include <linux/pci.h>
#include <net/mac80211.h>

#include "../../include/phy/phy_core.h"
#include "../../include/core/wifi67.h"

/* Register definitions */
#define WIFI67_PHY_CTRL        0x0000
#define WIFI67_PHY_STATUS      0x0004
#define WIFI67_PHY_CONFIG      0x0008
#define WIFI67_PHY_POWER      0x000C

/* Control register bits */
#define PHY_CTRL_ENABLE       BIT(0)
#define PHY_CTRL_RESET        BIT(1)
#define PHY_CTRL_TX_EN        BIT(2)
#define PHY_CTRL_RX_EN        BIT(3)
#define PHY_CTRL_PS_EN        BIT(4)

static void wifi67_phy_hw_init(struct wifi67_priv *priv)
{
    struct wifi67_phy *phy = &priv->phy;
    u32 reg;

    /* Reset PHY */
    iowrite32(PHY_CTRL_RESET, phy->regs + WIFI67_PHY_CTRL);
    udelay(100);
    
    /* Clear reset and enable PHY */
    reg = PHY_CTRL_ENABLE | PHY_CTRL_TX_EN | PHY_CTRL_RX_EN;
    iowrite32(reg, phy->regs + WIFI67_PHY_CTRL);
    
    /* Set default configuration */
    reg = WIFI67_PHY_CAP_BE | WIFI67_PHY_CAP_320MHZ | 
          WIFI67_PHY_CAP_4K_QAM | WIFI67_PHY_CAP_MULTI_RU;
    iowrite32(reg, phy->regs + WIFI67_PHY_CONFIG);
}

int wifi67_phy_init(struct wifi67_priv *priv)
{
    struct wifi67_phy *phy = &priv->phy;
    
    /* Initialize locks */
    spin_lock_init(&phy->lock);
    
    /* Map PHY registers */
    phy->regs = priv->mmio + 0x2000;  /* PHY register offset */
    
    /* Set initial state */
    phy->state = WIFI67_PHY_OFF;
    phy->ps_enabled = false;
    
    /* Initialize hardware */
    wifi67_phy_hw_init(priv);
    
    return 0;
}

void wifi67_phy_deinit(struct wifi67_priv *priv)
{
    struct wifi67_phy *phy = &priv->phy;
    
    /* Disable PHY */
    iowrite32(0, phy->regs + WIFI67_PHY_CTRL);
    
    /* Reset statistics */
    memset(&phy->stats, 0, sizeof(phy->stats));
}

int wifi67_phy_start(struct wifi67_priv *priv)
{
    struct wifi67_phy *phy = &priv->phy;
    u32 reg;
    
    spin_lock(&phy->lock);
    
    /* Enable PHY */
    reg = ioread32(phy->regs + WIFI67_PHY_CTRL);
    reg |= PHY_CTRL_ENABLE | PHY_CTRL_TX_EN | PHY_CTRL_RX_EN;
    iowrite32(reg, phy->regs + WIFI67_PHY_CTRL);
    
    phy->state = WIFI67_PHY_READY;
    
    spin_unlock(&phy->lock);
    
    return 0;
}

void wifi67_phy_stop(struct wifi67_priv *priv)
{
    struct wifi67_phy *phy = &priv->phy;
    
    spin_lock(&phy->lock);
    
    /* Disable PHY */
    iowrite32(0, phy->regs + WIFI67_PHY_CTRL);
    phy->state = WIFI67_PHY_OFF;
    
    spin_unlock(&phy->lock);
}

int wifi67_phy_config(struct wifi67_priv *priv, struct ieee80211_conf *conf)
{
    struct wifi67_phy *phy = &priv->phy;
    u32 reg;
    
    spin_lock(&phy->lock);
    
    /* Update PHY configuration */
    reg = ioread32(phy->regs + WIFI67_PHY_CONFIG);
    
    if (conf->chandef.width >= NL80211_CHAN_WIDTH_320)
        reg |= WIFI67_PHY_CAP_320MHZ;
    
    iowrite32(reg, phy->regs + WIFI67_PHY_CONFIG);
    
    spin_unlock(&phy->lock);
    
    return 0;
}

void wifi67_phy_set_power(struct wifi67_priv *priv, bool enable)
{
    struct wifi67_phy *phy = &priv->phy;
    u32 reg;
    
    spin_lock(&phy->lock);
    
    reg = ioread32(phy->regs + WIFI67_PHY_CTRL);
    
    if (enable)
        reg &= ~PHY_CTRL_PS_EN;
    else
        reg |= PHY_CTRL_PS_EN;
    
    iowrite32(reg, phy->regs + WIFI67_PHY_CTRL);
    phy->ps_enabled = !enable;
    
    spin_unlock(&phy->lock);
} 