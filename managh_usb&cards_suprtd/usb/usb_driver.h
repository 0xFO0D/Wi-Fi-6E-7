/*
 * WiFi 7 USB Device Driver
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_USB_DRIVER_H
#define __WIFI7_USB_DRIVER_H

#include <linux/types.h>
#include <linux/usb.h>
#include "../../core/wifi7_core.h"

/* USB transfer sizes */
#define USB_MAX_BULK_SIZE    (64 * 1024)  /* 64KB */
#define USB_MAX_INTR_SIZE    64           /* 64 bytes */

/* USB timeout values (in milliseconds) */
#define USB_CTRL_TIMEOUT     1000
#define USB_BULK_TIMEOUT     2000
#define USB_INTR_TIMEOUT     1000

/* USB endpoint types */
#define USB_EP_BULK_IN       0x81
#define USB_EP_BULK_OUT      0x02
#define USB_EP_INTR_IN       0x83

/* USB device capabilities */
#define USB_CAP_BULK_SG      BIT(0)  /* Scatter-gather bulk */
#define USB_CAP_ISOC         BIT(1)  /* Isochronous transfers */
#define USB_CAP_INTR         BIT(2)  /* Interrupt transfers */
#define USB_CAP_REMOTE_WAKE  BIT(3)  /* Remote wakeup */
#define USB_CAP_LPM          BIT(4)  /* Link power management */
#define USB_CAP_SS           BIT(5)  /* SuperSpeed */
#define USB_CAP_SSP          BIT(6)  /* SuperSpeedPlus */

/* USB device flags */
#define USB_FLAG_BULK_SG     BIT(0)  /* Use scatter-gather */
#define USB_FLAG_ISOC        BIT(1)  /* Use isochronous */
#define USB_FLAG_INTR        BIT(2)  /* Use interrupt */
#define USB_FLAG_NO_LPM      BIT(3)  /* Disable LPM */
#define USB_FLAG_NO_SUSPEND  BIT(4)  /* Disable suspend */
#define USB_FLAG_RESET       BIT(5)  /* Reset device */

/* USB device statistics */
struct wifi7_usb_stats {
    u32 rx_packets;          /* Received packets */
    u32 tx_packets;          /* Transmitted packets */
    u32 rx_bytes;            /* Received bytes */
    u32 tx_bytes;            /* Transmitted bytes */
    u32 rx_errors;           /* Receive errors */
    u32 tx_errors;           /* Transmit errors */
    u32 rx_dropped;          /* Dropped RX packets */
    u32 tx_dropped;          /* Dropped TX packets */
    u32 rx_overruns;         /* RX overruns */
    u32 tx_overruns;         /* TX overruns */
    u32 rx_fifo_errors;      /* RX FIFO errors */
    u32 tx_fifo_errors;      /* TX FIFO errors */
    u32 rx_crc_errors;       /* RX CRC errors */
    u32 tx_heartbeat_errors; /* TX heartbeat errors */
    u32 rx_frame_errors;     /* RX frame errors */
    u32 tx_window_errors;    /* TX window errors */
    u32 rx_missed_errors;    /* RX missed errors */
    u32 tx_aborted_errors;   /* TX aborted errors */
    u32 rx_length_errors;    /* RX length errors */
    u32 tx_carrier_errors;   /* TX carrier errors */
    u32 collisions;          /* Collisions */
    u32 tx_timeouts;         /* TX timeouts */
    u32 resets;              /* Device resets */
    u32 recovery_complete;   /* Recovery complete */
};

/* USB device configuration */
struct wifi7_usb_config {
    u32 capabilities;        /* Device capabilities */
    u32 flags;              /* Device flags */
    u16 vendor_id;          /* Vendor ID */
    u16 product_id;         /* Product ID */
    u16 bulk_in_size;       /* Bulk IN size */
    u16 bulk_out_size;      /* Bulk OUT size */
    u8 bulk_in_ep;          /* Bulk IN endpoint */
    u8 bulk_out_ep;         /* Bulk OUT endpoint */
    u8 intr_ep;             /* Interrupt endpoint */
    u8 num_rx_urbs;         /* Number of RX URBs */
    u8 num_tx_urbs;         /* Number of TX URBs */
    u8 rx_urb_size;         /* RX URB size */
    u8 tx_urb_size;         /* TX URB size */
    bool use_dma;           /* Use DMA */
    bool use_sg;            /* Use scatter-gather */
};

/* Function prototypes */
int wifi7_usb_init(struct wifi7_dev *dev);
void wifi7_usb_deinit(struct wifi7_dev *dev);

int wifi7_usb_start(struct wifi7_dev *dev);
void wifi7_usb_stop(struct wifi7_dev *dev);

int wifi7_usb_tx(struct wifi7_dev *dev, struct sk_buff *skb);
void wifi7_usb_rx(struct wifi7_dev *dev, struct sk_buff *skb);

int wifi7_usb_set_config(struct wifi7_dev *dev,
                        struct wifi7_usb_config *config);
int wifi7_usb_get_config(struct wifi7_dev *dev,
                        struct wifi7_usb_config *config);

int wifi7_usb_get_stats(struct wifi7_dev *dev,
                       struct wifi7_usb_stats *stats);
int wifi7_usb_clear_stats(struct wifi7_dev *dev);

int wifi7_usb_reset(struct wifi7_dev *dev);
int wifi7_usb_suspend(struct wifi7_dev *dev);
int wifi7_usb_resume(struct wifi7_dev *dev);

/* Debug interface */
#ifdef CONFIG_WIFI7_USB_DEBUG
int wifi7_usb_debugfs_init(struct wifi7_dev *dev);
void wifi7_usb_debugfs_remove(struct wifi7_dev *dev);
#else
static inline int wifi7_usb_debugfs_init(struct wifi7_dev *dev) { return 0; }
static inline void wifi7_usb_debugfs_remove(struct wifi7_dev *dev) {}
#endif

#endif /* __WIFI7_USB_DRIVER_H */ 