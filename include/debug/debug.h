#ifndef _WIFI67_DEBUG_H_
#define _WIFI67_DEBUG_H_

#include <linux/debugfs.h>
#include "../core/wifi67_types.h"

/* Debug levels */
#define WIFI67_DBG_ERROR     0x0001
#define WIFI67_DBG_WARNING   0x0002
#define WIFI67_DBG_INFO      0x0004
#define WIFI67_DBG_TRACE     0x0008
#define WIFI67_DBG_MAC       0x0010
#define WIFI67_DBG_PHY       0x0020
#define WIFI67_DBG_DMA       0x0040
#define WIFI67_DBG_REG       0x0080
#define WIFI67_DBG_FW        0x0100
#define WIFI67_DBG_CRYPTO    0x0200
#define WIFI67_DBG_HAL       0x0400
#define WIFI67_DBG_ALL       0xFFFF

struct wifi67_debugfs {
    struct dentry *dir;
    struct dentry *stats;
    struct dentry *regs;
    struct dentry *debug_level;
    u32 debug_mask;
};

/* Function declarations */
int wifi67_debugfs_init(struct wifi67_priv *priv);
void wifi67_debugfs_remove(struct wifi67_priv *priv);
void wifi67_dbg(struct wifi67_priv *priv, u32 level, const char *fmt, ...);

#endif /* _WIFI67_DEBUG_H_ */ 