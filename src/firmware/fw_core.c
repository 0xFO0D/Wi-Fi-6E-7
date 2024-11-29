#include <linux/kernel.h>
#include <linux/firmware.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/crc32.h>
#include "../../include/firmware/fw_core.h"
#include "../../include/core/wifi67_debug.h"
#include "../../include/core/wifi67_utils.h"

#define WIFI67_FW_MAGIC         0x57494637  // "WIF7"
#define WIFI67_FW_REG_CTRL      0x4000
#define WIFI67_FW_REG_STATUS    0x4004
#define WIFI67_FW_REG_ADDR      0x4008

#define WIFI67_FW_CTRL_START    BIT(0)
#define WIFI67_FW_CTRL_STOP     BIT(1)
#define WIFI67_FW_CTRL_RESET    BIT(2)

#define WIFI67_FW_STATUS_READY  BIT(0)
#define WIFI67_FW_STATUS_ERROR  BIT(1)
#define WIFI67_FW_STATUS_BUSY   BIT(2)

int wifi67_fw_load(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = &priv->fw;
    struct wifi67_fw_header *header;
    int ret;

    if (fw->loaded)
        return 0;

    ret = request_firmware(&fw->fw, WIFI67_FW_NAME, &priv->pdev->dev);
    if (ret) {
        wifi67_err(priv, "Failed to load firmware: %d\n", ret);
        return ret;
    }

    fw->fw_dma_size = fw->fw->size;
    fw->fw_mem = dma_alloc_coherent(&priv->pdev->dev, fw->fw_dma_size,
                                   &fw->fw_dma_addr, GFP_KERNEL);
    if (!fw->fw_mem) {
        wifi67_err(priv, "Failed to allocate DMA memory for firmware\n");
        release_firmware(fw->fw);
        return -ENOMEM;
    }

    memcpy(fw->fw_mem, fw->fw->data, fw->fw->size);

    header = (struct wifi67_fw_header *)fw->fw_mem;
    if (header->magic != WIFI67_FW_MAGIC) {
        wifi67_err(priv, "Invalid firmware magic: 0x%08x\n", header->magic);
        ret = -EINVAL;
        goto err_free;
    }

    fw->version = header->version;
    fw->api_version = header->api_version;
    fw->loaded = true;

    wifi67_info(priv, "Firmware loaded: v%u.%u (API v%u)\n",
                fw->version >> 16, fw->version & 0xFFFF,
                fw->api_version);

    return 0;

err_free:
    dma_free_coherent(&priv->pdev->dev, fw->fw_dma_size,
                      fw->fw_mem, fw->fw_dma_addr);
    release_firmware(fw->fw);
    return ret;
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
    const u8 *data;
    u32 checksum;

    if (!fw->loaded)
        return -EINVAL;

    header = (struct wifi67_fw_header *)fw->fw_mem;
    data = (u8 *)fw->fw_mem + sizeof(*header);

    if (header->api_version < WIFI67_FW_API_VER_MIN ||
        header->api_version > WIFI67_FW_API_VER_MAX) {
        wifi67_err(priv, "Unsupported firmware API version: %u\n",
                  header->api_version);
        return -EINVAL;
    }

    checksum = crc32(0, data, header->data_size);
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
    u32 val;
    int ret;

    if (!fw->loaded || !fw->verified)
        return -EINVAL;

    header = (struct wifi67_fw_header *)fw->fw_mem;

    /* Write firmware DMA address */
    writel(fw->fw_dma_addr + header->entry_point,
           priv->mmio + WIFI67_FW_REG_ADDR);

    /* Start firmware execution */
    val = WIFI67_FW_CTRL_START;
    writel(val, priv->mmio + WIFI67_FW_REG_CTRL);

    /* Wait for firmware to become ready */
    ret = wifi67_wait_bit(priv->mmio + WIFI67_FW_REG_STATUS,
                         WIFI67_FW_STATUS_READY, 1000);
    if (ret) {
        wifi67_err(priv, "Firmware failed to start\n");
        return ret;
    }

    fw->running = true;
    return 0;
}

void wifi67_fw_stop(struct wifi67_priv *priv)
{
    struct wifi67_firmware *fw = &priv->fw;
    u32 val;

    if (!fw->running)
        return;

    /* Stop firmware execution */
    val = WIFI67_FW_CTRL_STOP;
    writel(val, priv->mmio + WIFI67_FW_REG_CTRL);

    /* Wait for firmware to stop */
    wifi67_wait_bit(priv->mmio + WIFI67_FW_REG_STATUS,
                    WIFI67_FW_STATUS_READY, 1000);

    fw->running = false;
}

EXPORT_SYMBOL_GPL(wifi67_fw_load);
EXPORT_SYMBOL_GPL(wifi67_fw_unload);
EXPORT_SYMBOL_GPL(wifi67_fw_verify);
EXPORT_SYMBOL_GPL(wifi67_fw_start);
EXPORT_SYMBOL_GPL(wifi67_fw_stop); 