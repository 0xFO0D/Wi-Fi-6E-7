#ifndef _WIFI67_PHY_CORE_H_
#define _WIFI67_PHY_CORE_H_

#include <linux/types.h>
#include <linux/workqueue.h>
#include "../core/wifi67_forward.h"

/* PHY Register definitions */
#define PHY_REG_BASE         0x1000
#define PHY_CONTROL         (PHY_REG_BASE + 0x00)
#define PHY_STATUS          (PHY_REG_BASE + 0x04)
#define PHY_CALIBRATION     (PHY_REG_BASE + 0x08)
#define PHY_INT_STATUS      (PHY_REG_BASE + 0x0C)
#define PHY_INT_MASK        (PHY_REG_BASE + 0x10)
#define PHY_CHANNEL         (PHY_REG_BASE + 0x14)
#define PHY_TXPOWER         (PHY_REG_BASE + 0x18)
#define PHY_AGC             (PHY_REG_BASE + 0x1C)

/* PHY Control Register bits */
#define PHY_CTRL_ENABLE     BIT(0)
#define PHY_CTRL_RESET      BIT(1)
#define PHY_CTRL_TX         BIT(2)
#define PHY_CTRL_RX         BIT(3)
#define PHY_CTRL_CALIB      BIT(4)

/* PHY Status Register bits */
#define PHY_STATUS_READY    BIT(0)
#define PHY_STATUS_CAL      BIT(1)
#define PHY_STATUS_TX       BIT(2)
#define PHY_STATUS_RX       BIT(3)

/* PHY Calibration flags */
#define PHY_CAL_VALID       BIT(0)
#define PHY_CAL_TX_DONE     BIT(1)
#define PHY_CAL_RX_DONE     BIT(2)

/* Per-chain calibration data */
struct phy_chain_cal {
    u32 tx_iq_cal;
    u32 rx_iq_cal;
    u32 tx_dc_offset;
    u32 rx_dc_offset;
    u32 pll_cal;
    u32 temp_comp;
};

/* PHY Calibration data structure */
struct phy_calibration {
    struct phy_chain_cal per_chain[4];  // Support up to 4 chains
    unsigned long last_cal_time;
    u32 cal_flags;
};

/* Main PHY structure */
struct wifi67_phy {
    /* PHY Status */
    bool enabled;
    bool calibrated;
    u32 current_channel;
    u32 current_band;
    u32 current_bandwidth;
    int current_txpower;
    
    /* Calibration data */
    struct phy_calibration cal;
    
    /* Temperature compensation */
    s16 temp_slope;
    s16 temp_intercept;
    s16 last_temp;
    
    /* Work queue for periodic calibration */
    struct delayed_work cal_work;
    
    /* Lock for PHY access */
    spinlock_t lock;
} __packed __aligned(4);

/* Function declarations */
int wifi67_phy_init(struct wifi67_priv *priv);
void wifi67_phy_deinit(struct wifi67_priv *priv);
int wifi67_phy_start(struct wifi67_priv *priv);
void wifi67_phy_stop(struct wifi67_priv *priv);
int wifi67_phy_config_channel(struct wifi67_priv *priv, u32 freq, u32 bandwidth);
int wifi67_phy_set_txpower(struct wifi67_priv *priv, int dbm);
int wifi67_phy_get_rssi(struct wifi67_priv *priv);
int wifi67_phy_calibrate(struct wifi67_priv *priv);

#endif /* _WIFI67_PHY_CORE_H_ */ 