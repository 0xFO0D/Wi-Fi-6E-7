#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include "../../include/core/wifi67.h"

int wifi67_setup_pci(struct wifi67_priv *priv)
{
    struct pci_dev *pdev = priv->pdev;
    int ret;

    /* Enable PCI device */
    ret = pci_enable_device(pdev);
    if (ret)
        return ret;

    /* Request PCI regions */
    ret = pci_request_regions(pdev, "wifi67");
    if (ret)
        goto err_disable_device;

    /* Set DMA mask */
    ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
    if (ret) {
        ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
        if (ret)
            goto err_release_regions;
    }

    /* Map BAR0 */
    priv->membase = pci_iomap(pdev, 0, 0);
    if (!priv->membase) {
        ret = -EIO;
        goto err_release_regions;
    }

    /* Enable bus mastering */
    pci_set_master(pdev);

    /* Request IRQ */
    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
    if (ret < 0)
        goto err_iounmap;

    priv->irq = pci_irq_vector(pdev, 0);
    ret = request_irq(priv->irq, wifi67_hw_interrupt, 0, "wifi67", priv);
    if (ret)
        goto err_free_irq_vectors;

    return 0;

err_free_irq_vectors:
    pci_free_irq_vectors(pdev);
err_iounmap:
    pci_iounmap(pdev, priv->membase);
err_release_regions:
    pci_release_regions(pdev);
err_disable_device:
    pci_disable_device(pdev);
    return ret;
}

void wifi67_cleanup_pci(struct wifi67_priv *priv)
{
    struct pci_dev *pdev = priv->pdev;

    free_irq(priv->irq, priv);
    pci_free_irq_vectors(pdev);
    pci_iounmap(pdev, priv->membase);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
}

void wifi67_setup_hw_caps(struct ieee80211_hw *hw)
{
    hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) |
                                BIT(NL80211_IFTYPE_AP);
    
    hw->queues = 4;
    hw->max_rates = 4;
    hw->max_rate_tries = 11;
    hw->extra_tx_headroom = 32;
    
    hw->wiphy->flags |= WIPHY_FLAG_SUPPORTS_TDLS |
                       WIPHY_FLAG_HAS_CHANNEL_SWITCH |
                       WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL;
                       
    hw->wiphy->features |= NL80211_FEATURE_SCAN_RANDOM_MAC_ADDR |
                          NL80211_FEATURE_LOW_PRIORITY_SCAN;
                          
    ieee80211_hw_set(hw, SIGNAL_DBM);
    ieee80211_hw_set(hw, RX_INCLUDES_FCS);
    ieee80211_hw_set(hw, SUPPORT_FAST_XMIT);
    ieee80211_hw_set(hw, SUPPORTS_PS);
    ieee80211_hw_set(hw, SUPPORTS_DYNAMIC_PS);
    ieee80211_hw_set(hw, MFP_CAPABLE);
    ieee80211_hw_set(hw, REPORTS_TX_ACK_STATUS);
    ieee80211_hw_set(hw, AMPDU_AGGREGATION);
    ieee80211_hw_set(hw, TX_AMPDU_SETUP_IN_HW);
    ieee80211_hw_set(hw, SUPPORTS_TX_FRAG);
    ieee80211_hw_set(hw, CONNECTION_MONITOR);
    ieee80211_hw_set(hw, CHANCTX_STA_CSA);
    ieee80211_hw_set(hw, SUPPORTS_MULTI_BSSID);
    ieee80211_hw_set(hw, SUPPORTS_ONLY_HE_MULTI_BSSID);
} 