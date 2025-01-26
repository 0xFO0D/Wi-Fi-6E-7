#ifndef _WIFI67_PHY_CORE_H_
#define _WIFI67_PHY_CORE_H_

#include <linux/types.h>
#include "../core/wifi67.h"

#define WIFI67_PHY_REG_CTRL          0x0000
#define WIFI67_PHY_REG_STATUS        0x0004
#define WIFI67_PHY_REG_AGC           0x0008
#define WIFI67_PHY_REG_RSSI          0x000C
#define WIFI67_PHY_REG_POWER         0x0010
#define WIFI67_PHY_REG_CHANNEL       0x0014
#define WIFI67_PHY_REG_BANDWIDTH     0x0018
#define WIFI67_PHY_REG_ANTENNA       0x001C
#define WIFI67_PHY_REG_CALIBRATION   0x0020
#define WIFI67_PHY_REG_QAM_CTRL      0x0024
#define WIFI67_PHY_REG_MLO_CTRL      0x0028
#define WIFI67_PHY_REG_CHAN_WIDTH    0x002C
#define WIFI67_PHY_REG_6G_CTRL       0x0030

/* PHY Control Register bits */
#define WIFI67_PHY_CTRL_ENABLE       BIT(0)
#define WIFI67_PHY_CTRL_RESET        BIT(1)
#define WIFI67_PHY_CTRL_AGC_EN       BIT(2)
#define WIFI67_PHY_CTRL_CALIB_EN     BIT(3)
#define WIFI67_PHY_CTRL_SLEEP        BIT(4)
#define WIFI67_PHY_CTRL_4K_QAM_EN    BIT(5)
#define WIFI67_PHY_CTRL_320M_EN      BIT(6)
#define WIFI67_PHY_CTRL_MLO_EN       BIT(7)

/* PHY Status Register bits */
#define WIFI67_PHY_STATUS_READY      BIT(0)
#define WIFI67_PHY_STATUS_AGC_DONE   BIT(1)
#define WIFI67_PHY_STATUS_CALIB_DONE BIT(2)
#define WIFI67_PHY_STATUS_PLL_LOCK   BIT(3)
#define WIFI67_PHY_STATUS_4K_READY   BIT(4)
#define WIFI67_PHY_STATUS_320M_READY BIT(5)

/* Channel width definitions */
#define WIFI67_CHAN_WIDTH_20       0
#define WIFI67_CHAN_WIDTH_40       1
#define WIFI67_CHAN_WIDTH_80       2
#define WIFI67_CHAN_WIDTH_160      3
#define WIFI67_CHAN_WIDTH_320      4

/* QAM modes */
#define WIFI67_QAM_MODE_AUTO       0
#define WIFI67_QAM_MODE_1024       1
#define WIFI67_QAM_MODE_4096       2

struct wifi67_phy {
    void __iomem *regs;
    u32 current_channel;
    u32 current_band;
    u32 current_power;
    u32 agc_gain;
    u32 qam_mode;
    u32 chan_width;
    bool enabled;
    bool mlo_enabled;
    spinlock_t lock;
};

int wifi67_phy_init(struct wifi67_priv *priv);
void wifi67_phy_deinit(struct wifi67_priv *priv);
int wifi67_phy_start(struct wifi67_priv *priv);
void wifi67_phy_stop(struct wifi67_priv *priv);
int wifi67_phy_config(struct wifi67_priv *priv, u32 channel, u32 band);
int wifi67_phy_set_power(struct wifi67_priv *priv, u32 power_level);
int wifi67_phy_get_rssi(struct wifi67_priv *priv);
int wifi67_phy_set_bandwidth(struct wifi67_priv *priv, u32 width);
int wifi67_phy_set_qam_mode(struct wifi67_priv *priv, u32 mode);
int wifi67_phy_enable_mlo(struct wifi67_priv *priv, bool enable);

#endif /* _WIFI67_PHY_CORE_H_ */ 