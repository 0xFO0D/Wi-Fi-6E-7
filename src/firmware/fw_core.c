#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/delay.h>

#include "../../include/firmware/fw_core.h"
#include "../../include/firmware/fw_regs.h"
#include "../../include/core/wifi67.h"
#include "../../include/core/wifi67_debug.h"

int wifi67_fw_load(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = &priv->fw;
    struct wifi67_fw_header *header;
    phys_addr_t fw_addr;
    int ret;

    /* Request firmware from the kernel */
    ret = request_firmware(&fw->fw, WIFI67_FW_NAME, &priv->pdev->dev);
    if (ret) {
        wifi67_err(priv, "Failed to load firmware: %d\n", ret);
        return ret;
    }

    /* Allocate DMA memory for firmware */
    fw->fw_dma_size = fw->fw->size;
    fw->fw_mem = dma_alloc_coherent(&priv->pdev->dev, fw->fw_dma_size,
                                   &fw->fw_dma_addr, GFP_KERNEL);
    if (!fw->fw_mem) {
        wifi67_err(priv, "Failed to allocate firmware DMA memory\n");
        release_firmware(fw->fw);
        return -ENOMEM;
    }

    /* Copy firmware to DMA memory */
    memcpy(fw->fw_mem, fw->fw->data, fw->fw->size);

    /* Verify firmware header */
    header = (struct wifi67_fw_header *)fw->fw_mem;
    if (header->magic != WIFI67_FW_MAGIC) {
        wifi67_err(priv, "Invalid firmware magic: 0x%08x\n", header->magic);
        goto err_free_dma;
    }

    fw->version = header->version;
    fw->api_version = header->api_version;
    fw->loaded = true;

    wifi67_info(priv, "Firmware loaded: v%u.%u (API v%u)\n",
                fw->version >> 16, fw->version & 0xFFFF,
                fw->api_version);

    return 0;

err_free_dma:
    dma_free_coherent(&priv->pdev->dev, fw->fw_dma_size,
                      fw->fw_mem, fw->fw_dma_addr);
    release_firmware(fw->fw);
    return -EINVAL;
}

void wifi67_fw_unload(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = &priv->fw;

    if (fw->running)
        wifi67_fw_stop(priv);

    if (fw->fw_mem) {
        dma_free_coherent(&priv->pdev->dev, fw->fw_dma_size,
                         fw->fw_mem, fw->fw_dma_addr);
        fw->fw_mem = NULL;
    }

    if (fw->fw) {
        release_firmware(fw->fw);
        fw->fw = NULL;
    }

    fw->loaded = false;
    fw->verified = false;
}

int wifi67_fw_verify(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = &priv->fw;
    struct wifi67_fw_header *header;
    u32 checksum = 0;
    u8 *data;
    int i;

    if (!fw->loaded)
        return -EINVAL;

    header = (struct wifi67_fw_header *)fw->fw_mem;
    data = (u8 *)fw->fw_mem + sizeof(*header);

    /* Calculate checksum */
    for (i = 0; i < header->size; i++)
        checksum += data[i];

    if (checksum != header->checksum) {
        wifi67_err(priv, "Firmware checksum mismatch\n");
        return -EINVAL;
    }

    fw->verified = true;
    return 0;
}

int wifi67_fw_start(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = &priv->fw;
    struct wifi67_fw_header *header;

    if (!fw->loaded || !fw->verified)
        return -EINVAL;

    header = (struct wifi67_fw_header *)fw->fw_mem;

    /* Start firmware execution */
    iowrite32(header->entry_point, priv->mmio + WIFI67_REG_FW_ENTRY);
    iowrite32(1, priv->mmio + WIFI67_REG_FW_START);

    /* Wait for firmware to initialize */
    msleep(100);

    fw->running = true;
    return 0;
}

void wifi67_fw_stop(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = &priv->fw;

    if (!fw->running)
        return;

    /* Stop firmware execution */
    iowrite32(0, priv->mmio + WIFI67_REG_FW_START);
    msleep(50);

    fw->running = false;
} 