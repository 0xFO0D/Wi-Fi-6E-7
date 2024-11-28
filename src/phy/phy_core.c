#include <linux/delay.h>
#include <linux/bitfield.h>
#include "../../include/phy/phy_regs.h"
#include "../../include/phy/phy_core.h"

#define PHY_CALIBRATION_RETRY_MAX 3
#define PHY_PLL_LOCK_TIMEOUT_MS   50

static inline u32 phy_read32(struct wifi67_priv *priv, u32 reg)
{
    return readl(priv->mmio + reg);
}

static inline void phy_write32(struct wifi67_priv *priv, u32 reg, u32 val)
{
    writel(val, priv->mmio + reg);
}

static int wifi67_phy_calibrate(struct wifi67_priv *priv)
{
    struct phy_calibration cal;
    u32 val, retry = 0;
    int ret = 0;

    /* Start calibration sequence */
    val = phy_read32(priv, PHY_CALIBRATION);
    val |= BIT(0);  /* Trigger calibration */
    phy_write32(priv, PHY_CALIBRATION, val);

    do {
        msleep(10);
        val = phy_read32(priv, PHY_CALIBRATION);
        if (!(val & BIT(0)))
            break;
        
        if (++retry >= PHY_CALIBRATION_RETRY_MAX) {
            ret = -ETIMEDOUT;
            goto out;
        }
    } while (1);

    /* Store calibration results */
    cal.tx_iq_cal = phy_read32(priv, PHY_CALIBRATION + 0x4);
    cal.rx_iq_cal = phy_read32(priv, PHY_CALIBRATION + 0x8);
    cal.tx_dc_offset = phy_read32(priv, PHY_CALIBRATION + 0xC);
    cal.rx_dc_offset = phy_read32(priv, PHY_CALIBRATION + 0x10);
    cal.pll_cal = phy_read32(priv, PHY_CALIBRATION + 0x14);
    cal.temp_comp = phy_read32(priv, PHY_CALIBRATION + 0x18);

    memcpy(&priv->phy.cal, &cal, sizeof(cal));

out:
    return ret;
}

static int wifi67_phy_configure_pll(struct wifi67_priv *priv, u32 freq)
{
    u32 val;
    int timeout = PHY_PLL_LOCK_TIMEOUT_MS;

    /* Program PLL frequency */
    val = FIELD_PREP(PLL_FREQ_MASK, freq);
    phy_write32(priv, PHY_PLL_CTRL, val);

    /* Wait for PLL lock */
    do {
        msleep(1);
        val = phy_read32(priv, PHY_PLL_CTRL);
        if (val & PLL_LOCK_BIT)
            return 0;
    } while (--timeout > 0);

    return -ETIMEDOUT;
}

int wifi67_phy_init(struct wifi67_priv *priv)
{
    u32 val;
    int ret;

    /* Reset PHY */
    val = PHY_CFG_RESET;
    phy_write32(priv, PHY_CTRL_CONFIG, val);
    msleep(1);
    
    /* Configure PHY features */
    val = PHY_CFG_ENABLE | PHY_CFG_320MHZ | PHY_CFG_4K_QAM | PHY_CFG_MLO;
    phy_write32(priv, PHY_CTRL_CONFIG, val);

    /* Initialize AGC */
    val = FIELD_PREP(AGC_GAIN_MASK, 0x40) |
          FIELD_PREP(AGC_THRESHOLD_MASK, 0x80);
    phy_write32(priv, PHY_AGC_CTRL, val);

    /* Configure initial RF parameters */
    ret = wifi67_phy_configure_pll(priv, 5180); /* Start at 5.18GHz */
    if (ret)
        goto err;

    /* Perform initial calibration */
    ret = wifi67_phy_calibrate(priv);
    if (ret)
        goto err;

    return 0;

err:
    val = PHY_CFG_RESET;
    phy_write32(priv, PHY_CTRL_CONFIG, val);
    return ret;
}

void wifi67_phy_deinit(struct wifi67_priv *priv)
{
    u32 val = PHY_CFG_RESET;
    phy_write32(priv, PHY_CTRL_CONFIG, val);
}

int wifi67_phy_start(struct wifi67_priv *priv)
{
    u32 val = phy_read32(priv, PHY_CTRL_CONFIG);
    val |= PHY_CFG_ENABLE;
    phy_write32(priv, PHY_CTRL_CONFIG, val);
    return 0;
}

void wifi67_phy_stop(struct wifi67_priv *priv)
{
    u32 val = phy_read32(priv, PHY_CTRL_CONFIG);
    val &= ~PHY_CFG_ENABLE;
    phy_write32(priv, PHY_CTRL_CONFIG, val);
}

int wifi67_phy_config_channel(struct wifi67_priv *priv, u32 freq, u32 bandwidth)
{
    int ret;

    /* Configure PLL for new frequency */
    ret = wifi67_phy_configure_pll(priv, freq);
    if (ret)
        return ret;

    /* Recalibrate for new frequency */
    return wifi67_phy_calibrate(priv);
}

int wifi67_phy_set_txpower(struct wifi67_priv *priv, int dbm)
{
    u32 val;
    
    /* Convert dBm to hardware gain value */
    val = min_t(int, dbm * 2, 0xFF);
    phy_write32(priv, PHY_TX_GAIN_CTRL, val);
    
    return 0;
}

int wifi67_phy_get_rssi(struct wifi67_priv *priv)
{
    u32 val = phy_read32(priv, PHY_RX_GAIN_CTRL);
    return -(val & 0xFF); /* Convert to negative dBm */
} 