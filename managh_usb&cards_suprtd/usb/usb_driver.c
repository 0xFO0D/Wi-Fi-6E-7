#include <linux/module.h>
#include <linux/usb.h>
#include <linux/firmware.h>
#include "../supported_devices.h"
#include "../../include/core/wifi67.h"

/* USB device private structure */
struct managh_usb_dev {
    struct usb_device *udev;
    struct usb_interface *intf;
    struct wifi67_priv *wifi_priv;
    
    u8 *bulk_in_buffer;
    size_t bulk_in_size;
    struct urb *bulk_in_urb;
    
    u8 *bulk_out_buffer;
    size_t bulk_out_size;
    struct urb *bulk_out_urb;
    
    u8 bulk_in_endpointAddr;
    u8 bulk_out_endpointAddr;
    
    bool initialized;
    bool suspended;
    
    spinlock_t lock;
    
    /* Device-specific information */
    const struct managh_device_info *dev_info;
};

static int managh_usb_probe(struct usb_interface *interface,
                          const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct managh_usb_dev *usb_dev;
    struct wifi67_priv *wifi_priv;
    const struct managh_device_info *dev_info = NULL;
    int i, ret;

    /* Find device info */
    for (i = 0; i < ARRAY_SIZE(supported_devices); i++) {
        if (supported_devices[i].vendor_id == le16_to_cpu(udev->descriptor.idVendor) &&
            supported_devices[i].device_id == le16_to_cpu(udev->descriptor.idProduct) &&
            supported_devices[i].is_usb) {
            dev_info = &supported_devices[i];
            break;
        }
    }

    if (!dev_info) {
        pr_err("Unsupported USB device %04x:%04x\n",
               le16_to_cpu(udev->descriptor.idVendor),
               le16_to_cpu(udev->descriptor.idProduct));
        return -ENODEV;
    }

    /* Allocate USB device structure */
    usb_dev = kzalloc(sizeof(*usb_dev), GFP_KERNEL);
    if (!usb_dev)
        return -ENOMEM;

    /* Initialize USB device */
    usb_dev->udev = usb_get_dev(udev);
    usb_dev->intf = interface;
    usb_dev->dev_info = dev_info;
    spin_lock_init(&usb_dev->lock);

    /* Find endpoints */
    ret = managh_usb_find_endpoints(interface, usb_dev);
    if (ret)
        goto err_put_dev;

    /* Allocate URBs */
    ret = managh_usb_alloc_urbs(usb_dev);
    if (ret)
        goto err_put_dev;

    /* Initialize WiFi core */
    wifi_priv = wifi67_core_alloc();
    if (!wifi_priv) {
        ret = -ENOMEM;
        goto err_free_urbs;
    }

    usb_dev->wifi_priv = wifi_priv;
    wifi_priv->dev = &interface->dev;

    /* Set device capabilities */
    wifi_priv->features.has_6ghz = !!(dev_info->capabilities & DEVICE_CAP_WIFI_6E);
    wifi_priv->features.has_7ghz = !!(dev_info->capabilities & DEVICE_CAP_WIFI_7);
    wifi_priv->features.max_spatial_streams = dev_info->max_spatial_streams;
    wifi_priv->features.max_bandwidth = dev_info->max_bandwidth_mhz;

    /* Initialize WiFi core */
    ret = wifi67_core_init(wifi_priv);
    if (ret)
        goto err_free_wifi;

    /* Set interface data */
    usb_set_intfdata(interface, usb_dev);
    usb_dev->initialized = true;

    pr_info("Initialized %s USB WiFi device\n", dev_info->name);
    return 0;

err_free_wifi:
    wifi67_core_free(wifi_priv);
err_free_urbs:
    managh_usb_free_urbs(usb_dev);
err_put_dev:
    usb_put_dev(udev);
    kfree(usb_dev);
    return ret;
}

static void managh_usb_disconnect(struct usb_interface *interface)
{
    struct managh_usb_dev *usb_dev = usb_get_intfdata(interface);

    if (!usb_dev)
        return;

    /* Stop all URBs */
    managh_usb_stop_urbs(usb_dev);

    /* Cleanup WiFi core */
    if (usb_dev->wifi_priv) {
        wifi67_core_deinit(usb_dev->wifi_priv);
        wifi67_core_free(usb_dev->wifi_priv);
    }

    /* Free URBs */
    managh_usb_free_urbs(usb_dev);

    /* Cleanup USB device */
    usb_set_intfdata(interface, NULL);
    usb_put_dev(usb_dev->udev);
    kfree(usb_dev);

    pr_info("USB WiFi device disconnected\n");
}

static int managh_usb_suspend(struct usb_interface *intf, pm_message_t message)
{
    struct managh_usb_dev *usb_dev = usb_get_intfdata(intf);

    if (!usb_dev)
        return 0;

    managh_usb_stop_urbs(usb_dev);
    wifi67_core_suspend(usb_dev->wifi_priv);
    usb_dev->suspended = true;

    return 0;
}

static int managh_usb_resume(struct usb_interface *intf)
{
    struct managh_usb_dev *usb_dev = usb_get_intfdata(intf);
    int ret;

    if (!usb_dev)
        return 0;

    ret = wifi67_core_resume(usb_dev->wifi_priv);
    if (ret)
        return ret;

    ret = managh_usb_start_urbs(usb_dev);
    if (ret)
        return ret;

    usb_dev->suspended = false;
    return 0;
}

/* USB device ID table */
static const struct usb_device_id managh_usb_ids[] = {
    /* MediaTek */
    { USB_DEVICE(MT_VENDOR_ID, MT7925_USB_PRODUCT_ID) },
    
    /* Realtek */
    { USB_DEVICE(RTK_VENDOR_ID, RTL8852BU_PRODUCT_ID) },
    
    { }
};
MODULE_DEVICE_TABLE(usb, managh_usb_ids);

/* USB driver structure */
static struct usb_driver managh_usb_driver = {
    .name = "managh_wifi_usb",
    .id_table = managh_usb_ids,
    .probe = managh_usb_probe,
    .disconnect = managh_usb_disconnect,
    .suspend = managh_usb_suspend,
    .resume = managh_usb_resume,
    .reset_resume = managh_usb_resume,
    .supports_autosuspend = 1,
};

module_usb_driver(managh_usb_driver);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Managh WiFi USB Driver");
MODULE_LICENSE("GPL"); 