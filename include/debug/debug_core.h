#ifndef _WIFI67_DEBUG_CORE_H_
#define _WIFI67_DEBUG_CORE_H_

#include <linux/debugfs.h>
#include "../core/wifi67_forward.h"
#include "debug_types.h"

/* Debug function prototypes */
int wifi67_debugfs_init(struct wifi67_priv *priv);
void wifi67_debugfs_remove(struct wifi67_priv *priv);
void wifi67_dbg(struct wifi67_priv *priv, u32 level, const char *fmt, ...);

/* Debug macros */
#define wifi67_err(priv, fmt, args...) \
    wifi67_dbg(priv, WIFI67_DBG_ERROR, fmt, ##args)

#define wifi67_warn(priv, fmt, args...) \
    wifi67_dbg(priv, WIFI67_DBG_WARNING, fmt, ##args)

#define wifi67_info(priv, fmt, args...) \
    wifi67_dbg(priv, WIFI67_DBG_INFO, fmt, ##args)

#endif /* _WIFI67_DEBUG_CORE_H_ */ 