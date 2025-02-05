/*
 * WiFi 7 Firmware Loader
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/etherdevice.h>
#include "../supported_devices.h"
#include "mt7921_fw.h"
#include "rtl8852_fw.h"
#include "firmware_loader.h"

/* TODO: Add Intel firmware support */
/* TODO: Add Qualcomm firmware support */
/* TODO: Add Broadcom firmware support */

struct fw_desc {
    const char *name;
    u32 addr;
    u32 flags;
};

static int load_mt7921_firmware(struct managh_device_info *dev_info)
{
    const struct firmware *fw;
    int ret;

    /* Load ROM patch */
    ret = request_firmware(&fw, MT7921_ROM_PATCH, dev_info->dev);
    if (ret) {
        pr_err("Failed to request ROM patch %s\n", MT7921_ROM_PATCH);
        return ret;
    }

    /* TODO: Verify ROM patch checksum */
    /* TODO: Load ROM patch to device */

    release_firmware(fw);

    /* Load main firmware */
    ret = request_firmware(&fw, MT7921_FIRMWARE, dev_info->dev);
    if (ret) {
        pr_err("Failed to request firmware %s\n", MT7921_FIRMWARE);
        return ret;
    }

    /* TODO: Verify firmware checksum */
    /* TODO: Load firmware to device */

    release_firmware(fw);
    return 0;
}

static int load_rtl8852_firmware(struct managh_device_info *dev_info)
{
    const struct firmware *fw;
    int ret;

    /* Load ROM image */
    ret = request_firmware(&fw, RTL8852_ROM_IMG, dev_info->dev);
    if (ret) {
        pr_err("Failed to request ROM image %s\n", RTL8852_ROM_IMG);
        return ret;
    }

    /* TODO: Verify ROM checksum */
    /* TODO: Load ROM to device */

    release_firmware(fw);

    /* Load RAM code */
    ret = request_firmware(&fw, RTL8852_RAM_CODE, dev_info->dev);
    if (ret) {
        pr_err("Failed to request RAM code %s\n", RTL8852_RAM_CODE);
        return ret;
    }

    /* TODO: Verify RAM code checksum */
    /* TODO: Load RAM code to device */

    release_firmware(fw);

    /* Load main firmware */
    ret = request_firmware(&fw, RTL8852_FIRMWARE, dev_info->dev);
    if (ret) {
        pr_err("Failed to request firmware %s\n", RTL8852_FIRMWARE);
        return ret;
    }

    /* TODO: Verify firmware checksum */
    /* TODO: Load firmware to device */

    release_firmware(fw);
    return 0;
}

int managh_load_firmware(struct managh_device_info *dev_info)
{
    int ret = -ENODEV;

    switch (dev_info->vendor_id) {
    case MT_VENDOR_ID:
        switch (dev_info->device_id) {
        case MT7921_PCI_DEVICE_ID:
        case MT7922_PCI_DEVICE_ID:
        case MT7925_USB_PRODUCT_ID:
            ret = load_mt7921_firmware(dev_info);
            break;
        }
        break;

    case RTK_VENDOR_ID:
        switch (dev_info->device_id) {
        case RTL8852BE_DEVICE_ID:
        case RTL8852AE_DEVICE_ID:
        case RTL8852BU_PRODUCT_ID:
            ret = load_rtl8852_firmware(dev_info);
            break;
        }
        break;

    /* TODO: Add other vendors */
    }

    return ret;
}
EXPORT_SYMBOL_GPL(managh_load_firmware);

/* Firmware version query functions */
int managh_get_fw_version(struct managh_device_info *dev_info,
                         char *version, size_t len)
{
    /* TODO: Implement firmware version query */
    return -ENOSYS;
}
EXPORT_SYMBOL_GPL(managh_get_fw_version);

/* Firmware feature query functions */
int managh_get_fw_features(struct managh_device_info *dev_info,
                          u32 *features)
{
    /* TODO: Implement firmware features query */
    return -ENOSYS;
}
EXPORT_SYMBOL_GPL(managh_get_fw_features);

/* Firmware file paths */
#define FW_PATH_PREFIX      "wifi7/"
#define FW_PATH_SUFFIX      ".bin"
#define FW_CONFIG_SUFFIX    ".conf"

/* Firmware chunk size for USB transfers */
#define FW_CHUNK_SIZE       4096

/* Firmware load timeout (in milliseconds) */
#define FW_LOAD_TIMEOUT     5000

/* Firmware status codes */
#define FW_STATUS_SUCCESS   0x00
#define FW_STATUS_ERROR     0x01
#define FW_STATUS_RETRY     0x02
#define FW_STATUS_INVALID   0x03
#define FW_STATUS_TIMEOUT   0x04

/* Firmware header magic */
#define FW_MAGIC           0x57494637  /* "WIF7" */

/* Firmware header structure */
struct fw_header {
    u32 magic;              /* Magic number */
    u32 version;            /* Firmware version */
    u32 size;              /* Total size */
    u32 checksum;          /* CRC32 checksum */
    u32 entry_point;       /* Entry point */
    u32 flags;             /* Flags */
    u8 target_hw[16];      /* Target hardware */
    u8 build_date[16];     /* Build date */
    u8 reserved[16];       /* Reserved */
};

/* Internal firmware context */
struct fw_context {
    struct wifi7_dev *dev;
    const struct firmware *fw;
    struct fw_header header;
    u32 offset;
    u32 chunk_size;
    bool config_loaded;
    struct completion completion;
    int status;
};

/* Forward declarations */
static int fw_validate_header(struct fw_context *ctx);
static int fw_load_config(struct fw_context *ctx);
static int fw_transfer_chunk(struct fw_context *ctx);
static void fw_cleanup(struct fw_context *ctx);

/**
 * wifi7_load_firmware - Load firmware for WiFi 7 device
 * @dev: Device structure
 *
 * This function loads the firmware for the WiFi 7 device.
 * It first loads the firmware configuration file, then
 * transfers the firmware in chunks to the device.
 *
 * Return: 0 on success, negative error code on failure
 */
int wifi7_load_firmware(struct wifi7_dev *dev)
{
    struct fw_context *ctx;
    char fw_path[64];
    int ret;

    /* Allocate firmware context */
    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return -ENOMEM;

    /* Initialize context */
    ctx->dev = dev;
    ctx->chunk_size = FW_CHUNK_SIZE;
    init_completion(&ctx->completion);

    /* Build firmware path */
    snprintf(fw_path, sizeof(fw_path), "%s%s%s",
             FW_PATH_PREFIX, dev->hw_info.fw_name, FW_PATH_SUFFIX);

    /* Request firmware */
    ret = request_firmware(&ctx->fw, fw_path, dev->dev);
    if (ret) {
        dev_err(dev->dev, "Failed to request firmware: %d\n", ret);
        goto err_free;
    }

    /* Validate firmware header */
    ret = fw_validate_header(ctx);
    if (ret)
        goto err_release;

    /* Load firmware configuration */
    ret = fw_load_config(ctx);
    if (ret)
        goto err_release;

    /* Transfer firmware in chunks */
    while (ctx->offset < ctx->fw->size) {
        ret = fw_transfer_chunk(ctx);
        if (ret)
            goto err_release;

        /* Wait for chunk transfer completion */
        if (!wait_for_completion_timeout(&ctx->completion,
                                       msecs_to_jiffies(FW_LOAD_TIMEOUT))) {
            ret = -ETIMEDOUT;
            goto err_release;
        }

        /* Check transfer status */
        if (ctx->status != FW_STATUS_SUCCESS) {
            ret = -EIO;
            goto err_release;
        }

        /* Reset completion for next chunk */
        reinit_completion(&ctx->completion);
    }

    /* Cleanup and return */
    fw_cleanup(ctx);
    return 0;

err_release:
    release_firmware(ctx->fw);
err_free:
    kfree(ctx);
    return ret;
}

/**
 * fw_validate_header - Validate firmware header
 * @ctx: Firmware context
 *
 * This function validates the firmware header by checking
 * the magic number, version, size, and checksum.
 *
 * Return: 0 on success, negative error code on failure
 */
static int fw_validate_header(struct fw_context *ctx)
{
    struct fw_header *hdr = (struct fw_header *)ctx->fw->data;
    u32 checksum;

    /* Check firmware size */
    if (ctx->fw->size < sizeof(*hdr)) {
        dev_err(ctx->dev->dev, "Invalid firmware size\n");
        return -EINVAL;
    }

    /* Copy header */
    memcpy(&ctx->header, hdr, sizeof(*hdr));

    /* Check magic number */
    if (ctx->header.magic != FW_MAGIC) {
        dev_err(ctx->dev->dev, "Invalid firmware magic\n");
        return -EINVAL;
    }

    /* Check firmware size */
    if (ctx->header.size != ctx->fw->size) {
        dev_err(ctx->dev->dev, "Firmware size mismatch\n");
        return -EINVAL;
    }

    /* Verify checksum */
    checksum = crc32(0, ctx->fw->data + sizeof(*hdr),
                    ctx->fw->size - sizeof(*hdr));
    if (checksum != ctx->header.checksum) {
        dev_err(ctx->dev->dev, "Firmware checksum mismatch\n");
        return -EINVAL;
    }

    /* Set initial offset */
    ctx->offset = sizeof(*hdr);

    return 0;
}

/**
 * fw_load_config - Load firmware configuration
 * @ctx: Firmware context
 *
 * This function loads the firmware configuration file
 * and applies it to the device.
 *
 * Return: 0 on success, negative error code on failure
 */
static int fw_load_config(struct fw_context *ctx)
{
    char config_path[64];
    const struct firmware *config;
    int ret;

    /* Build config path */
    snprintf(config_path, sizeof(config_path), "%s%s%s",
             FW_PATH_PREFIX, ctx->dev->hw_info.fw_name, FW_CONFIG_SUFFIX);

    /* Request config */
    ret = request_firmware(&config, config_path, ctx->dev->dev);
    if (ret) {
        dev_warn(ctx->dev->dev, "No firmware config found\n");
        return 0;
    }

    /* Apply config */
    ret = wifi7_apply_config(ctx->dev, config->data, config->size);
    if (ret)
        dev_err(ctx->dev->dev, "Failed to apply config: %d\n", ret);

    /* Release config */
    release_firmware(config);

    return ret;
}

/**
 * fw_transfer_chunk - Transfer firmware chunk
 * @ctx: Firmware context
 *
 * This function transfers a chunk of firmware to the device.
 *
 * Return: 0 on success, negative error code on failure
 */
static int fw_transfer_chunk(struct fw_context *ctx)
{
    size_t remaining = ctx->fw->size - ctx->offset;
    size_t chunk_size = min(remaining, (size_t)ctx->chunk_size);
    int ret;

    /* Transfer chunk */
    ret = wifi7_write_firmware(ctx->dev,
                             ctx->fw->data + ctx->offset,
                             chunk_size);
    if (ret) {
        dev_err(ctx->dev->dev, "Failed to transfer chunk: %d\n", ret);
        return ret;
    }

    /* Update offset */
    ctx->offset += chunk_size;

    return 0;
}

/**
 * fw_cleanup - Clean up firmware context
 * @ctx: Firmware context
 *
 * This function cleans up the firmware context by
 * releasing the firmware and freeing memory.
 */
static void fw_cleanup(struct fw_context *ctx)
{
    release_firmware(ctx->fw);
    kfree(ctx);
}

/**
 * wifi7_firmware_complete - Complete firmware transfer
 * @dev: Device structure
 * @status: Transfer status
 *
 * This function is called by the device to complete
 * a firmware transfer operation.
 */
void wifi7_firmware_complete(struct wifi7_dev *dev, int status)
{
    struct fw_context *ctx = dev->fw_context;

    if (!ctx)
        return;

    /* Set status and complete */
    ctx->status = status;
    complete(&ctx->completion);
}

EXPORT_SYMBOL(wifi7_load_firmware);
EXPORT_SYMBOL(wifi7_firmware_complete); 