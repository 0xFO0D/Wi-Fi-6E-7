#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/delay.h>

#include "../../include/firmware/fw_core.h"
#include "../../include/core/wifi67.h"

int wifi67_fw_load(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = &priv->fw;
    resource_size_t fw_addr;
    int ret;

    ret = request_firmware(&fw->fw_data, WIFI67_FW_NAME, &priv->pdev->dev);
    if (ret) {
        dev_err(&priv->pdev->dev, "Failed to load firmware\n");
        fw->fw_data = NULL;
        return ret;
    }

    /* Calculate firmware physical address */
    fw_addr = pci_resource_start(priv->pdev, 0) + 0x1000;

    /* Map firmware to device memory */
    fw->fw_mem = ioremap(fw_addr, fw->fw_data->size);
    if (!fw->fw_mem) {
        dev_err(&priv->pdev->dev, "Failed to map firmware memory\n");
        release_firmware(fw->fw_data);
        fw->fw_data = NULL;
        return -ENOMEM;
    }

    /* Copy firmware to device memory */
    memcpy_toio(fw->fw_mem, fw->fw_data->data, fw->fw_data->size);
    fw->loaded = true;

    return 0;
}

void wifi67_fw_unload(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = &priv->fw;

    if (fw->running)
        wifi67_fw_stop(priv);

    if (fw->fw_mem) {
        iounmap(fw->fw_mem);
        fw->fw_mem = NULL;
    }

    if (fw->fw_data) {
        release_firmware(fw->fw_data);
        fw->fw_data = NULL;
    }

    fw->loaded = false;
}

int wifi67_fw_verify(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = &priv->fw;
    u32 magic;

    if (!fw->loaded)
        return -EINVAL;

    /* Read and verify firmware magic */
    magic = ioread32(fw->fw_mem);
    if (magic != WIFI67_FW_MAGIC) {
        dev_err(&priv->pdev->dev, "Invalid firmware magic: %08x\n", magic);
        return -EINVAL;
    }

    return 0;
}

int wifi67_fw_start(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = &priv->fw;
    int ret;

    if (!fw->loaded)
        return -EINVAL;

    ret = wifi67_fw_verify(priv);
    if (ret)
        return ret;

    /* Start firmware execution */
    iowrite32(1, priv->mmio + 0x100);
    fw->running = true;

    return 0;
}

int wifi67_fw_stop(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = &priv->fw;

    if (!fw->running)
        return 0;

    /* Stop firmware execution */
    iowrite32(0, priv->mmio + 0x100);
    fw->running = false;

    return 0;
} 