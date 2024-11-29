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

/* PHY Control Register bits */
#define WIFI67_PHY_CTRL_ENABLE       BIT(0)
#define WIFI67_PHY_CTRL_RESET        BIT(1)
#define WIFI67_PHY_CTRL_AGC_EN       BIT(2)
#define WIFI67_PHY_CTRL_CALIB_EN     BIT(3)
#define WIFI67_PHY_CTRL_SLEEP        BIT(4)

/* PHY Status Register bits */
#define WIFI67_PHY_STATUS_READY      BIT(0)
#define WIFI67_PHY_STATUS_AGC_DONE   BIT(1)
#define WIFI67_PHY_STATUS_CALIB_DONE BIT(2)
#define WIFI67_PHY_STATUS_PLL_LOCK   BIT(3)

struct wifi67_phy {
    void __iomem *regs;
    u32 current_channel;
    u32 current_band;
    u32 current_power;
    u32 agc_gain;
    bool enabled;
    spinlock_t lock;
};

int wifi67_phy_init(struct wifi67_priv *priv);
void wifi67_phy_deinit(struct wifi67_priv *priv);
int wifi67_phy_start(struct wifi67_priv *priv);
void wifi67_phy_stop(struct wifi67_priv *priv);
int wifi67_phy_config(struct wifi67_priv *priv, u32 channel, u32 band);
int wifi67_phy_set_power(struct wifi67_priv *priv, u32 power_level);
int wifi67_phy_get_rssi(struct wifi67_priv *priv);

#endif /* _WIFI67_PHY_CORE_H_ */ 