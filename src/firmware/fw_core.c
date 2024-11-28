#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/crc32.h>
#include "../../include/firmware/fw_core.h"

int wifi67_fw_load(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw;
    int ret;

    fw = kzalloc(sizeof(*fw), GFP_KERNEL);
    if (!fw)
        return -ENOMEM;

    priv->fw = fw;
    init_completion(&fw->fw_load);

    ret = request_firmware(&fw->fw, FW_NAME, &priv->pdev->dev);
    if (ret) {
        kfree(fw);
        priv->fw = NULL;
        return ret;
    }

    fw->fw_data = kmemdup(fw->fw->data, fw->fw->size, GFP_KERNEL);
    if (!fw->fw_data) {
        release_firmware(fw->fw);
        kfree(fw);
        priv->fw = NULL;
        return -ENOMEM;
    }

    fw->fw_size = fw->fw->size;
    fw->fw_crc = crc32_le(0, fw->fw_data, fw->fw_size);

    complete(&fw->fw_load);
    return 0;
}

void wifi67_fw_unload(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = priv->fw;

    if (!fw)
        return;

    release_firmware(fw->fw);
    kfree(fw->fw_data);
    kfree(fw);
    priv->fw = NULL;
}

int wifi67_fw_verify(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = priv->fw;
    u32 crc;

    if (!fw)
        return -EINVAL;

    crc = crc32_le(0, fw->fw_data, fw->fw_size);
    if (crc != fw->fw_crc)
        return -EIO;

    return 0;
}

int wifi67_fw_start(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = priv->fw;
    u32 val;

    if (!fw)
        return -EINVAL;

    /* Load firmware into hardware */
    writel(fw->fw_size, priv->mmio + 0x5000);
    memcpy_toio(priv->mmio + 0x5004, fw->fw_data, fw->fw_size);

    /* Start firmware */
    val = readl(priv->mmio + 0x5000);
    val |= 0x1; /* Start bit */
    writel(val, priv->mmio + 0x5000);

    return 0;
}

void wifi67_fw_stop(struct wifi67_priv *priv)
{
    u32 val;

    /* Stop firmware */
    val = readl(priv->mmio + 0x5000);
    val &= ~0x1; /* Clear start bit */
    writel(val, priv->mmio + 0x5000);
} 