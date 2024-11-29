#ifndef _WIFI67_TYPES_H_
#define _WIFI67_TYPES_H_

#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <net/mac80211.h>
#include "wifi67_forward.h"

#include "../mac/mac_core.h"
#include "../phy/phy_core.h"
#include "../dma/dma_core.h"
#include "../hal/hardware.h"
#include "../regulatory/reg_types.h"
#include "../crypto/crypto_core.h"
#include "../firmware/fw_types.h"
#include "../debug/debug_types.h"
#include "../perf/perf_types.h"
#include "../diag/diag_types.h"

/* Main driver private structure */
struct wifi67_priv {
    struct ieee80211_hw *hw;
    struct pci_dev *pdev;
    
    /* Memory-mapped I/O */
    void __iomem *mmio;
    
    /* Component structures */
    struct wifi67_mac mac;
    struct wifi67_phy phy;
    struct wifi67_dma dma;
    struct wifi67_hw hal;
    struct wifi67_regulatory reg;
    struct wifi67_crypto crypto;
    struct wifi67_firmware fw;
    struct wifi67_debugfs debugfs;
    struct wifi67_perf_monitor perf;
    struct wifi67_hw_diag hw_diag;
    
    /* Locks */
    spinlock_t lock;
    
    /* Work items */
    struct work_struct tx_work;
} __packed __aligned(8);

#endif /* _WIFI67_TYPES_H_ */ 