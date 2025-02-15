#ifndef __WIFI67_HARDWARE_H
#define __WIFI67_HARDWARE_H

#include <linux/types.h>
#include <linux/pci.h>
#include "../core/wifi67.h"

/* Hardware register definitions */
#define WIFI67_REG_CONTROL     0x0000
#define WIFI67_REG_STATUS      0x0004
#define WIFI67_REG_INT_STATUS  0x0008
#define WIFI67_REG_INT_MASK    0x000C

/* Control register bits */
#define WIFI67_CTRL_RESET      BIT(0)
#define WIFI67_CTRL_START      BIT(1)
#define WIFI67_CTRL_INT_ENABLE BIT(2)
#define WIFI67_CTRL_RX_ENABLE  BIT(3)
#define WIFI67_CTRL_TX_ENABLE  BIT(4)

/* Firmware control bits */
#define WIFI67_FW_CTRL_START   BIT(0)
#define WIFI67_FW_CTRL_RESET   BIT(1)
#define WIFI67_FW_CTRL_SLEEP   BIT(2)
#define WIFI67_FW_CTRL_WAKE    BIT(3)

/* Radio status bits */
#define WIFI67_RADIO_AVAILABLE BIT(0)
#define WIFI67_RADIO_ACTIVE    BIT(1)
#define WIFI67_RADIO_ERROR     BIT(2)
#define WIFI67_RADIO_SLEEP     BIT(3)

/* Link status bits */
#define WIFI67_LINK_ACTIVE     BIT(0)
#define WIFI67_LINK_ERROR      BIT(1)
#define WIFI67_LINK_SWITCHING  BIT(2)
#define WIFI67_LINK_QUALITY_MASK GENMASK(15, 8)
#define WIFI67_LINK_RADIO_MASK   GENMASK(23, 16)

/* Firmware magic number */
#define WIFI67_FW_MAGIC       0x57494637  /* "WIF7" */

/* Firmware header structure */
struct wifi67_fw_header {
    u32 magic;
    u32 version;
    u32 flags;
    u32 iram_size;
    u32 dram_size;
    u32 sram_size;
    u32 ipc_size;
    u8 reserved[32];
};

/* Function Declarations */
int wifi67_hw_init(struct wifi67_priv *priv);
void wifi67_hw_deinit(struct wifi67_priv *priv);
void wifi67_hw_irq_enable(struct wifi67_priv *priv);
void wifi67_hw_irq_disable(struct wifi67_priv *priv);
irqreturn_t wifi67_hw_interrupt(int irq, void *dev);

/* Firmware management */
int wifi67_hw_load_fw(struct wifi67_priv *priv, u8 radio_id,
                     const void *data, size_t size,
                     dma_addr_t iram_addr, dma_addr_t dram_addr,
                     dma_addr_t sram_addr);
int wifi67_hw_start_fw(struct wifi67_priv *priv, u8 radio_id,
                      dma_addr_t ipc_addr, size_t ipc_size);
void wifi67_hw_stop_fw(struct wifi67_priv *priv, u8 radio_id);
void wifi67_hw_reset_radio(struct wifi67_priv *priv, u8 radio_id);

/* Register access */
u32 wifi67_hw_read32(struct wifi67_priv *priv, u32 reg);
void wifi67_hw_write32(struct wifi67_priv *priv, u32 reg, u32 val);

/* Link management */
int wifi67_hw_check_link_quality(struct wifi67_priv *priv, u8 link_id);
u8 wifi67_hw_find_best_radio(struct wifi67_priv *priv, u8 link_id);
int wifi67_hw_switch_link_radio(struct wifi67_priv *priv, u8 link_id,
                              u8 radio_id);
int wifi67_hw_check_radio_quality(struct wifi67_priv *priv, u8 radio_id,
                                u8 link_id);

#endif /* __WIFI67_HARDWARE_H */ 