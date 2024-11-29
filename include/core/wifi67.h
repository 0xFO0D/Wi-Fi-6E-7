#ifndef _WIFI67_H_
#define _WIFI67_H_

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/ieee80211.h>
#include <linux/spinlock.h>
#include "../debug/debug.h"
#include "../diag/hw_diag.h"
#include "../power/power.h"
#include "../firmware/firmware.h"
#include "../debug/debugfs.h"
#include "../perf/perf.h"
#include "features.h"

/* Main driver private structure */
struct wifi67_priv {
    struct ieee80211_hw *hw;
    struct pci_dev *pdev;
    
    struct ieee80211_supported_band bands[NUM_NL80211_BANDS];
    struct wifi67_features features;
    struct wifi67_mac *mac_dev;
    struct wifi67_phy *phy_dev;
    struct wifi67_hw_info *hw_info;
    struct wifi67_crypto_ctx *crypto_ctx;
    struct wifi67_firmware fw;
    struct wifi67_debugfs debugfs;
    struct wifi67_perf_monitor perf;
    struct wifi67_hw_diag hw_diag;
    struct wifi67_power_mgmt power;
    
    spinlock_t lock;
    
    bool initialized;
    bool suspended;
};

/* Function declarations */
int wifi67_core_init(struct pci_dev *pdev, struct ieee80211_hw *hw);
void wifi67_core_deinit(struct pci_dev *pdev, struct ieee80211_hw *hw);
int wifi67_core_start(struct wifi67_priv *priv);
void wifi67_core_stop(struct wifi67_priv *priv);
int wifi67_core_suspend(struct wifi67_priv *priv);
int wifi67_core_resume(struct wifi67_priv *priv);
int wifi67_setup_pci(struct wifi67_priv *priv);
void wifi67_cleanup_pci(struct wifi67_priv *priv);

#endif /* _WIFI67_H_ */ 