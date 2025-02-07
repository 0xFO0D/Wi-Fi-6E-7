/*
 * WiFi 7 CAN Bus Integration Module
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/can.h>
#include <linux/can/dev.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/ktime.h>
#include "wifi7_can.h"
#include "../core/wifi7_core.h"

/* CAN device context */
struct wifi7_can_dev {
    struct wifi7_dev *dev;           /* Core device structure */
    struct wifi7_can_config config;  /* CAN configuration */
    struct wifi7_can_stats stats;    /* CAN statistics */
    struct dentry *debugfs_dir;      /* debugfs directory */
    spinlock_t lock;                /* Device lock */
    bool initialized;               /* Initialization flag */
    struct {
        struct delayed_work tx_work;  /* Frame transmission */
        struct delayed_work rx_work;  /* Frame reception */
        struct delayed_work err_work; /* Error handling */
        struct completion frame_done; /* Frame completion */
    } workers;
    struct {
        struct sk_buff_head tx_queue; /* Transmission queue */
        struct sk_buff_head rx_queue; /* Reception queue */
        spinlock_t lock;             /* Queue lock */
    } queues;
    struct {
        u8 state;                    /* Interface state */
        u8 tx_err_counter;           /* TX error counter */
        u8 rx_err_counter;           /* RX error counter */
        bool bus_off;                /* Bus-off state */
        bool error_warning;          /* Error warning state */
        bool error_passive;          /* Error passive state */
    } status;
};

static struct wifi7_can_dev *can_dev;

/* Error handling helpers */
static void can_handle_state_change(struct wifi7_can_dev *dev, u8 new_state)
{
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);
    
    if (new_state != dev->status.state) {
        switch (new_state) {
        case WIFI7_CAN_STATE_ERROR:
            dev->stats.bus_errors++;
            break;
        case WIFI7_CAN_STATE_SLEEP:
            /* Handle sleep mode transition */
            break;
        }
        dev->status.state = new_state;
    }

    spin_unlock_irqrestore(&dev->lock, flags);
}

static void can_handle_error(struct wifi7_can_dev *dev,
                           struct can_frame *cf)
{
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);

    if (cf->can_id & CAN_ERR_RESTARTED) {
        dev->stats.bus_errors++;
        dev->status.bus_off = false;
    }
    if (cf->can_id & CAN_ERR_BUSOFF) {
        dev->stats.bus_errors++;
        dev->status.bus_off = true;
    }
    if (cf->can_id & CAN_ERR_CRTL) {
        if (cf->data[1] & CAN_ERR_CRTL_TX_WARNING)
            dev->status.error_warning = true;
        if (cf->data[1] & CAN_ERR_CRTL_TX_PASSIVE)
            dev->status.error_passive = true;
    }

    spin_unlock_irqrestore(&dev->lock, flags);
}

/* Work handlers */
static void can_tx_work_handler(struct work_struct *work)
{
    struct wifi7_can_dev *dev = can_dev;
    struct sk_buff *skb;
    struct can_frame *cf;
    int ret;

    while ((skb = skb_dequeue(&dev->queues.tx_queue))) {
        cf = (struct can_frame *)skb->data;

        /* Transmit the frame */
        ret = wifi7_can_hw_send_frame(dev->dev, cf);
        if (ret) {
            dev->stats.frames_dropped++;
            if (ret == -EBUSY)
                dev->stats.tx_timeouts++;
            /* Requeue frame if retries available */
            if (dev->config.queue.tx_queue_size > 0)
                skb_queue_head(&dev->queues.tx_queue, skb);
            else
                dev_kfree_skb(skb);
        } else {
            dev->stats.frames_tx++;
            dev_kfree_skb(skb);
        }
    }

    /* Schedule next transmission if needed */
    if (!skb_queue_empty(&dev->queues.tx_queue) && dev->initialized)
        schedule_delayed_work(&dev->workers.tx_work, 1);
}

static void can_rx_work_handler(struct work_struct *work)
{
    struct wifi7_can_dev *dev = can_dev;
    struct sk_buff *skb;
    struct can_frame *cf;

    while ((skb = skb_dequeue(&dev->queues.rx_queue))) {
        cf = (struct can_frame *)skb->data;

        /* Handle error frames */
        if (cf->can_id & CAN_ERR_FLAG) {
            can_handle_error(dev, cf);
            dev_kfree_skb(skb);
            continue;
        }

        /* Process received frame */
        dev->stats.frames_rx++;
        
        /* Forward frame to upper layer */
        wifi7_can_deliver_frame(dev->dev, cf);
        dev_kfree_skb(skb);
    }

    /* Schedule next reception */
    if (dev->initialized)
        schedule_delayed_work(&dev->workers.rx_work, 1);
}

static void can_err_work_handler(struct work_struct *work)
{
    struct wifi7_can_dev *dev = can_dev;
    unsigned long flags;

    spin_lock_irqsave(&dev->lock, flags);

    /* Check error thresholds */
    if (dev->status.tx_err_counter > 96 ||
        dev->status.rx_err_counter > 96)
        dev->status.error_warning = true;

    if (dev->status.tx_err_counter > 127 ||
        dev->status.rx_err_counter > 127)
        dev->status.error_passive = true;

    if (dev->status.tx_err_counter > 255) {
        dev->status.bus_off = true;
        can_handle_state_change(dev, WIFI7_CAN_STATE_ERROR);
    }

    spin_unlock_irqrestore(&dev->lock, flags);

    /* Schedule next check */
    if (dev->initialized)
        schedule_delayed_work(&dev->workers.err_work, HZ);
}

/* Module initialization */
int wifi7_can_init(struct wifi7_dev *dev)
{
    struct wifi7_can_dev *can;
    int ret;

    can = kzalloc(sizeof(*can), GFP_KERNEL);
    if (!can)
        return -ENOMEM;

    can->dev = dev;
    spin_lock_init(&can->lock);
    spin_lock_init(&can->queues.lock);

    /* Initialize queues */
    skb_queue_head_init(&can->queues.tx_queue);
    skb_queue_head_init(&can->queues.rx_queue);

    /* Initialize work items */
    INIT_DELAYED_WORK(&can->workers.tx_work, can_tx_work_handler);
    INIT_DELAYED_WORK(&can->workers.rx_work, can_rx_work_handler);
    INIT_DELAYED_WORK(&can->workers.err_work, can_err_work_handler);
    init_completion(&can->workers.frame_done);

    /* Set default configuration */
    can->config.enabled = true;
    can->config.bitrate = 500000; /* 500 kbps */
    can->config.sjw = 1;
    can->config.tseg1 = 6;
    can->config.tseg2 = 3;
    can->config.queue.rx_queue_size = 1024;
    can->config.queue.tx_queue_size = 1024;
    can->config.queue.timeout = 100;

    can->status.state = WIFI7_CAN_STATE_DOWN;
    can->initialized = true;
    can_dev = can;

    /* Initialize debugfs */
    ret = wifi7_can_debugfs_init(dev);
    if (ret)
        goto err_free;

    return 0;

err_free:
    kfree(can);
    return ret;
}

void wifi7_can_deinit(struct wifi7_dev *dev)
{
    struct wifi7_can_dev *can = can_dev;

    if (!can)
        return;

    can->initialized = false;

    /* Cancel pending work */
    cancel_delayed_work_sync(&can->workers.tx_work);
    cancel_delayed_work_sync(&can->workers.rx_work);
    cancel_delayed_work_sync(&can->workers.err_work);

    /* Clean up queues */
    skb_queue_purge(&can->queues.tx_queue);
    skb_queue_purge(&can->queues.rx_queue);

    wifi7_can_debugfs_remove(dev);
    kfree(can);
    can_dev = NULL;
}

int wifi7_can_start(struct wifi7_dev *dev)
{
    struct wifi7_can_dev *can = can_dev;

    if (!can || !can->initialized)
        return -EINVAL;

    /* Configure CAN hardware */
    ret = wifi7_can_hw_setup(dev, &can->config);
    if (ret)
        return ret;

    can->status.state = WIFI7_CAN_STATE_UP;

    /* Start work handlers */
    schedule_delayed_work(&can->workers.tx_work, 0);
    schedule_delayed_work(&can->workers.rx_work, 0);
    schedule_delayed_work(&can->workers.err_work, 0);

    return 0;
}

void wifi7_can_stop(struct wifi7_dev *dev)
{
    struct wifi7_can_dev *can = can_dev;

    if (!can)
        return;

    can->status.state = WIFI7_CAN_STATE_DOWN;

    /* Stop work handlers */
    cancel_delayed_work_sync(&can->workers.tx_work);
    cancel_delayed_work_sync(&can->workers.rx_work);
    cancel_delayed_work_sync(&can->workers.err_work);

    /* Reset hardware */
    wifi7_can_hw_reset(dev);
}

int wifi7_can_set_config(struct wifi7_dev *dev,
                        struct wifi7_can_config *config)
{
    struct wifi7_can_dev *can = can_dev;
    unsigned long flags;
    int ret = 0;

    if (!can || !config)
        return -EINVAL;

    spin_lock_irqsave(&can->lock, flags);

    /* Apply configuration */
    if (can->status.state == WIFI7_CAN_STATE_UP) {
        ret = wifi7_can_hw_setup(dev, config);
        if (ret)
            goto out;
    }

    memcpy(&can->config, config, sizeof(*config));

out:
    spin_unlock_irqrestore(&can->lock, flags);
    return ret;
}

int wifi7_can_get_config(struct wifi7_dev *dev,
                        struct wifi7_can_config *config)
{
    struct wifi7_can_dev *can = can_dev;
    unsigned long flags;

    if (!can || !config)
        return -EINVAL;

    spin_lock_irqsave(&can->lock, flags);
    memcpy(config, &can->config, sizeof(*config));
    spin_unlock_irqrestore(&can->lock, flags);

    return 0;
}

int wifi7_can_send_frame(struct wifi7_dev *dev,
                        struct can_frame *frame,
                        u8 priority)
{
    struct wifi7_can_dev *can = can_dev;
    struct sk_buff *skb;
    unsigned long flags;

    if (!can || !frame)
        return -EINVAL;

    if (can->status.state != WIFI7_CAN_STATE_UP)
        return -ENETDOWN;

    /* Allocate skb for frame */
    skb = alloc_can_skb(dev->net_dev, &cf);
    if (!skb)
        return -ENOMEM;

    /* Copy frame data */
    memcpy(cf, frame, sizeof(*frame));

    spin_lock_irqsave(&can->queues.lock, flags);

    /* Check queue limits */
    if (skb_queue_len(&can->queues.tx_queue) >= can->config.queue.tx_queue_size) {
        spin_unlock_irqrestore(&can->queues.lock, flags);
        dev_kfree_skb(skb);
        return -ENOBUFS;
    }

    /* Queue frame */
    skb_queue_tail(&can->queues.tx_queue, skb);
    
    spin_unlock_irqrestore(&can->queues.lock, flags);

    /* Trigger transmission */
    schedule_delayed_work(&can->workers.tx_work, 0);

    return 0;
}

int wifi7_can_recv_frame(struct wifi7_dev *dev,
                        struct can_frame *frame)
{
    struct wifi7_can_dev *can = can_dev;
    struct sk_buff *skb;
    unsigned long flags;

    if (!can || !frame)
        return -EINVAL;

    spin_lock_irqsave(&can->queues.lock, flags);

    /* Get next received frame */
    skb = skb_dequeue(&can->queues.rx_queue);
    if (!skb) {
        spin_unlock_irqrestore(&can->queues.lock, flags);
        return -EAGAIN;
    }

    /* Copy frame data */
    memcpy(frame, (struct can_frame *)skb->data, sizeof(*frame));

    spin_unlock_irqrestore(&can->queues.lock, flags);
    dev_kfree_skb(skb);

    return 0;
}

int wifi7_can_get_state(struct wifi7_dev *dev)
{
    struct wifi7_can_dev *can = can_dev;
    unsigned long flags;
    int state;

    if (!can)
        return -EINVAL;

    spin_lock_irqsave(&can->lock, flags);
    state = can->status.state;
    spin_unlock_irqrestore(&can->lock, flags);

    return state;
}

int wifi7_can_get_stats(struct wifi7_dev *dev,
                       struct wifi7_can_stats *stats)
{
    struct wifi7_can_dev *can = can_dev;
    unsigned long flags;

    if (!can || !stats)
        return -EINVAL;

    spin_lock_irqsave(&can->lock, flags);
    memcpy(stats, &can->stats, sizeof(*stats));
    spin_unlock_irqrestore(&can->lock, flags);

    return 0;
}

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 CAN Bus Integration Module");
MODULE_VERSION("1.0"); 