#include <linux/pci.h>
#include <linux/interrupt.h>
#include "../../include/core/wifi67.h"

int wifi67_setup_pci(struct wifi67_priv *priv)
{
    struct pci_dev *pdev = priv->pdev;
    int ret;

    ret = pci_enable_device(pdev);
    if (ret)
        return ret;

    ret = pci_request_regions(pdev, "wifi67");
    if (ret)
        goto err_disable_device;

    pci_set_master(pdev);

    ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
    if (ret)
        goto err_release_regions;

    ret = pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32));
    if (ret)
        goto err_release_regions;

    priv->mmio = pci_iomap(pdev, 0, 0);
    if (!priv->mmio) {
        ret = -ENOMEM;
        goto err_release_regions;
    }

    ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
    if (ret < 0)
        goto err_iounmap;

    ret = request_irq(pci_irq_vector(pdev, 0), wifi67_hw_interrupt,
                     IRQF_SHARED, "wifi67", priv);
    if (ret)
        goto err_free_irq_vectors;

    return 0;

err_free_irq_vectors:
    pci_free_irq_vectors(pdev);
err_iounmap:
    pci_iounmap(pdev, priv->mmio);
err_release_regions:
    pci_release_regions(pdev);
err_disable_device:
    pci_disable_device(pdev);
    return ret;
}

void wifi67_cleanup_pci(struct wifi67_priv *priv)
{
    struct pci_dev *pdev = priv->pdev;

    free_irq(pci_irq_vector(pdev, 0), priv);
    pci_free_irq_vectors(pdev);
    pci_iounmap(pdev, priv->mmio);
    pci_release_regions(pdev);
    pci_disable_device(pdev);
} 