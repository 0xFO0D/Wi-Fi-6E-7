#ifndef _WIFI67_MAC_REGS_H_
#define _WIFI67_MAC_REGS_H_

/* MAC Register Map */
#define MAC_CTRL_BASE         0x2000
#define MAC_CTRL_CONFIG       (MAC_CTRL_BASE + 0x00)
#define MAC_FRAME_CTRL        (MAC_CTRL_BASE + 0x04)
#define MAC_QOS_CTRL         (MAC_CTRL_BASE + 0x08)
#define MAC_AMPDU_CTRL       (MAC_CTRL_BASE + 0x0C)
#define MAC_BA_CTRL          (MAC_CTRL_BASE + 0x10)
#define MAC_TSF_TIMER_LOW    (MAC_CTRL_BASE + 0x14)
#define MAC_TSF_TIMER_HIGH   (MAC_CTRL_BASE + 0x18)
#define MAC_BEACON_TIME      (MAC_CTRL_BASE + 0x1C)
#define MAC_DTIM_CTRL        (MAC_CTRL_BASE + 0x20)
#define MAC_MLO_CTRL         (MAC_CTRL_BASE + 0x24)

/* MAC Configuration Bits */
#define MAC_CFG_ENABLE       BIT(0)
#define MAC_CFG_RX_EN        BIT(1)
#define MAC_CFG_TX_EN        BIT(2)
#define MAC_CFG_AMPDU_EN     BIT(3)
#define MAC_CFG_AMSDU_EN     BIT(4)
#define MAC_CFG_MLO_EN       BIT(5)
#define MAC_CFG_PS_EN        BIT(6)
#define MAC_CFG_BCN_EN       BIT(7)

/* AMPDU Control Bits */
#define AMPDU_MAX_LEN_MASK   0xFF
#define AMPDU_MAX_LEN_SHIFT  0
#define AMPDU_MIN_SPACING    0x700
#define AMPDU_DENSITY_SHIFT  8

/* Block ACK Control */
#define BA_POLICY_IMMEDIATE  BIT(0)
#define BA_BUFFER_SIZE_MASK  0xFF00
#define BA_BUFFER_SIZE_SHIFT 8
#define BA_TID_MASK         0xF0000
#define BA_TID_SHIFT        16

#endif /* _WIFI67_MAC_REGS_H_ */ 