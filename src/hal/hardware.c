#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/delay.h>
#include "../../include/hal/hardware.h"
#include "../../include/core/wifi67.h"
#include "../../include/dma/dma_core.h"
#include "../../include/firmware/emlfm.h"

/* Hardware-specific constants */
#define WIFI67_HW_RESET_DELAY_US    100
#define WIFI67_HW_BOOT_DELAY_US     500
#define WIFI67_HW_QUALITY_THRESHOLD  30
#define WIFI67_HW_MAX_RETRIES       3

/* Hardware register access helpers */
static inline void hw_write32(struct wifi67_priv *priv, u32 reg, u32 val)
{
    writel(val, priv->mmio + reg);
}

static inline u32 hw_read32(struct wifi67_priv *priv, u32 reg)
{
    return readl(priv->mmio + reg);
}

/* Hardware initialization */
int wifi67_hw_init(struct wifi67_priv *priv)
{
    int i;

    /* Map PCI BAR0 for register access */
    priv->mmio = pci_iomap(priv->pdev, 0, 0);
    if (!priv->mmio)
        return -ENOMEM;

    /* Initialize hardware state */
    for (i = 0; i < WIFI67_MAX_RADIOS; i++) {
        hw_write32(priv, WIFI67_REG_FW_CONTROL + i * 0x100, 0);
        hw_write32(priv, WIFI67_REG_FW_STATUS + i * 0x100, 0);
    }

    return 0;
}

void wifi67_hw_deinit(struct wifi67_priv *priv)
{
    if (priv->mmio) {
        pci_iounmap(priv->pdev, priv->mmio);
        priv->mmio = NULL;
    }
}

/* Firmware loading support */
int wifi67_hw_load_fw(struct wifi67_priv *priv, u8 radio_id,
                     const void *data, size_t size,
                     dma_addr_t iram_addr, dma_addr_t dram_addr,
                     dma_addr_t sram_addr)
{
    const struct wifi67_fw_header *hdr = data;
    const u8 *ptr = data;
    int ret = 0;

    /* Validate firmware header */
    if (size < sizeof(*hdr) || hdr->magic != WIFI67_FW_MAGIC)
        return -EINVAL;

    /* Configure memory regions */
    hw_write32(priv, WIFI67_REG_FW_IRAM_ADDR + radio_id * 0x100,
               lower_32_bits(iram_addr));
    hw_write32(priv, WIFI67_REG_FW_DRAM_ADDR + radio_id * 0x100,
               lower_32_bits(dram_addr));
    hw_write32(priv, WIFI67_REG_FW_SRAM_ADDR + radio_id * 0x100,
               lower_32_bits(sram_addr));

    /* Load IRAM section */
    if (hdr->iram_size > 0) {
        ptr += sizeof(*hdr);
        memcpy_toio(priv->mmio + radio_id * WIFI67_FW_IRAM_SIZE,
                    ptr, hdr->iram_size);
        ptr += hdr->iram_size;
    }

    /* Load DRAM section */
    if (hdr->dram_size > 0) {
        memcpy_toio(priv->mmio + radio_id * WIFI67_FW_DRAM_SIZE + 
                    WIFI67_FW_IRAM_SIZE,
                    ptr, hdr->dram_size);
        ptr += hdr->dram_size;
    }

    /* Load SRAM section */
    if (hdr->sram_size > 0) {
        memcpy_toio(priv->mmio + radio_id * WIFI67_FW_SRAM_SIZE +
                    WIFI67_FW_IRAM_SIZE + WIFI67_FW_DRAM_SIZE,
                    ptr, hdr->sram_size);
    }

    return ret;
}

int wifi67_hw_start_fw(struct wifi67_priv *priv, u8 radio_id,
                      dma_addr_t ipc_addr, size_t ipc_size)
{
    u32 val;
    int retries = WIFI67_HW_MAX_RETRIES;

    /* Configure IPC ring buffer */
    hw_write32(priv, WIFI67_REG_IPC_ADDR + radio_id * 0x100,
               lower_32_bits(ipc_addr));
    hw_write32(priv, WIFI67_REG_IPC_SIZE + radio_id * 0x100,
               ipc_size);

    /* Start firmware execution */
    val = hw_read32(priv, WIFI67_REG_FW_CONTROL + radio_id * 0x100);
    val |= WIFI67_FW_CTRL_START;
    hw_write32(priv, WIFI67_REG_FW_CONTROL + radio_id * 0x100, val);

    /* Wait for firmware to initialize */
    udelay(WIFI67_HW_BOOT_DELAY_US);

    /* Check firmware status */
    while (retries--) {
        val = hw_read32(priv, WIFI67_REG_FW_STATUS + radio_id * 0x100);
        if (val & WIFI67_FW_IRQ_READY)
            return 0;
        udelay(WIFI67_HW_BOOT_DELAY_US);
    }

    return -ETIMEDOUT;
}

void wifi67_hw_stop_fw(struct wifi67_priv *priv, u8 radio_id)
{
    u32 val;

    /* Stop firmware execution */
    val = hw_read32(priv, WIFI67_REG_FW_CONTROL + radio_id * 0x100);
    val &= ~WIFI67_FW_CTRL_START;
    hw_write32(priv, WIFI67_REG_FW_CONTROL + radio_id * 0x100, val);

    /* Clear status */
    hw_write32(priv, WIFI67_REG_FW_STATUS + radio_id * 0x100, 0);
}

void wifi67_hw_reset_radio(struct wifi67_priv *priv, u8 radio_id)
{
    u32 val;

    /* Assert reset */
    val = hw_read32(priv, WIFI67_REG_FW_CONTROL + radio_id * 0x100);
    val |= WIFI67_FW_CTRL_RESET;
    hw_write32(priv, WIFI67_REG_FW_CONTROL + radio_id * 0x100, val);

    udelay(WIFI67_HW_RESET_DELAY_US);

    /* Deassert reset */
    val &= ~WIFI67_FW_CTRL_RESET;
    hw_write32(priv, WIFI67_REG_FW_CONTROL + radio_id * 0x100, val);

    udelay(WIFI67_HW_RESET_DELAY_US);
}

u32 wifi67_hw_read32(struct wifi67_priv *priv, u32 reg)
{
    return hw_read32(priv, reg);
}

void wifi67_hw_write32(struct wifi67_priv *priv, u32 reg, u32 val)
{
    hw_write32(priv, reg, val);
}

int wifi67_hw_check_link_quality(struct wifi67_priv *priv, u8 link_id)
{
    u32 val;
    int quality = 0;

    /* Read link quality metrics */
    val = hw_read32(priv, WIFI67_REG_LINK_QUALITY + link_id * 0x4);
    
    /* Calculate quality score (0-100) */
    quality = FIELD_GET(WIFI67_LINK_QUALITY_MASK, val);
    
    return quality;
}

u8 wifi67_hw_find_best_radio(struct wifi67_priv *priv, u8 link_id)
{
    u32 val;
    u8 best_radio = WIFI67_INVALID_RADIO;
    int best_quality = -1;
    int i, quality;

    for (i = 0; i < WIFI67_MAX_RADIOS; i++) {
        /* Skip if radio is not available */
        val = hw_read32(priv, WIFI67_REG_RADIO_STATUS + i * 0x4);
        if (!(val & WIFI67_RADIO_AVAILABLE))
            continue;

        /* Check radio quality for this link */
        quality = wifi67_hw_check_radio_quality(priv, i, link_id);
        if (quality > best_quality) {
            best_quality = quality;
            best_radio = i;
        }
    }

    return best_radio;
}

int wifi67_hw_switch_link_radio(struct wifi67_priv *priv, u8 link_id,
                              u8 radio_id)
{
    u32 val;
    int retries = WIFI67_HW_MAX_RETRIES;

    /* Validate parameters */
    if (radio_id >= WIFI67_MAX_RADIOS)
        return -EINVAL;

    /* Check if radio is available */
    val = hw_read32(priv, WIFI67_REG_RADIO_STATUS + radio_id * 0x4);
    if (!(val & WIFI67_RADIO_AVAILABLE))
        return -ENODEV;

    /* Configure link-radio mapping */
    val = hw_read32(priv, WIFI67_REG_LINK_MAP + link_id * 0x4);
    val &= ~WIFI67_LINK_RADIO_MASK;
    val |= FIELD_PREP(WIFI67_LINK_RADIO_MASK, radio_id);
    hw_write32(priv, WIFI67_REG_LINK_MAP + link_id * 0x4, val);

    /* Wait for switch to complete */
    while (retries--) {
        val = hw_read32(priv, WIFI67_REG_LINK_STATUS + link_id * 0x4);
        if (!(val & WIFI67_LINK_SWITCHING))
            return 0;
        udelay(WIFI67_HW_RESET_DELAY_US);
    }

    return -ETIMEDOUT;
}

EXPORT_SYMBOL(wifi67_hw_init);
EXPORT_SYMBOL(wifi67_hw_deinit);
EXPORT_SYMBOL(wifi67_hw_load_fw);
EXPORT_SYMBOL(wifi67_hw_start_fw);
EXPORT_SYMBOL(wifi67_hw_stop_fw);
EXPORT_SYMBOL(wifi67_hw_reset_radio);
EXPORT_SYMBOL(wifi67_hw_read32);
EXPORT_SYMBOL(wifi67_hw_write32);
EXPORT_SYMBOL(wifi67_hw_check_link_quality);
EXPORT_SYMBOL(wifi67_hw_find_best_radio);
EXPORT_SYMBOL(wifi67_hw_switch_link_radio); 