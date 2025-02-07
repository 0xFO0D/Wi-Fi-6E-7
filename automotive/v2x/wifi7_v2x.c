/*
 * WiFi 7 V2X Communication Module
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/ktime.h>
#include <linux/random.h>
#include "wifi7_v2x.h"
#include "../core/wifi7_core.h"

/* V2X device context */
struct wifi7_v2x_dev {
    struct wifi7_dev *dev;           /* Core device structure */
    struct wifi7_v2x_config config;  /* V2X configuration */
    struct wifi7_v2x_stats stats;    /* V2X statistics */
    struct dentry *debugfs_dir;      /* debugfs directory */
    spinlock_t lock;                /* Device lock */
    bool initialized;               /* Initialization flag */
    struct {
        struct delayed_work tx_work;  /* Message transmission */
        struct delayed_work rx_work;  /* Message reception */
        struct delayed_work stats_work; /* Statistics collection */
        struct completion msg_done;   /* Message completion */
    } workers;
    struct {
        struct sk_buff_head emergency; /* Emergency messages */
        struct sk_buff_head safety;    /* Safety messages */
        struct sk_buff_head mobility;  /* Mobility messages */
        struct sk_buff_head info;      /* Information messages */
        spinlock_t lock;              /* Queue lock */
    } queues;
};

static struct wifi7_v2x_dev *v2x_dev;

/* Queue management helpers */
static struct sk_buff_head *get_queue_by_priority(struct wifi7_v2x_dev *dev,
                                                u8 priority)
{
    switch (priority) {
    case WIFI7_V2X_PRIO_EMERGENCY:
        return &dev->queues.emergency;
    case WIFI7_V2X_PRIO_SAFETY:
        return &dev->queues.safety;
    case WIFI7_V2X_PRIO_MOBILITY:
        return &dev->queues.mobility;
    case WIFI7_V2X_PRIO_INFO:
        return &dev->queues.info;
    default:
        return NULL;
    }
}

static void v2x_update_stats(struct wifi7_v2x_dev *dev,
                           u8 msg_type,
                           bool success,
                           u32 latency)
{
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    
    if (success) {
        dev->stats.msgs_tx++;
        switch (msg_type) {
        case WIFI7_V2X_MSG_BSM:
        case WIFI7_V2X_MSG_EVA:
            dev->stats.msg_counts.emergency++;
            break;
        case WIFI7_V2X_MSG_RSA:
            dev->stats.msg_counts.safety++;
            break;
        case WIFI7_V2X_MSG_TIM:
            dev->stats.msg_counts.mobility++;
            break;
        case WIFI7_V2X_MSG_SPAT:
        case WIFI7_V2X_MSG_MAP:
            dev->stats.msg_counts.info++;
            break;
        }

        /* Update latency statistics */
        if (latency) {
            if (latency > dev->stats.latency_max)
                dev->stats.latency_max = latency;
            dev->stats.latency_avg = (dev->stats.latency_avg + latency) / 2;
        }
    } else {
        dev->stats.msgs_dropped++;
    }

    spin_unlock_irqrestore(&dev->lock, flags);
}

/* Work handlers */
static void v2x_tx_work_handler(struct work_struct *work)
{
    struct wifi7_v2x_dev *dev = v2x_dev;
    struct sk_buff_head *queue;
    struct sk_buff *skb;
    int i, ret;
    ktime_t start, end;
    u32 latency;

    /* Process queues in priority order */
    for (i = WIFI7_V2X_PRIO_EMERGENCY; i <= WIFI7_V2X_PRIO_INFO; i++) {
        queue = get_queue_by_priority(dev, i);
        if (!queue)
            continue;

        while ((skb = skb_dequeue(queue))) {
            start = ktime_get();
            
            /* Transmit the message */
            ret = wifi7_tx(dev->dev, skb);
            
            end = ktime_get();
            latency = ktime_to_us(ktime_sub(end, start));

            v2x_update_stats(dev, skb->priority, ret == 0, latency);

            if (ret) {
                dev->stats.retries++;
                if (dev->stats.retries < dev->config.max_retries)
                    skb_queue_head(queue, skb);
                else
                    dev_kfree_skb(skb);
            }
        }
    }

    /* Schedule next transmission */
    if (dev->initialized)
        schedule_delayed_work(&dev->workers.tx_work,
                            msecs_to_jiffies(dev->config.channel_interval));
}

static void v2x_rx_work_handler(struct work_struct *work)
{
    struct wifi7_v2x_dev *dev = v2x_dev;
    struct sk_buff *skb;
    ktime_t start, end;
    u32 latency;

    while ((skb = wifi7_rx(dev->dev))) {
        start = ktime_get();

        /* Process received message */
        if (dev->config.security_enabled) {
            /* Validate message security */
            if (!wifi7_v2x_validate_security(skb)) {
                dev->stats.security_failures++;
                dev_kfree_skb(skb);
                continue;
            }
        }

        /* Update statistics */
        end = ktime_get();
        latency = ktime_to_us(ktime_sub(end, start));
        dev->stats.msgs_rx++;
        
        if (latency > dev->stats.latency_max)
            dev->stats.latency_max = latency;
        dev->stats.latency_avg = (dev->stats.latency_avg + latency) / 2;

        /* Forward message to upper layer */
        wifi7_v2x_deliver_msg(dev->dev, skb);
    }

    /* Schedule next reception */
    if (dev->initialized)
        schedule_delayed_work(&dev->workers.rx_work, HZ/10);
}

static void v2x_stats_work_handler(struct work_struct *work)
{
    struct wifi7_v2x_dev *dev = v2x_dev;
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    
    /* Check for congestion */
    if (dev->stats.msgs_dropped > dev->stats.msgs_tx / 10)
        dev->stats.congestion_events++;

    /* Update range statistics if available */
    if (dev->dev->range_info)
        dev->stats.range_avg = dev->dev->range_info->avg_range;

    spin_unlock_irqrestore(&dev->lock, flags);

    /* Schedule next update */
    if (dev->initialized)
        schedule_delayed_work(&dev->workers.stats_work, HZ);
}

/* Module initialization */
int wifi7_v2x_init(struct wifi7_dev *dev)
{
    struct wifi7_v2x_dev *v2x;
    int ret;

    v2x = kzalloc(sizeof(*v2x), GFP_KERNEL);
    if (!v2x)
        return -ENOMEM;

    v2x->dev = dev;
    spin_lock_init(&v2x->lock);
    spin_lock_init(&v2x->queues.lock);

    /* Initialize message queues */
    skb_queue_head_init(&v2x->queues.emergency);
    skb_queue_head_init(&v2x->queues.safety);
    skb_queue_head_init(&v2x->queues.mobility);
    skb_queue_head_init(&v2x->queues.info);

    /* Initialize work items */
    INIT_DELAYED_WORK(&v2x->workers.tx_work, v2x_tx_work_handler);
    INIT_DELAYED_WORK(&v2x->workers.rx_work, v2x_rx_work_handler);
    INIT_DELAYED_WORK(&v2x->workers.stats_work, v2x_stats_work_handler);
    init_completion(&v2x->workers.msg_done);

    /* Set default configuration */
    v2x->config.mode = WIFI7_V2X_MODE_DIRECT;
    v2x->config.max_retries = 3;
    v2x->config.channel_interval = 100;
    v2x->config.intervals.emergency = 50;
    v2x->config.intervals.safety = 100;
    v2x->config.intervals.mobility = 200;
    v2x->config.intervals.info = 500;

    v2x->initialized = true;
    v2x_dev = v2x;

    /* Initialize debugfs */
    ret = wifi7_v2x_debugfs_init(dev);
    if (ret)
        goto err_free;

    return 0;

err_free:
    kfree(v2x);
    return ret;
}

void wifi7_v2x_deinit(struct wifi7_dev *dev)
{
    struct wifi7_v2x_dev *v2x = v2x_dev;

    if (!v2x)
        return;

    v2x->initialized = false;

    /* Cancel pending work */
    cancel_delayed_work_sync(&v2x->workers.tx_work);
    cancel_delayed_work_sync(&v2x->workers.rx_work);
    cancel_delayed_work_sync(&v2x->workers.stats_work);

    /* Clean up queues */
    skb_queue_purge(&v2x->queues.emergency);
    skb_queue_purge(&v2x->queues.safety);
    skb_queue_purge(&v2x->queues.mobility);
    skb_queue_purge(&v2x->queues.info);

    wifi7_v2x_debugfs_remove(dev);
    kfree(v2x);
    v2x_dev = NULL;
}

int wifi7_v2x_start(struct wifi7_dev *dev)
{
    struct wifi7_v2x_dev *v2x = v2x_dev;

    if (!v2x || !v2x->initialized)
        return -EINVAL;

    /* Start work handlers */
    schedule_delayed_work(&v2x->workers.tx_work, 0);
    schedule_delayed_work(&v2x->workers.rx_work, 0);
    schedule_delayed_work(&v2x->workers.stats_work, 0);

    return 0;
}

void wifi7_v2x_stop(struct wifi7_dev *dev)
{
    struct wifi7_v2x_dev *v2x = v2x_dev;

    if (!v2x)
        return;

    /* Stop work handlers */
    cancel_delayed_work_sync(&v2x->workers.tx_work);
    cancel_delayed_work_sync(&v2x->workers.rx_work);
    cancel_delayed_work_sync(&v2x->workers.stats_work);
}

int wifi7_v2x_set_config(struct wifi7_dev *dev,
                        struct wifi7_v2x_config *config)
{
    struct wifi7_v2x_dev *v2x = v2x_dev;
    unsigned long flags;

    if (!v2x || !config)
        return -EINVAL;

    spin_lock_irqsave(&v2x->lock, flags);
    memcpy(&v2x->config, config, sizeof(*config));
    spin_unlock_irqrestore(&v2x->lock, flags);

    return 0;
}

int wifi7_v2x_get_config(struct wifi7_dev *dev,
                        struct wifi7_v2x_config *config)
{
    struct wifi7_v2x_dev *v2x = v2x_dev;
    unsigned long flags;

    if (!v2x || !config)
        return -EINVAL;

    spin_lock_irqsave(&v2x->lock, flags);
    memcpy(config, &v2x->config, sizeof(*config));
    spin_unlock_irqrestore(&v2x->lock, flags);

    return 0;
}

int wifi7_v2x_send_msg(struct wifi7_dev *dev,
                      struct sk_buff *skb,
                      u8 msg_type,
                      u8 priority)
{
    struct wifi7_v2x_dev *v2x = v2x_dev;
    struct sk_buff_head *queue;
    unsigned long flags;

    if (!v2x || !skb)
        return -EINVAL;

    queue = get_queue_by_priority(v2x, priority);
    if (!queue)
        return -EINVAL;

    /* Set message type in skb->priority for stats tracking */
    skb->priority = msg_type;

    spin_lock_irqsave(&v2x->queues.lock, flags);
    skb_queue_tail(queue, skb);
    spin_unlock_irqrestore(&v2x->queues.lock, flags);

    /* Trigger immediate transmission for emergency messages */
    if (priority == WIFI7_V2X_PRIO_EMERGENCY)
        mod_delayed_work(system_wq, &v2x->workers.tx_work, 0);

    return 0;
}

int wifi7_v2x_get_stats(struct wifi7_dev *dev,
                       struct wifi7_v2x_stats *stats)
{
    struct wifi7_v2x_dev *v2x = v2x_dev;
    unsigned long flags;

    if (!v2x || !stats)
        return -EINVAL;

    spin_lock_irqsave(&v2x->lock, flags);
    memcpy(stats, &v2x->stats, sizeof(*stats));
    spin_unlock_irqrestore(&v2x->lock, flags);

    return 0;
}

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 V2X Communication Module");
MODULE_VERSION("1.0"); 