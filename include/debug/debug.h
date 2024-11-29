#ifndef _WIFI67_DEBUG_H_
#define _WIFI67_DEBUG_H_

#include <linux/debugfs.h>
#include "../core/wifi67.h"

/* Debug levels */
#define WIFI67_DEBUG_ERROR   BIT(0)
#define WIFI67_DEBUG_WARNING BIT(1)
#define WIFI67_DEBUG_INFO    BIT(2)
#define WIFI67_DEBUG_MAC     BIT(3)
#define WIFI67_DEBUG_PHY     BIT(4)
#define WIFI67_DEBUG_DMA     BIT(5)
#define WIFI67_DEBUG_FW      BIT(6)
#define WIFI67_DEBUG_REG     BIT(7)
#define WIFI67_DEBUG_TX      BIT(8)
#define WIFI67_DEBUG_RX      BIT(9)
#define WIFI67_DEBUG_ALL     0xFFFFFFFF

void wifi67_debug(struct wifi67_priv *priv, u32 level, const char *fmt, ...);

#define wifi67_dbg(priv, level, fmt, ...) \
    wifi67_debug(priv, level, fmt, ##__VA_ARGS__)

#endif /* _WIFI67_DEBUG_H_ */ 