#ifndef _WIFI67_DEBUG_TYPES_H_
#define _WIFI67_DEBUG_TYPES_H_

#include <linux/debugfs.h>

/* Debug levels - using BIT() for proper bit shifting */
#define WIFI67_DBG_ERROR     BIT(0)   /* 0x0001 */
#define WIFI67_DBG_WARNING   BIT(1)   /* 0x0002 */
#define WIFI67_DBG_INFO      BIT(2)   /* 0x0004 */
#define WIFI67_DBG_MAC       BIT(3)   /* 0x0008 */
#define WIFI67_DBG_PHY       BIT(4)   /* 0x0010 */
#define WIFI67_DBG_DMA       BIT(5)   /* 0x0020 */
#define WIFI67_DBG_FW        BIT(6)   /* 0x0040 */
#define WIFI67_DBG_REG       BIT(7)   /* 0x0080 */
#define WIFI67_DBG_ALL       0xFFFFFFFF

/* Debug file names */
#define WIFI67_DBG_DIR_NAME          "wifi67"
#define WIFI67_DBG_LEVEL_NAME        "debug_level"
#define WIFI67_DBG_STATS_NAME        "stats"
#define WIFI67_DBG_REGDUMP_NAME      "regdump"

struct wifi67_debugfs {
    struct dentry *dir;
    struct dentry *debug_level;
    struct dentry *reg_dump;
    struct dentry *stats;
    u32 debug_mask;
};

#endif /* _WIFI67_DEBUG_TYPES_H_ */ 