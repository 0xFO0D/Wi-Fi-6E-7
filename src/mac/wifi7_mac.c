/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/debugfs.h>
#include "wifi7_mac.h"

/* Module parameters */
static int max_ampdu_len = WIFI7_MAX_AMPDU_LEN;
module_param(max_ampdu_len, int, 0644);
MODULE_PARM_DESC(max_ampdu_len, "Maximum A-MPDU length (default: 262144)");

static int max_links = WIFI7_MAX_LINKS;
module_param(max_links, int, 0644);
MODULE_PARM_DESC(max_links, "Maximum number of MLO links (default: 8)");

/* Forward declarations */
static void wifi7_mac_link_work(struct work_struct *work);
static void wifi7_mac_power_work(struct work_struct *work);

/* Device allocation and initialization */
struct wifi7_mac_dev *wifi7_mac_alloc(struct device *dev)
{
    struct wifi7_mac_dev *mac_dev;
    int i;

    mac_dev = kzalloc(sizeof(*mac_dev), GFP_KERNEL);
    if (!mac_dev)
        return NULL;

    mac_dev->dev = dev;
    atomic_set(&mac_dev->active_links, 0);
    atomic_set(&mac_dev->ampdu_len, max_ampdu_len);
    atomic_set(&mac_dev->power_state, 0);

    /* Initialize spinlocks */
    spin_lock_init(&mac_dev->aggr_lock);
    for (i = 0; i < WIFI7_MAX_QUEUES; i++) {
        spin_lock_init(&mac_dev->queue_locks[i]);
        skb_queue_head_init(&mac_dev->queues[i]);
    }

    /* Initialize workqueues */
    mac_dev->mlo_wq = alloc_workqueue("wifi7_mlo_wq", WQ_HIGHPRI | WQ_UNBOUND, 0);
    if (!mac_dev->mlo_wq)
        goto err_free_dev;

    mac_dev->pm_wq = alloc_workqueue("wifi7_pm_wq", WQ_FREEZABLE | WQ_UNBOUND, 0);
    if (!mac_dev->pm_wq)
        goto err_free_mlo_wq;

    /* Initialize link states */
    for (i = 0; i < max_links; i++) {
        mac_dev->links[i].link_id = i;
        mac_dev->links[i].enabled = false;
        mac_dev->links[i].mlo_state = MLO_STATE_DISABLED;
        spin_lock_init(&mac_dev->links[i].lock);
    }

    return mac_dev;

err_free_mlo_wq:
    destroy_workqueue(mac_dev->mlo_wq);
err_free_dev:
    kfree(mac_dev);
    return NULL;
}
EXPORT_SYMBOL_GPL(wifi7_mac_alloc);

void wifi7_mac_free(struct wifi7_mac_dev *dev)
{
    int i;

    if (!dev)
        return;

    /* Clean up workqueues */
    if (dev->mlo_wq)
        destroy_workqueue(dev->mlo_wq);
    if (dev->pm_wq)
        destroy_workqueue(dev->pm_wq);

    /* Free queued packets */
    for (i = 0; i < WIFI7_MAX_QUEUES; i++)
        skb_queue_purge(&dev->queues[i]);

    kfree(dev);
}
EXPORT_SYMBOL_GPL(wifi7_mac_free);

/* Device registration */
int wifi7_mac_register(struct wifi7_mac_dev *dev)
{
    int ret;

    if (!dev || !dev->ops)
        return -EINVAL;

    /* Initialize hardware */
    if (dev->ops->init) {
        ret = dev->ops->init(dev);
        if (ret)
            return ret;
    }

    /* Initialize debugfs */
    ret = wifi7_mac_debugfs_init(dev);
    if (ret)
        goto err_deinit;

    return 0;

err_deinit:
    if (dev->ops->deinit)
        dev->ops->deinit(dev);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_mac_register);

void wifi7_mac_unregister(struct wifi7_mac_dev *dev)
{
    if (!dev)
        return;

    /* Remove debugfs entries */
    wifi7_mac_debugfs_remove(dev);

    /* Deinitialize hardware */
    if (dev->ops && dev->ops->deinit)
        dev->ops->deinit(dev);
}
EXPORT_SYMBOL_GPL(wifi7_mac_unregister);

/* Link management */
int wifi7_mac_link_setup(struct wifi7_mac_dev *dev, u8 link_id)
{
    struct wifi7_link_state *link;
    int ret = 0;

    if (!dev || link_id >= max_links)
        return -EINVAL;

    link = &dev->links[link_id];

    spin_lock_bh(&link->lock);
    if (link->enabled) {
        ret = -EBUSY;
        goto out_unlock;
    }

    /* Update link state */
    link->mlo_state = MLO_STATE_SETUP;
    link->enabled = true;

    /* Call hardware setup */
    if (dev->ops && dev->ops->link_setup) {
        ret = dev->ops->link_setup(dev, link_id);
        if (ret) {
            link->enabled = false;
            link->mlo_state = MLO_STATE_ERROR;
            goto out_unlock;
        }
    }

    atomic_inc(&dev->active_links);
    link->mlo_state = MLO_STATE_ACTIVE;

out_unlock:
    spin_unlock_bh(&link->lock);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_mac_link_setup);

int wifi7_mac_link_teardown(struct wifi7_mac_dev *dev, u8 link_id)
{
    struct wifi7_link_state *link;
    int ret = 0;

    if (!dev || link_id >= max_links)
        return -EINVAL;

    link = &dev->links[link_id];

    spin_lock_bh(&link->lock);
    if (!link->enabled) {
        ret = -EINVAL;
        goto out_unlock;
    }

    link->mlo_state = MLO_STATE_TEARDOWN;

    /* Call hardware teardown */
    if (dev->ops && dev->ops->link_teardown) {
        ret = dev->ops->link_teardown(dev, link_id);
        if (ret) {
            link->mlo_state = MLO_STATE_ERROR;
            goto out_unlock;
        }
    }

    link->enabled = false;
    atomic_dec(&dev->active_links);
    link->mlo_state = MLO_STATE_DISABLED;

out_unlock:
    spin_unlock_bh(&link->lock);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_mac_link_teardown);

/* Frame transmission and reception */
int wifi7_mac_tx_frame(struct wifi7_mac_dev *dev, struct sk_buff *skb, u8 link_id)
{
    struct wifi7_link_state *link;
    int ret = 0;

    if (!dev || !skb || link_id >= max_links)
        return -EINVAL;

    link = &dev->links[link_id];

    spin_lock_bh(&link->lock);
    if (!link->enabled || link->mlo_state != MLO_STATE_ACTIVE) {
        ret = -EINVAL;
        goto out_unlock;
    }

    /* Update statistics */
    link->tx_bytes += skb->len;
    link->tx_packets++;

    /* Call hardware TX */
    if (dev->ops && dev->ops->tx_frame) {
        ret = dev->ops->tx_frame(dev, skb, link_id);
        if (ret)
            link->tx_errors++;
    }

out_unlock:
    spin_unlock_bh(&link->lock);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_mac_tx_frame);

int wifi7_mac_rx_frame(struct wifi7_mac_dev *dev, struct sk_buff *skb, u8 link_id)
{
    struct wifi7_link_state *link;
    int ret = 0;

    if (!dev || !skb || link_id >= max_links)
        return -EINVAL;

    link = &dev->links[link_id];

    spin_lock_bh(&link->lock);
    if (!link->enabled || link->mlo_state != MLO_STATE_ACTIVE) {
        ret = -EINVAL;
        goto out_unlock;
    }

    /* Update statistics */
    link->rx_bytes += skb->len;
    link->rx_packets++;

    /* Call hardware RX */
    if (dev->ops && dev->ops->rx_frame) {
        ret = dev->ops->rx_frame(dev, skb, link_id);
        if (ret)
            link->rx_errors++;
    }

out_unlock:
    spin_unlock_bh(&link->lock);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_mac_rx_frame);

/* Power management */
int wifi7_mac_set_power_save(struct wifi7_mac_dev *dev, u8 link_id, bool enable)
{
    struct wifi7_link_state *link;
    int ret = 0;

    if (!dev || link_id >= max_links)
        return -EINVAL;

    link = &dev->links[link_id];

    spin_lock_bh(&link->lock);
    if (!link->enabled) {
        ret = -EINVAL;
        goto out_unlock;
    }

    if (link->power_save == enable)
        goto out_unlock;

    /* Call hardware power save */
    if (dev->ops && dev->ops->set_power_state) {
        ret = dev->ops->set_power_state(dev, link_id, enable);
        if (ret)
            goto out_unlock;
    }

    link->power_save = enable;

out_unlock:
    spin_unlock_bh(&link->lock);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_mac_set_power_save);

/* Multi-TID aggregation */
int wifi7_mac_set_multi_tid(struct wifi7_mac_dev *dev,
                          struct wifi7_multi_tid_config *config)
{
    if (!dev || !config)
        return -EINVAL;

    spin_lock_bh(&dev->aggr_lock);
    memcpy(&dev->multi_tid, config, sizeof(*config));
    spin_unlock_bh(&dev->aggr_lock);

    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_mac_set_multi_tid);

/* Module initialization */
static int __init wifi7_mac_init(void)
{
    pr_info("WiFi 7 MAC layer initialized\n");
    return 0;
}

static void __exit wifi7_mac_exit(void)
{
    pr_info("WiFi 7 MAC layer unloaded\n");
}

module_init(wifi7_mac_init);
module_exit(wifi7_mac_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 MAC Layer Core");
MODULE_VERSION("1.0"); 