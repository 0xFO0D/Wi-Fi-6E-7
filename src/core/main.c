#include <linux/module.h>
#include <linux/pci.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>
#include "../../include/core/wifi67.h"
#include "../../include/core/bands.h"
#include "../../include/core/caps.h"
#include "../../include/core/ops.h"

static void wifi67_setup_bands(struct wifi67_priv *priv)
{
    struct ieee80211_supported_band *sband;
    
    /* Setup 5 GHz band */
    sband = &priv->bands[NL80211_BAND_5GHZ];
    sband->channels = wifi67_band_5ghz.channels;
    sband->n_channels = wifi67_band_5ghz.n_channels;
    sband->bitrates = wifi67_band_5ghz.bitrates;
    sband->n_bitrates = wifi67_band_5ghz.n_bitrates;
    
    /* HT/VHT/HE capabilities */
    if (wifi67_band_5ghz.ht_supported)
        wifi67_setup_ht_cap(sband);
    if (wifi67_band_5ghz.vht_supported)
        wifi67_setup_vht_cap(sband);
    if (wifi67_band_5ghz.he_supported)
        wifi67_setup_he_cap(sband);
        
    priv->hw->wiphy->bands[NL80211_BAND_5GHZ] = sband;
    
    /* Setup 6 GHz band */
    sband = &priv->bands[NL80211_BAND_6GHZ];
    sband->channels = wifi67_band_6ghz.channels;
    sband->n_channels = wifi67_band_6ghz.n_channels;
    sband->bitrates = wifi67_band_6ghz.bitrates;
    sband->n_bitrates = wifi67_band_6ghz.n_bitrates;
    
    /* HE/EHT capabilities */
    if (wifi67_band_6ghz.he_supported)
        wifi67_setup_he_cap(sband);
    if (wifi67_band_6ghz.eht_supported)
        wifi67_setup_eht_cap(sband);
        
    priv->hw->wiphy->bands[NL80211_BAND_6GHZ] = sband;
}

static int wifi67_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct ieee80211_hw *hw;
    struct wifi67_priv *priv;
    int ret;

    /* Allocate ieee80211_hw */
    hw = ieee80211_alloc_hw(sizeof(struct wifi67_priv), &wifi67_ops);
    if (!hw) {
        dev_err(&pdev->dev, "Failed to allocate hw\n");
        return -ENOMEM;
    }

    /* Initialize private data */
    priv = hw->priv;
    priv->hw = hw;
    priv->pdev = pdev;

    /* Setup PCI device */
    ret = wifi67_setup_pci(priv);
    if (ret)
        goto err_free_hw;

    /* Setup hardware capabilities */
    wifi67_setup_hw_caps(hw);
    
    /* Setup bands */
    wifi67_setup_bands(priv);

    /* Register with mac80211 */
    ret = ieee80211_register_hw(hw);
    if (ret)
        goto err_cleanup_pci;

    pci_set_drvdata(pdev, hw);
    return 0;

err_cleanup_pci:
    wifi67_cleanup_pci(priv);
err_free_hw:
    ieee80211_free_hw(hw);
    return ret;
}

static void wifi67_remove(struct pci_dev *pdev)
{
    struct ieee80211_hw *hw = pci_get_drvdata(pdev);
    struct wifi67_priv *priv = hw->priv;

    ieee80211_unregister_hw(hw);
    wifi67_cleanup_pci(priv);
    ieee80211_free_hw(hw);
}

static const struct pci_device_id wifi67_pci_ids[] = {
    { PCI_DEVICE(PCI_VENDOR_ID_WIFI67, PCI_DEVICE_ID_WIFI67) },
    { }
};
MODULE_DEVICE_TABLE(pci, wifi67_pci_ids);

static struct pci_driver wifi67_driver = {
    .name = "wifi67",
    .id_table = wifi67_pci_ids,
    .probe = wifi67_probe,
    .remove = wifi67_remove,
};

module_pci_driver(wifi67_driver);

MODULE_AUTHOR("OXFO0D");
MODULE_DESCRIPTION("WiFi 6E/7 Driver");
MODULE_LICENSE("GPL"); 