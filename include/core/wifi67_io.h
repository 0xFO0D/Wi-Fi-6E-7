#ifndef _WIFI67_IO_H_
#define _WIFI67_IO_H_

#include "wifi67_types.h"

static inline u32 wifi67_read32(struct wifi67_priv *priv, u32 reg)
{
    return readl(priv->mmio + reg);
}

static inline void wifi67_write32(struct wifi67_priv *priv, u32 reg, u32 val)
{
    writel(val, priv->mmio + reg);
}

#endif /* _WIFI67_IO_H_ */ 