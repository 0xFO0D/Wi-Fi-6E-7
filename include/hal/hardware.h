#ifndef _WIFI67_HARDWARE_H_
#define _WIFI67_HARDWARE_H_

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/wireless.h>
#include "../core/wifi67_types.h"

/* Hardware registers */
#define HW_REG_BASE          0x0000
#define HW_REG_CONTROL       (HW_REG_BASE + 0x00)
#define HW_REG_STATUS        (HW_REG_BASE + 0x04)
#define HW_REG_INT_STATUS    (HW_REG_BASE + 0x08)
#define HW_REG_INT_MASK      (HW_REG_BASE + 0x0C)
#define HW_REG_VERSION       (HW_REG_BASE + 0x10)
#define HW_REG_CAPABILITIES  (HW_REG_BASE + 0x14)
#define HW_REG_POWER        (HW_REG_BASE + 0x18)
#define HW_REG_RESET        (HW_REG_BASE + 0x1C)

/* Control register bits */
#define HW_CTRL_ENABLE      BIT(0)
#define HW_CTRL_RESET       BIT(1)
#define HW_CTRL_SLEEP       BIT(2)
#define HW_CTRL_INT_EN      BIT(3)
#define HW_CTRL_RX_EN       BIT(4)
#define HW_CTRL_TX_EN       BIT(5)

/* Status register bits */
#define HW_STATUS_READY     BIT(0)
#define HW_STATUS_ERROR     BIT(1)
#define HW_STATUS_SLEEP     BIT(2)
#define HW_STATUS_RX_ACTIVE BIT(3)
#define HW_STATUS_TX_ACTIVE BIT(4)

/* Interrupt bits */
#define HW_INT_RX_DONE      BIT(0)
#define HW_INT_TX_DONE      BIT(1)
#define HW_INT_RX_ERROR     BIT(2)
#define HW_INT_TX_ERROR     BIT(3)
#define HW_INT_TEMP_WARNING BIT(4)
#define HW_INT_RADAR_DETECT BIT(5)

struct hw_stats {
    u32 hw_errors;
    u32 hw_resets;
    u32 hw_timeouts;
    u32 hw_interrupts;
    u32 hw_temp_warnings;
    u32 hw_radar_detects;
} __packed;

/* Function prototypes */
int wifi67_hw_init(struct wifi67_priv *priv);
void wifi67_hw_deinit(struct wifi67_priv *priv);
int wifi67_hw_start(struct wifi67_priv *priv);
void wifi67_hw_stop(struct wifi67_priv *priv);
int wifi67_hw_reset(struct wifi67_priv *priv);
int wifi67_hw_set_power(struct wifi67_priv *priv, bool enable);
u32 wifi67_hw_get_status(struct wifi67_priv *priv);
void wifi67_hw_enable_interrupts(struct wifi67_priv *priv);
void wifi67_hw_disable_interrupts(struct wifi67_priv *priv);
void wifi67_hw_handle_interrupt(struct wifi67_priv *priv);
int wifi67_hw_wait_for_bit(struct wifi67_priv *priv, u32 reg, u32 bit, int timeout);

#endif /* _WIFI67_HARDWARE_H_ */ 