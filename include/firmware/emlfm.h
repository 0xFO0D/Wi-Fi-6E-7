#ifndef __WIFI67_EMLFM_H
#define __WIFI67_EMLFM_H

#define WIFI67_FW_IRAM_SIZE     (256 * 1024)  /* 256KB */
#define WIFI67_FW_DRAM_SIZE     (512 * 1024)  /* 512KB */
#define WIFI67_FW_SRAM_SIZE     (128 * 1024)  /* 128KB */
#define WIFI67_IPC_RING_SIZE    (64 * 1024)   /* 64KB */

#define WIFI67_FW_READY_TIMEOUT (HZ * 3)      /* 3 seconds */

/* Firmware memory region flags */
#define WIFI67_FW_REGION_EXEC    BIT(0)  /* Executable memory */
#define WIFI67_FW_REGION_RW      BIT(1)  /* Read-write memory */
#define WIFI67_FW_REGION_SHARED  BIT(2)  /* Shared memory */

/* Hardware register offsets */
#define WIFI67_REG_FW_STATUS    0x0100
#define WIFI67_REG_FW_ERROR     0x0104
#define WIFI67_REG_FW_CONTROL   0x0108
#define WIFI67_REG_FW_IRAM_ADDR 0x010C
#define WIFI67_REG_FW_DRAM_ADDR 0x0110
#define WIFI67_REG_FW_SRAM_ADDR 0x0114
#define WIFI67_REG_IPC_ADDR     0x0118
#define WIFI67_REG_IPC_SIZE     0x011C

/* Firmware status register bits */
#define WIFI67_FW_IRQ_CRASH     BIT(0)
#define WIFI67_FW_IRQ_READY     BIT(1)
#define WIFI67_FW_RADIO_ID_MASK GENMASK(7, 4)

/* Firmware states */
enum wifi67_fw_state {
    WIFI67_FW_STATE_RESET,
    WIFI67_FW_STATE_LOADED,
    WIFI67_FW_STATE_STARTING,
    WIFI67_FW_STATE_READY,
    WIFI67_FW_STATE_CRASHED
};

/* Function prototypes */
int wifi67_emlfm_init(struct wifi67_priv *priv);
void wifi67_emlfm_deinit(struct wifi67_priv *priv);
int wifi67_emlfm_load_fw(struct wifi67_priv *priv, u8 radio_id,
                        const char *name);
int wifi67_emlfm_start_fw(struct wifi67_priv *priv, u8 radio_id);
void wifi67_emlfm_stop_fw(struct wifi67_priv *priv, u8 radio_id);

/* Hardware abstraction layer functions that must be implemented by the driver */
int wifi67_hw_load_fw(struct wifi67_priv *priv, u8 radio_id,
                     const void *data, size_t size,
                     dma_addr_t iram_addr, dma_addr_t dram_addr,
                     dma_addr_t sram_addr);
int wifi67_hw_start_fw(struct wifi67_priv *priv, u8 radio_id,
                      dma_addr_t ipc_addr, size_t ipc_size);
void wifi67_hw_stop_fw(struct wifi67_priv *priv, u8 radio_id);
void wifi67_hw_reset_radio(struct wifi67_priv *priv, u8 radio_id);
u32 wifi67_hw_read32(struct wifi67_priv *priv, u32 reg);
void wifi67_hw_write32(struct wifi67_priv *priv, u32 reg, u32 val);
int wifi67_hw_check_link_quality(struct wifi67_priv *priv, u8 link_id);
u8 wifi67_hw_find_best_radio(struct wifi67_priv *priv, u8 link_id);
int wifi67_hw_switch_link_radio(struct wifi67_priv *priv, u8 link_id,
                              u8 radio_id);

#endif /* __WIFI67_EMLFM_H */ 