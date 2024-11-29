#ifndef _WIFI67_DEBUGFS_H_
#define _WIFI67_DEBUGFS_H_

#include <linux/debugfs.h>

struct wifi67_debugfs {
    struct dentry *dir;
    struct dentry *debug_level;
    struct dentry *stats;
    struct dentry *registers;
    u32 debug_mask;
};

#endif /* _WIFI67_DEBUGFS_H_ */ 