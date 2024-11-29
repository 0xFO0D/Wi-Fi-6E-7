#ifndef _WIFI67_DEBUG_H_
#define _WIFI67_DEBUG_H_

#include <linux/device.h>
#include "wifi67_types.h"

/* Debug levels */
#define WIFI67_DEBUG_FATAL     0
#define WIFI67_DEBUG_ERROR     1
#define WIFI67_DEBUG_WARNING   2
#define WIFI67_DEBUG_INFO      3
#define WIFI67_DEBUG_DEBUG     4
#define WIFI67_DEBUG_TRACE     5

/* Debug zones */
#define WIFI67_DEBUG_CORE      BIT(0)
#define WIFI67_DEBUG_MAC       BIT(1)
#define WIFI67_DEBUG_PHY       BIT(2)
#define WIFI67_DEBUG_DMA       BIT(3)
#define WIFI67_DEBUG_REG       BIT(4)
#define WIFI67_DEBUG_HAL       BIT(5)
#define WIFI67_DEBUG_TX        BIT(6)
#define WIFI67_DEBUG_RX        BIT(7)
#define WIFI67_DEBUG_IRQ       BIT(8)
#define WIFI67_DEBUG_POWER     BIT(9)
#define WIFI67_DEBUG_THERMAL   BIT(10)
#define WIFI67_DEBUG_FIRMWARE  BIT(11)
#define WIFI67_DEBUG_BEACON    BIT(12)
#define WIFI67_DEBUG_SCAN      BIT(13)
#define WIFI67_DEBUG_AUTH      BIT(14)
#define WIFI67_DEBUG_ASSOC     BIT(15)

/* Debug configuration */
extern unsigned int wifi67_debug_level;
extern unsigned int wifi67_debug_zones;

/* Debug macros */
#define wifi67_printk(level, priv, fmt, ...) \
    dev_printk(level, &(priv)->pdev->dev, \
               "wifi67: " fmt, ##__VA_ARGS__)

#define wifi67_emerg(priv, fmt, ...) \
    wifi67_printk(KERN_EMERG, priv, fmt, ##__VA_ARGS__)

#define wifi67_alert(priv, fmt, ...) \
    wifi67_printk(KERN_ALERT, priv, fmt, ##__VA_ARGS__)

#define wifi67_crit(priv, fmt, ...) \
    wifi67_printk(KERN_CRIT, priv, fmt, ##__VA_ARGS__)

#define wifi67_err(priv, fmt, ...) \
    wifi67_printk(KERN_ERR, priv, fmt, ##__VA_ARGS__)

#define wifi67_warning(priv, fmt, ...) \
    wifi67_printk(KERN_WARNING, priv, fmt, ##__VA_ARGS__)

#define wifi67_notice(priv, fmt, ...) \
    wifi67_printk(KERN_NOTICE, priv, fmt, ##__VA_ARGS__)

#define wifi67_info(priv, fmt, ...) \
    wifi67_printk(KERN_INFO, priv, fmt, ##__VA_ARGS__)

#define wifi67_debug(zone, priv, fmt, ...) \
    do { \
        if (wifi67_debug_level >= WIFI67_DEBUG_DEBUG && \
            (wifi67_debug_zones & (zone))) \
            wifi67_printk(KERN_DEBUG, priv, fmt, ##__VA_ARGS__); \
    } while (0)

#define wifi67_trace(zone, priv, fmt, ...) \
    do { \
        if (wifi67_debug_level >= WIFI67_DEBUG_TRACE && \
            (wifi67_debug_zones & (zone))) \
            wifi67_printk(KERN_DEBUG, priv, "%s: " fmt, \
                         __func__, ##__VA_ARGS__); \
    } while (0)

/* Function prototypes */
void wifi67_debug_init(void);
void wifi67_debug_exit(void);
void wifi67_debug_dump_registers(struct wifi67_priv *priv);
void wifi67_debug_dump_tx_ring(struct wifi67_priv *priv, int ring);
void wifi67_debug_dump_rx_ring(struct wifi67_priv *priv, int ring);
void wifi67_debug_dump_stats(struct wifi67_priv *priv);

#endif /* _WIFI67_DEBUG_H_ */ 