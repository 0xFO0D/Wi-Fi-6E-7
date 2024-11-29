#ifndef _WIFI67_MAC_DEFS_H_
#define _WIFI67_MAC_DEFS_H_

/* Queue Definitions */
#define WIFI67_NUM_TX_QUEUES     4
#define WIFI67_NUM_RX_QUEUES     4

#define WIFI67_MAC_QUEUE_VO     0
#define WIFI67_MAC_QUEUE_VI     1
#define WIFI67_MAC_QUEUE_BE     2
#define WIFI67_MAC_QUEUE_BK     3

/* MAC Register Offsets */
#define WIFI67_MAC_CTRL          0x0000
#define WIFI67_MAC_CONFIG        0x0004
#define WIFI67_MAC_STATUS        0x0008
#define WIFI67_MAC_INT_STATUS    0x000C
#define WIFI67_MAC_INT_MASK      0x0010
#define WIFI67_MAC_ADDR_L        0x0014
#define WIFI67_MAC_ADDR_H        0x0018
#define WIFI67_MAC_BSSID_L       0x001C
#define WIFI67_MAC_BSSID_H       0x0020
#define WIFI67_MAC_BEACON_TIME   0x0024
#define WIFI67_MAC_DTIM_PERIOD   0x0028
#define WIFI67_MAC_TX_CONFIG     0x002C
#define WIFI67_MAC_RX_CONFIG     0x0030
#define WIFI67_MAC_TX_STATUS     0x0034
#define WIFI67_MAC_RX_STATUS     0x0038

/* MAC Control Register Bits */
#define MAC_CTRL_RESET          BIT(0)
#define MAC_CTRL_TX_EN          BIT(1)
#define MAC_CTRL_RX_EN          BIT(2)
#define MAC_CTRL_PROMISC        BIT(3)
#define MAC_CTRL_HW_RETRY       BIT(4)
#define MAC_CTRL_BSSID_FILTER   BIT(5)
#define MAC_CTRL_BEACON_EN      BIT(6)
#define MAC_CTRL_AUTO_RESP      BIT(7)

/* MAC Capability Flags */
#define WIFI67_MAC_CAP_MLO      BIT(0)
#define WIFI67_MAC_CAP_MU_MIMO  BIT(1)
#define WIFI67_MAC_CAP_OFDMA    BIT(2)
#define WIFI67_MAC_CAP_TWT      BIT(3)
#define WIFI67_MAC_CAP_BSR      BIT(4)
#define WIFI67_MAC_CAP_UL_MU    BIT(5)
#define WIFI67_MAC_CAP_EHT      BIT(6)

/* MAC States */
#define WIFI67_MAC_OFF          0
#define WIFI67_MAC_IDLE         1
#define WIFI67_MAC_TX           2
#define WIFI67_MAC_RX           3
#define WIFI67_MAC_SCAN         4
#define WIFI67_MAC_PS           5

/* MAC Configuration Flags */
#define WIFI67_MAC_CFG_SHORT_PREAMBLE  BIT(0)
#define WIFI67_MAC_CFG_SHORT_SLOT      BIT(1)
#define WIFI67_MAC_CFG_RTS_CTS         BIT(2)
#define WIFI67_MAC_CFG_PROTECTION      BIT(3)
#define WIFI67_MAC_CFG_QOS             BIT(4)
#define WIFI67_MAC_CFG_AMPDU           BIT(5)
#define WIFI67_MAC_CFG_AMSDU           BIT(6)

#endif /* _WIFI67_MAC_DEFS_H_ */ 