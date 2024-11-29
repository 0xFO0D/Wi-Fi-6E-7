#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include "../../include/debug/debug.h"

void wifi67_debug(struct wifi67_priv *priv, u32 level, const char *fmt, ...)
{
    va_list args;
    struct wifi67_debugfs *debugfs = &priv->debugfs;

    if (!(debugfs->debug_mask & level))
        return;

    va_start(args, fmt);
    vprintk(fmt, args);
    va_end(args);
}

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

static const struct file_operations wifi67_debug_level_ops = {
    .read = wifi67_debug_level_read,
    .write = wifi67_debug_level_write,
    .open = simple_open,
    .llseek = default_llseek,
};

int wifi67_debugfs_init(struct wifi67_priv *priv)
{
    struct wifi67_debugfs *debugfs = &priv->debugfs;
    char dirname[32];
    int ret = 0;

    snprintf(dirname, sizeof(dirname), "wifi67-%s",
             dev_name(&priv->pdev->dev));

    debugfs->dir = debugfs_create_dir(dirname, NULL);
    if (IS_ERR(debugfs->dir)) {
        ret = PTR_ERR(debugfs->dir);
        goto err;
    }

    debugfs->debug_level = debugfs_create_file("debug_level", 0600,
                                             debugfs->dir, priv,
                                             &wifi67_debug_level_ops);
    if (IS_ERR(debugfs->debug_level)) {
        ret = PTR_ERR(debugfs->debug_level);
        goto err_remove;
    }

    debugfs->debug_mask = WIFI67_DEBUG_ERROR | WIFI67_DEBUG_WARNING;
    return 0;

err_remove:
    debugfs_remove_recursive(debugfs->dir);
err:
    return ret;
}

void wifi67_debugfs_remove(struct wifi67_priv *priv)
{
    debugfs_remove_recursive(priv->debugfs.dir);
}

EXPORT_SYMBOL_GPL(wifi67_debug);
EXPORT_SYMBOL_GPL(wifi67_debugfs_init);
EXPORT_SYMBOL_GPL(wifi67_debugfs_remove); 