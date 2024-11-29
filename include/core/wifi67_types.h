#ifndef _WIFI67_TYPES_H_
#define _WIFI67_TYPES_H_

#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <net/mac80211.h>

/* Include component headers */
#include "../phy/phy_core.h"
#include "../mac/mac_core.h"
#include "../dma/dma_core.h"
#include "../regulatory/reg_core.h"
#include "../crypto/crypto_core.h"
#include "../firmware/fw_core.h"

struct wifi67_priv {
    struct pci_dev *pdev;
    struct ieee80211_hw *hw;
    struct net_device *netdev;
    void __iomem *mmio;
    
    /* Components */
    struct wifi67_phy phy;
    struct wifi67_mac mac;
    struct wifi67_dma dma;
    struct wifi67_regulatory reg;
    struct wifi67_crypto crypto;
    struct wifi67_firmware fw;
    
    /* Locks */
    spinlock_t lock;
    
    /* Work items */
    struct work_struct tx_work;
} __packed __aligned(8);

#endif /* _WIFI67_TYPES_H_ */ 