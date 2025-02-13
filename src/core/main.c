#include <linux/module.h>
#include <linux/pci.h>
#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include <net/mac80211.h>

#include "../../include/core/wifi67.h"
#include "../../include/core/caps.h"
#include "../../include/core/bands.h"
#include "../../include/debug/debug.h"
#include "../../include/core/mlo.h"

/* Function prototypes */
static int wifi67_probe(struct pci_dev *pdev, const struct pci_device_id *id);
static void wifi67_remove(struct pci_dev *pdev);
int wifi67_setup_bands(struct wifi67_priv *priv);
int wifi67_setup_pci(struct wifi67_priv *priv);
void wifi67_cleanup_pci(struct wifi67_priv *priv);

static int wifi67_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct ieee80211_hw *hw;
    struct wifi67_priv *priv;
    int ret;

    /* Allocate ieee80211_hw */
    hw = ieee80211_alloc_hw(sizeof(struct wifi67_priv), &wifi67_ops);
    if (!hw) {
        wifi67_debug(NULL, WIFI67_DEBUG_ERROR, "Failed to allocate hw\n");
        return -ENOMEM;
    }

    /* Get private data */
    priv = hw->priv;
    priv->hw = hw;
    priv->pdev = pdev;

    /* Initialize PCI device */
    ret = wifi67_setup_pci(priv);
    if (ret) {
        wifi67_debug(priv, WIFI67_DEBUG_ERROR, "Failed to setup PCI: %d\n", ret);
        goto err_free_hw;
    }

    /* Setup hardware capabilities */
    wifi67_setup_hw_caps(priv);  // Fixed: Pass priv instead of hw

    /* Setup frequency bands */
    ret = wifi67_setup_bands(priv);
    if (ret) {
        wifi67_debug(priv, WIFI67_DEBUG_ERROR, "Failed to setup bands: %d\n", ret);
        goto err_cleanup_pci;
    }

    /* Initialize subsystems */
    ret = wifi67_hw_diag_init(priv);
    if (ret) {
        wifi67_debug(priv, WIFI67_DEBUG_ERROR, "Failed to init diagnostics: %d\n", ret);
        goto err_cleanup_pci;
    }

    ret = wifi67_power_init(priv);
    if (ret) {
        wifi67_debug(priv, WIFI67_DEBUG_ERROR, "Failed to init power mgmt: %d\n", ret);
        goto err_deinit_diag;
    }

    /* Initialize MLO subsystem */
    ret = wifi67_mlo_init(priv);
    if (ret) {
        wifi67_debug(priv, WIFI67_DEBUG_ERROR, "Failed to init MLO: %d\n", ret);
        goto err_deinit_power;
    }

    /* Register with mac80211 */
    ret = ieee80211_register_hw(hw);
    if (ret) {
        wifi67_debug(priv, WIFI67_DEBUG_ERROR, "Failed to register hw: %d\n", ret);
        goto err_deinit_mlo;
    }

    pci_set_drvdata(pdev, hw);
    return 0;

err_deinit_mlo:
    wifi67_mlo_deinit(priv);
err_deinit_power:
    wifi67_power_deinit(priv);
err_deinit_diag:
    wifi67_hw_diag_deinit(priv);
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
    wifi67_mlo_deinit(priv);
    wifi67_power_deinit(priv);
    wifi67_hw_diag_deinit(priv);
    wifi67_cleanup_pci(priv);
    ieee80211_free_hw(hw);
}

/* PCI driver structure */
static const struct pci_device_id wifi67_pci_ids[] = {
    { PCI_DEVICE(PCI_VENDOR_ID_WIFI67, PCI_DEVICE_ID_WIFI67_DEV1) },
    { PCI_DEVICE(PCI_VENDOR_ID_WIFI67, PCI_DEVICE_ID_WIFI67_DEV2) },
    { }
};
MODULE_DEVICE_TABLE(pci, wifi67_pci_ids);

static struct pci_driver wifi67_pci_driver = {
    .name = KBUILD_MODNAME,
    .id_table = wifi67_pci_ids,
    .probe = wifi67_probe,
    .remove = wifi67_remove,
};

module_pci_driver(wifi67_pci_driver);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("WiFi 6E/7 Driver");
MODULE_LICENSE("GPL"); 