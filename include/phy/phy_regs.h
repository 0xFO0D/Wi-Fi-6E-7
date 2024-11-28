#ifndef _WIFI67_PHY_REGS_H_
#define _WIFI67_PHY_REGS_H_

/* PHY Control and Status Registers */
#define PHY_CTRL_BASE          0x6000
#define PHY_CTRL_CONFIG        (PHY_CTRL_BASE + 0x00)
#define PHY_CTRL_STATUS        (PHY_CTRL_BASE + 0x04)
#define PHY_CALIBRATION        (PHY_CTRL_BASE + 0x08)
#define PHY_AGC_CTRL          (PHY_CTRL_BASE + 0x0C)
#define PHY_TX_GAIN_CTRL      (PHY_CTRL_BASE + 0x10)
#define PHY_RX_GAIN_CTRL      (PHY_CTRL_BASE + 0x14)
#define PHY_PLL_CTRL          (PHY_CTRL_BASE + 0x18)
#define PHY_CHANNEL_EST       (PHY_CTRL_BASE + 0x1C)
#define PHY_FFT_CTRL          (PHY_CTRL_BASE + 0x20)

/* PHY Configuration Bits */
#define PHY_CFG_ENABLE        BIT(0)
#define PHY_CFG_RESET         BIT(1)
#define PHY_CFG_LOOPBACK      BIT(2)
#define PHY_CFG_320MHZ        BIT(3)
#define PHY_CFG_4K_QAM        BIT(4)
#define PHY_CFG_MLO           BIT(5)

/* AGC Configuration */
#define AGC_GAIN_MASK         0xFF
#define AGC_GAIN_SHIFT        0
#define AGC_THRESHOLD_MASK    0xFF00
#define AGC_THRESHOLD_SHIFT   8

/* PLL Configuration */
#define PLL_FREQ_MASK         0xFFFFF
#define PLL_FREQ_SHIFT        0
#define PLL_LOCK_BIT          BIT(20)

#endif /* _WIFI67_PHY_REGS_H_ */ 