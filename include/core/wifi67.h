#ifndef _WIFI67_H_
#define _WIFI67_H_

#include <linux/pci.h>
#include <linux/ieee80211.h>

/* PCI Device IDs */
#define PCI_VENDOR_ID_WIFI67    0x0666  /* Example vendor ID */
#define PCI_DEVICE_ID_WIFI67    0x0667  /* Example device ID */

/* Driver private data structure */
struct wifi67_priv {
    struct ieee80211_hw *hw;
    struct pci_dev *pdev;
    struct ieee80211_supported_band bands[NUM_NL80211_BANDS];
    void __iomem *mmio;
    spinlock_t lock;
    
    /* DMA rings */
    struct wifi67_dma_ring tx_ring;
    struct wifi67_dma_ring rx_ring;
    
    /* Power management */
    bool ps_enabled;
    u32 ps_state;
    
    /* Hardware state */
    bool hw_initialized;
    u32 hw_revision;
    
    /* Firmware */
    struct wifi67_fw_info fw;
    
    /* Statistics */
    struct wifi67_stats stats;
};

/* Hardware operations */
extern const struct ieee80211_ops wifi67_ops;

/* Function declarations */
int wifi67_setup_pci(struct wifi67_priv *priv);
void wifi67_cleanup_pci(struct wifi67_priv *priv);
void wifi67_setup_hw_caps(struct ieee80211_hw *hw);

#endif /* _WIFI67_H_ */ 