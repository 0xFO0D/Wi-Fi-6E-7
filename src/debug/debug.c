#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "../../include/core/wifi67.h"
#include "../../include/debug/debug_core.h"

/* Debug level read/write handlers */
static ssize_t wifi67_debug_level_read(struct file *file,
                                     char __user *user_buf,
                                     size_t count, loff_t *ppos)
{
    struct wifi67_priv *priv = file->private_data;
    char buf[32];
    int len;

    len = scnprintf(buf, sizeof(buf), "0x%08x\n", priv->debugfs.debug_mask);
    return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t wifi67_debug_level_write(struct file *file,
                                      const char __user *user_buf,
                                      size_t count, loff_t *ppos)
{
    struct wifi67_priv *priv = file->private_data;
    unsigned long value;
    int ret;

    ret = kstrtoul_from_user(user_buf, count, 0, &value);
    if (ret)
        return ret;

    priv->debugfs.debug_mask = value;
    return count;
}

static const struct file_operations wifi67_debug_level_fops = {
    .read = wifi67_debug_level_read,
    .write = wifi67_debug_level_write,
    .open = simple_open,
    .llseek = default_llseek,
};

/* Initialize debugfs */
int wifi67_debugfs_init(struct wifi67_priv *priv)
{
    struct wifi67_debugfs *debugfs = &priv->debugfs;

    debugfs->dir = debugfs_create_dir(WIFI67_DBG_DIR_NAME, NULL);
    if (IS_ERR(debugfs->dir))
        return PTR_ERR(debugfs->dir);

    debugfs->debug_level = debugfs_create_file(WIFI67_DBG_LEVEL_NAME,
                                             0644, debugfs->dir,
                                             priv, &wifi67_debug_level_fops);
    if (IS_ERR(debugfs->debug_level))
        goto err_remove_dir;

    debugfs->debug_mask = WIFI67_DBG_ERROR | WIFI67_DBG_WARNING;
    return 0;

err_remove_dir:
    debugfs_remove_recursive(debugfs->dir);
    return PTR_ERR(debugfs->debug_level);
}

void wifi67_debugfs_remove(struct wifi67_priv *priv)
{
    debugfs_remove_recursive(priv->debugfs.dir);
}

void wifi67_dbg(struct wifi67_priv *priv, u32 level, const char *fmt, ...)
{
    va_list args;

    if (!(priv->debugfs.debug_mask & level))
        return;

    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
} 