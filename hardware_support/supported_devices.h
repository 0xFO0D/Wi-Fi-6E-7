#ifndef _MANAGH_SUPPORTED_DEVICES_H_
#define _MANAGH_SUPPORTED_DEVICES_H_

/* MediaTek devices */
#define MT_VENDOR_ID                0x0e8d
#define MT7921_PCI_DEVICE_ID       0x7961  /* MediaTek MT7921 WiFi 6E */
#define MT7922_PCI_DEVICE_ID       0x7922  /* MediaTek MT7922 WiFi 6E */
#define MT7925_USB_PRODUCT_ID      0x7925  /* MediaTek MT7925 USB WiFi 6E */

/* Realtek devices */
#define RTK_VENDOR_ID              0x10ec
#define RTL8852BE_DEVICE_ID        0x8852  /* Realtek RTL8852BE WiFi 6E */
#define RTL8852AE_DEVICE_ID        0x8852  /* Realtek RTL8852AE WiFi 6E */
#define RTL8852BU_PRODUCT_ID       0x885b  /* Realtek RTL8852BU USB WiFi 6E */

/* Intel devices */
#define INTEL_VENDOR_ID            0x8086
#define AX211_DEVICE_ID            0x2725  /* Intel AX211 WiFi 6E */
#define AX411_DEVICE_ID            0x2726  /* Intel AX411 WiFi 6E */
#define BE200_DEVICE_ID            0x2727  /* Intel BE200 WiFi 7 */

/* Qualcomm devices */
#define QCA_VENDOR_ID              0x168c
#define QCA6490_DEVICE_ID          0x6490  /* Qualcomm QCA6490 WiFi 6E */
#define QCA6750_DEVICE_ID          0x6750  /* Qualcomm QCA6750 WiFi 6E */
#define QCN9074_DEVICE_ID          0x9074  /* Qualcomm QCN9074 WiFi 7 */

/* Broadcom devices */
#define BCM_VENDOR_ID              0x14e4
#define BCM4389_DEVICE_ID          0x4389  /* Broadcom BCM4389 WiFi 6E */
#define BCM4398_DEVICE_ID          0x4398  /* Broadcom BCM4398 WiFi 7 */

/* Device capabilities flags */
#define DEVICE_CAP_WIFI_6E         BIT(0)
#define DEVICE_CAP_WIFI_7          BIT(1)
#define DEVICE_CAP_160MHZ          BIT(2)
#define DEVICE_CAP_320MHZ          BIT(3)
#define DEVICE_CAP_4x4_MIMO        BIT(4)
#define DEVICE_CAP_8x8_MIMO        BIT(5)
#define DEVICE_CAP_MU_MIMO         BIT(6)
#define DEVICE_CAP_OFDMA           BIT(7)
#define DEVICE_CAP_MLO             BIT(8)  /* Multi-Link Operation */

/* Device information structure */
struct managh_device_info {
    u16 vendor_id;
    u16 device_id;
    const char *name;
    u32 capabilities;
    u8 max_spatial_streams;
    u8 max_bandwidth_mhz;
    bool is_usb;
};

/* Supported devices table */
static const struct managh_device_info supported_devices[] = {
    /* MediaTek */
    {
        .vendor_id = MT_VENDOR_ID,
        .device_id = MT7921_PCI_DEVICE_ID,
        .name = "MediaTek MT7921",
        .capabilities = DEVICE_CAP_WIFI_6E | DEVICE_CAP_160MHZ | 
                       DEVICE_CAP_MU_MIMO | DEVICE_CAP_OFDMA,
        .max_spatial_streams = 2,
        .max_bandwidth_mhz = 160,
        .is_usb = false,
    },
    {
        .vendor_id = MT_VENDOR_ID,
        .device_id = MT7925_USB_PRODUCT_ID,
        .name = "MediaTek MT7925",
        .capabilities = DEVICE_CAP_WIFI_6E | DEVICE_CAP_160MHZ | 
                       DEVICE_CAP_MU_MIMO | DEVICE_CAP_OFDMA,
        .max_spatial_streams = 2,
        .max_bandwidth_mhz = 160,
        .is_usb = true,
    },
    
    /* Realtek */
    {
        .vendor_id = RTK_VENDOR_ID,
        .device_id = RTL8852BE_DEVICE_ID,
        .name = "Realtek RTL8852BE",
        .capabilities = DEVICE_CAP_WIFI_6E | DEVICE_CAP_160MHZ | 
                       DEVICE_CAP_MU_MIMO | DEVICE_CAP_OFDMA,
        .max_spatial_streams = 2,
        .max_bandwidth_mhz = 160,
        .is_usb = false,
    },
    {
        .vendor_id = RTK_VENDOR_ID,
        .device_id = RTL8852BU_PRODUCT_ID,
        .name = "Realtek RTL8852BU",
        .capabilities = DEVICE_CAP_WIFI_6E | DEVICE_CAP_160MHZ | 
                       DEVICE_CAP_MU_MIMO | DEVICE_CAP_OFDMA,
        .max_spatial_streams = 2,
        .max_bandwidth_mhz = 160,
        .is_usb = true,
    },

    /* Intel */
    {
        .vendor_id = INTEL_VENDOR_ID,
        .device_id = AX211_DEVICE_ID,
        .name = "Intel AX211",
        .capabilities = DEVICE_CAP_WIFI_6E | DEVICE_CAP_160MHZ | 
                       DEVICE_CAP_MU_MIMO | DEVICE_CAP_OFDMA,
        .max_spatial_streams = 2,
        .max_bandwidth_mhz = 160,
        .is_usb = false,
    },
    {
        .vendor_id = INTEL_VENDOR_ID,
        .device_id = BE200_DEVICE_ID,
        .name = "Intel BE200",
        .capabilities = DEVICE_CAP_WIFI_7 | DEVICE_CAP_320MHZ | 
                       DEVICE_CAP_MU_MIMO | DEVICE_CAP_OFDMA | 
                       DEVICE_CAP_MLO,
        .max_spatial_streams = 4,
        .max_bandwidth_mhz = 320,
        .is_usb = false,
    },

    /* Qualcomm */
    {
        .vendor_id = QCA_VENDOR_ID,
        .device_id = QCA6490_DEVICE_ID,
        .name = "Qualcomm QCA6490",
        .capabilities = DEVICE_CAP_WIFI_6E | DEVICE_CAP_160MHZ | 
                       DEVICE_CAP_MU_MIMO | DEVICE_CAP_OFDMA,
        .max_spatial_streams = 2,
        .max_bandwidth_mhz = 160,
        .is_usb = false,
    },
    {
        .vendor_id = QCA_VENDOR_ID,
        .device_id = QCN9074_DEVICE_ID,
        .name = "Qualcomm QCN9074",
        .capabilities = DEVICE_CAP_WIFI_7 | DEVICE_CAP_320MHZ | 
                       DEVICE_CAP_MU_MIMO | DEVICE_CAP_OFDMA | 
                       DEVICE_CAP_MLO,
        .max_spatial_streams = 4,
        .max_bandwidth_mhz = 320,
        .is_usb = false,
    },

    /* Broadcom */
    {
        .vendor_id = BCM_VENDOR_ID,
        .device_id = BCM4389_DEVICE_ID,
        .name = "Broadcom BCM4389",
        .capabilities = DEVICE_CAP_WIFI_6E | DEVICE_CAP_160MHZ | 
                       DEVICE_CAP_MU_MIMO | DEVICE_CAP_OFDMA,
        .max_spatial_streams = 2,
        .max_bandwidth_mhz = 160,
        .is_usb = false,
    },
    {
        .vendor_id = BCM_VENDOR_ID,
        .device_id = BCM4398_DEVICE_ID,
        .name = "Broadcom BCM4398",
        .capabilities = DEVICE_CAP_WIFI_7 | DEVICE_CAP_320MHZ | 
                       DEVICE_CAP_MU_MIMO | DEVICE_CAP_OFDMA | 
                       DEVICE_CAP_MLO,
        .max_spatial_streams = 4,
        .max_bandwidth_mhz = 320,
        .is_usb = false,
    },
};

#endif /* _MANAGH_SUPPORTED_DEVICES_H_ */ 