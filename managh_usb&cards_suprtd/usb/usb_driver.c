/*
 * WiFi 7 USB Device Driver
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/usb.h>
#include <linux/firmware.h>
#include <linux/etherdevice.h>
#include "usb_driver.h"
#include "../firmware/firmware_loader.h"

/* Supported device table */
static const struct usb_device_id wifi7_usb_ids[] = {
    /* TP-Link */
    { USB_DEVICE(0x2357, 0x0001) }, /* TP-Link AXE300 */
    { USB_DEVICE(0x2357, 0x0002) }, /* TP-Link AXE75 */
    { USB_DEVICE(0x2357, 0x0003) }, /* TP-Link BE800 */
    { USB_DEVICE(0x2357, 0x0004) }, /* TP-Link BE900 */
    
    /* MediaTek */
    { USB_DEVICE(0x0e8d, 0x0001) }, /* MediaTek MT7921 */
    { USB_DEVICE(0x0e8d, 0x0002) }, /* MediaTek MT7922 */
    
    /* Realtek */
    { USB_DEVICE(0x0bda, 0x0001) }, /* Realtek RTL8852BE */
    { USB_DEVICE(0x0bda, 0x0002) }, /* Realtek RTL8852CE */
    
    { }
};
MODULE_DEVICE_TABLE(usb, wifi7_usb_ids);

/* USB device context */
struct wifi7_usb_dev {
    struct usb_device *udev;
    struct usb_interface *intf;
    struct wifi7_dev *dev;
    
    /* Endpoints */
    u8 bulk_in_pipe;
    u8 bulk_out_pipe;
    u8 intr_pipe;
    
    /* URBs */
    struct urb *bulk_rx_urb;
    struct urb *intr_urb;
    
    /* Buffers */
    void *bulk_rx_buf;
    void *intr_buf;
    
    /* Work items */
    struct work_struct rx_work;
    struct delayed_work stat_work;
    
    /* Device state */
    bool initialized;
    bool suspended;
    spinlock_t lock;
    
    /* Statistics */
    struct {
        u32 rx_packets;
        u32 tx_packets;
        u32 rx_errors;
        u32 tx_errors;
        u32 rx_dropped;
        u32 tx_dropped;
    } stats;
};

/* Forward declarations */
static int wifi7_usb_probe(struct usb_interface *intf,
                          const struct usb_device_id *id);
static void wifi7_usb_disconnect(struct usb_interface *intf);
static int wifi7_usb_suspend(struct usb_interface *intf, pm_message_t message);
static int wifi7_usb_resume(struct usb_interface *intf);
static int wifi7_usb_reset_resume(struct usb_interface *intf);

/* USB driver structure */
static struct usb_driver wifi7_usb_driver = {
    .name = "wifi7_usb",
    .id_table = wifi7_usb_ids,
    .probe = wifi7_usb_probe,
    .disconnect = wifi7_usb_disconnect,
    .suspend = wifi7_usb_suspend,
    .resume = wifi7_usb_resume,
    .reset_resume = wifi7_usb_reset_resume,
    .supports_autosuspend = 1,
};

/* Receive work handler */
static void wifi7_usb_rx_work(struct work_struct *work)
{
    struct wifi7_usb_dev *usb_dev = container_of(work, struct wifi7_usb_dev,
                                                rx_work);
    struct urb *urb = usb_dev->bulk_rx_urb;
    int ret;

    /* Resubmit URB */
    usb_fill_bulk_urb(urb, usb_dev->udev, usb_dev->bulk_in_pipe,
                      usb_dev->bulk_rx_buf, USB_MAX_BULK_SIZE,
                      wifi7_usb_rx_complete, usb_dev);
                      
    ret = usb_submit_urb(urb, GFP_KERNEL);
    if (ret)
        dev_err(&usb_dev->udev->dev, "Failed to resubmit RX URB: %d\n", ret);
}

/* Statistics work handler */
static void wifi7_usb_stat_work(struct work_struct *work)
{
    struct wifi7_usb_dev *usb_dev = container_of(work, struct wifi7_usb_dev,
                                                stat_work.work);
    unsigned long flags;

    spin_lock_irqsave(&usb_dev->lock, flags);
    /* Update statistics */
    spin_unlock_irqrestore(&usb_dev->lock, flags);

    /* Reschedule */
    schedule_delayed_work(&usb_dev->stat_work, HZ);
}

/* URB completion handlers */
static void wifi7_usb_rx_complete(struct urb *urb)
{
    struct wifi7_usb_dev *usb_dev = urb->context;
    struct sk_buff *skb;
    int ret;

    switch (urb->status) {
    case 0:
        /* Process received data */
        skb = dev_alloc_skb(urb->actual_length);
        if (skb) {
            memcpy(skb_put(skb, urb->actual_length),
                   urb->transfer_buffer, urb->actual_length);
            /* Pass to upper layer */
            wifi7_rx_packet(usb_dev->dev, skb);
            usb_dev->stats.rx_packets++;
        } else {
            usb_dev->stats.rx_dropped++;
        }
        break;

    case -ENOENT:
    case -ECONNRESET:
    case -ESHUTDOWN:
        return;

    default:
        dev_err(&usb_dev->udev->dev, "RX URB failed: %d\n", urb->status);
        usb_dev->stats.rx_errors++;
        break;
    }

    /* Resubmit URB via work queue */
    schedule_work(&usb_dev->rx_work);
}

static void wifi7_usb_intr_complete(struct urb *urb)
{
    struct wifi7_usb_dev *usb_dev = urb->context;
    int ret;

    switch (urb->status) {
    case 0:
        /* Process interrupt data */
        break;

    case -ENOENT:
    case -ECONNRESET:
    case -ESHUTDOWN:
        return;

    default:
        dev_err(&usb_dev->udev->dev, "INT URB failed: %d\n", urb->status);
        break;
    }

    /* Resubmit URB */
    ret = usb_submit_urb(urb, GFP_ATOMIC);
    if (ret)
        dev_err(&usb_dev->udev->dev, "Failed to resubmit INT URB: %d\n", ret);
}

/* Device initialization */
static int wifi7_usb_init_device(struct wifi7_usb_dev *usb_dev)
{
    struct usb_device *udev = usb_dev->udev;
    struct usb_host_interface *iface_desc;
    struct usb_endpoint_descriptor *endpoint;
    int i, ret;

    /* Get interface descriptor */
    iface_desc = usb_dev->intf->cur_altsetting;

    /* Find endpoints */
    for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
        endpoint = &iface_desc->endpoint[i].desc;

        if (usb_endpoint_is_bulk_in(endpoint))
            usb_dev->bulk_in_pipe = endpoint->bEndpointAddress;
        else if (usb_endpoint_is_bulk_out(endpoint))
            usb_dev->bulk_out_pipe = endpoint->bEndpointAddress;
        else if (usb_endpoint_is_int_in(endpoint))
            usb_dev->intr_pipe = endpoint->bEndpointAddress;
    }

    /* Allocate URBs */
    usb_dev->bulk_rx_urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!usb_dev->bulk_rx_urb)
        return -ENOMEM;

    usb_dev->intr_urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!usb_dev->intr_urb) {
        ret = -ENOMEM;
        goto err_free_rx;
    }

    /* Allocate buffers */
    usb_dev->bulk_rx_buf = usb_alloc_coherent(udev, USB_MAX_BULK_SIZE,
                                             GFP_KERNEL,
                                             &usb_dev->bulk_rx_urb->transfer_dma);
    if (!usb_dev->bulk_rx_buf) {
        ret = -ENOMEM;
        goto err_free_intr;
    }

    usb_dev->intr_buf = usb_alloc_coherent(udev, USB_MAX_INTR_SIZE,
                                          GFP_KERNEL,
                                          &usb_dev->intr_urb->transfer_dma);
    if (!usb_dev->intr_buf) {
        ret = -ENOMEM;
        goto err_free_rx_buf;
    }

    /* Initialize work items */
    INIT_WORK(&usb_dev->rx_work, wifi7_usb_rx_work);
    INIT_DELAYED_WORK(&usb_dev->stat_work, wifi7_usb_stat_work);

    /* Setup URBs */
    usb_fill_bulk_urb(usb_dev->bulk_rx_urb, udev, usb_dev->bulk_in_pipe,
                      usb_dev->bulk_rx_buf, USB_MAX_BULK_SIZE,
                      wifi7_usb_rx_complete, usb_dev);
    usb_dev->bulk_rx_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    usb_fill_int_urb(usb_dev->intr_urb, udev, usb_dev->intr_pipe,
                     usb_dev->intr_buf, USB_MAX_INTR_SIZE,
                     wifi7_usb_intr_complete, usb_dev, 1);
    usb_dev->intr_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

    /* Submit URBs */
    ret = usb_submit_urb(usb_dev->bulk_rx_urb, GFP_KERNEL);
    if (ret)
        goto err_free_intr_buf;

    ret = usb_submit_urb(usb_dev->intr_urb, GFP_KERNEL);
    if (ret)
        goto err_kill_rx;

    /* Start statistics collection */
    schedule_delayed_work(&usb_dev->stat_work, HZ);

    usb_dev->initialized = true;
    return 0;

err_kill_rx:
    usb_kill_urb(usb_dev->bulk_rx_urb);
err_free_intr_buf:
    usb_free_coherent(udev, USB_MAX_INTR_SIZE, usb_dev->intr_buf,
                      usb_dev->intr_urb->transfer_dma);
err_free_rx_buf:
    usb_free_coherent(udev, USB_MAX_BULK_SIZE, usb_dev->bulk_rx_buf,
                      usb_dev->bulk_rx_urb->transfer_dma);
err_free_intr:
    usb_free_urb(usb_dev->intr_urb);
err_free_rx:
    usb_free_urb(usb_dev->bulk_rx_urb);
    return ret;
}

/* Device cleanup */
static void wifi7_usb_deinit_device(struct wifi7_usb_dev *usb_dev)
{
    if (!usb_dev->initialized)
        return;

    /* Cancel work items */
    cancel_work_sync(&usb_dev->rx_work);
    cancel_delayed_work_sync(&usb_dev->stat_work);

    /* Kill URBs */
    usb_kill_urb(usb_dev->bulk_rx_urb);
    usb_kill_urb(usb_dev->intr_urb);

    /* Free buffers */
    usb_free_coherent(usb_dev->udev, USB_MAX_BULK_SIZE,
                      usb_dev->bulk_rx_buf,
                      usb_dev->bulk_rx_urb->transfer_dma);
    usb_free_coherent(usb_dev->udev, USB_MAX_INTR_SIZE,
                      usb_dev->intr_buf,
                      usb_dev->intr_urb->transfer_dma);

    /* Free URBs */
    usb_free_urb(usb_dev->bulk_rx_urb);
    usb_free_urb(usb_dev->intr_urb);

    usb_dev->initialized = false;
}

/* USB driver callbacks */
static int wifi7_usb_probe(struct usb_interface *intf,
                          const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(intf);
    struct wifi7_usb_dev *usb_dev;
    int ret;

    /* Allocate device context */
    usb_dev = kzalloc(sizeof(*usb_dev), GFP_KERNEL);
    if (!usb_dev)
        return -ENOMEM;

    /* Initialize context */
    usb_dev->udev = udev;
    usb_dev->intf = intf;
    spin_lock_init(&usb_dev->lock);

    /* Set interface data */
    usb_set_intfdata(intf, usb_dev);

    /* Initialize device */
    ret = wifi7_usb_init_device(usb_dev);
    if (ret)
        goto err_free;

    /* Load firmware */
    ret = wifi7_load_firmware(usb_dev);
    if (ret)
        goto err_deinit;

    dev_info(&udev->dev, "WiFi 7 USB device initialized\n");
    return 0;

err_deinit:
    wifi7_usb_deinit_device(usb_dev);
err_free:
    kfree(usb_dev);
    return ret;
}

static void wifi7_usb_disconnect(struct usb_interface *intf)
{
    struct wifi7_usb_dev *usb_dev = usb_get_intfdata(intf);

    if (!usb_dev)
        return;

    /* Clean up device */
    wifi7_usb_deinit_device(usb_dev);

    /* Free device context */
    kfree(usb_dev);

    dev_info(&intf->dev, "WiFi 7 USB device removed\n");
}

static int wifi7_usb_suspend(struct usb_interface *intf, pm_message_t message)
{
    struct wifi7_usb_dev *usb_dev = usb_get_intfdata(intf);

    if (!usb_dev)
        return 0;

    /* Stop device operation */
    wifi7_usb_deinit_device(usb_dev);
    usb_dev->suspended = true;

    return 0;
}

static int wifi7_usb_resume(struct usb_interface *intf)
{
    struct wifi7_usb_dev *usb_dev = usb_get_intfdata(intf);
    int ret;

    if (!usb_dev)
        return 0;

    /* Resume device operation */
    ret = wifi7_usb_init_device(usb_dev);
    if (ret == 0)
        usb_dev->suspended = false;

    return ret;
}

static int wifi7_usb_reset_resume(struct usb_interface *intf)
{
    return wifi7_usb_resume(intf);
}

/* Module initialization */
static int __init wifi7_usb_init(void)
{
    int ret;

    ret = usb_register(&wifi7_usb_driver);
    if (ret)
        pr_err("Failed to register USB driver: %d\n", ret);
    else
        pr_info("WiFi 7 USB driver loaded\n");

    return ret;
}

static void __exit wifi7_usb_exit(void)
{
    usb_deregister(&wifi7_usb_driver);
    pr_info("WiFi 7 USB driver unloaded\n");
}

module_init(wifi7_usb_init);
module_exit(wifi7_usb_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 USB Device Driver");
MODULE_VERSION("1.0"); 