/*
 * WiFi 7 TP-Link Router Support
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/etherdevice.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include "wifi7_tplink.h"
#include "../../core/wifi7_core.h"
#include "../../hal/wifi7_rf.h"
#include "../../mac/wifi7_mac.h"
#include "../../phy/wifi7_phy.h"

/* TP-Link specific registers */
#define TPLINK_REG_CHIP_ID       0x0000
#define TPLINK_REG_HW_VERSION    0x0004
#define TPLINK_REG_FW_VERSION    0x0008
#define TPLINK_REG_CONTROL       0x000C
#define TPLINK_REG_STATUS        0x0010
#define TPLINK_REG_INT_STATUS    0x0014
#define TPLINK_REG_INT_MASK      0x0018
#define TPLINK_REG_GPIO_CONTROL  0x001C
#define TPLINK_REG_POWER_CTRL    0x0020
#define TPLINK_REG_RESET         0x0024
#define TPLINK_REG_CLOCK_CTRL    0x0028
#define TPLINK_REG_PLL_CONFIG    0x002C
#define TPLINK_REG_THERMAL       0x0030
#define TPLINK_REG_LED_CTRL      0x0034

/* Control register bits */
#define TPLINK_CTRL_POWER_ON     BIT(0)
#define TPLINK_CTRL_RESET        BIT(1)
#define TPLINK_CTRL_INT_ENABLE   BIT(2)
#define TPLINK_CTRL_LED_ENABLE   BIT(3)
#define TPLINK_CTRL_AFC_ENABLE   BIT(4)
#define TPLINK_CTRL_MESH_ENABLE  BIT(5)
#define TPLINK_CTRL_GAMING_MODE  BIT(6)
#define TPLINK_CTRL_AI_OPT       BIT(7)

/* Status register bits */
#define TPLINK_STATUS_READY      BIT(0)
#define TPLINK_STATUS_ERROR      BIT(1)
#define TPLINK_STATUS_OVERHEAT   BIT(2)
#define TPLINK_STATUS_CALIBRATED BIT(3)
#define TPLINK_STATUS_AFC_READY  BIT(4)
#define TPLINK_STATUS_MESH_READY BIT(5)
#define TPLINK_STATUS_AI_READY   BIT(6)

/* Interrupt bits */
#define TPLINK_INT_RX_DONE      BIT(0)
#define TPLINK_INT_TX_DONE      BIT(1)
#define TPLINK_INT_ERROR        BIT(2)
#define TPLINK_INT_TEMP_HIGH    BIT(3)
#define TPLINK_INT_RADAR        BIT(4)
#define TPLINK_INT_AFC_EVENT    BIT(5)
#define TPLINK_INT_MESH_EVENT   BIT(6)
#define TPLINK_INT_AI_EVENT     BIT(7)

/* Device state */
struct wifi7_tplink_dev {
    struct wifi7_dev *dev;           /* Core device structure */
    struct wifi7_tplink_config config; /* Router configuration */
    struct wifi7_tplink_stats stats;  /* Router statistics */
    void __iomem *mmio;              /* Memory-mapped I/O */
    struct dentry *debugfs_dir;       /* debugfs directory */
    spinlock_t lock;                 /* Device lock */
    bool initialized;                /* Initialization flag */
    struct {
        u32 chip_id;                /* Chip ID */
        u32 hw_version;            /* Hardware version */
        u32 fw_version;            /* Firmware version */
        u32 capabilities;          /* Hardware capabilities */
    } hw_info;
    struct {
        bool afc_enabled;          /* AFC enabled flag */
        bool mesh_enabled;         /* Mesh enabled flag */
        bool gaming_enabled;       /* Gaming mode flag */
        bool ai_enabled;           /* AI optimization flag */
    } features;
    struct {
        struct delayed_work stats_work;  /* Statistics collection */
        struct delayed_work temp_work;   /* Temperature monitoring */
        struct delayed_work calib_work;  /* Calibration work */
    } workers;
};

/* Global device context */
static struct wifi7_tplink_dev *tplink_dev;

/* Helper functions */
static inline u32 tplink_read32(struct wifi7_tplink_dev *dev, u32 reg)
{
    return readl(dev->mmio + reg);
}

static inline void tplink_write32(struct wifi7_tplink_dev *dev, u32 reg, u32 val)
{
    writel(val, dev->mmio + reg);
}

static inline void tplink_set_bits32(struct wifi7_tplink_dev *dev, u32 reg,
                                   u32 bits)
{
    u32 val = tplink_read32(dev, reg);
    val |= bits;
    tplink_write32(dev, reg, val);
}

static inline void tplink_clear_bits32(struct wifi7_tplink_dev *dev, u32 reg,
                                     u32 bits)
{
    u32 val = tplink_read32(dev, reg);
    val &= ~bits;
    tplink_write32(dev, reg, val);
}

/* Temperature monitoring work */
static void tplink_temp_work_handler(struct work_struct *work)
{
    struct wifi7_tplink_dev *dev = tplink_dev;
    u32 temp;
    unsigned long flags;

    if (!dev->initialized)
        return;

    spin_lock_irqsave(&dev->lock, flags);

    /* Read temperature */
    temp = tplink_read32(dev, TPLINK_REG_THERMAL);

    /* Update statistics */
    dev->stats.radio_stats[0].temperature = temp;

    /* Check for overheating */
    if (temp > 85) {
        tplink_set_bits32(dev, TPLINK_REG_STATUS, TPLINK_STATUS_OVERHEAT);
        dev_warn(dev->dev->dev, "Device temperature too high: %u°C\n", temp);
    }

    spin_unlock_irqrestore(&dev->lock, flags);

    /* Schedule next check */
    schedule_delayed_work(&dev->workers.temp_work, HZ);
}

/* Statistics collection work */
static void tplink_stats_work_handler(struct work_struct *work)
{
    struct wifi7_tplink_dev *dev = tplink_dev;
    unsigned long flags;
    int i;

    if (!dev->initialized)
        return;

    spin_lock_irqsave(&dev->lock, flags);

    /* Update radio statistics */
    for (i = 0; i < dev->config.num_radios; i++) {
        /* TODO: Read actual values from hardware */
        dev->stats.radio_stats[i].tx_power = 20;
        dev->stats.radio_stats[i].phy_errors += 0;
        dev->stats.radio_stats[i].crc_errors += 0;
        dev->stats.radio_stats[i].retry_count += 0;
    }

    /* Update general statistics */
    dev->stats.channel_utilization = 50; /* Example value */
    dev->stats.noise_floor = -95;       /* Example value */
    dev->stats.interference = 10;       /* Example value */

    spin_unlock_irqrestore(&dev->lock, flags);

    /* Schedule next collection */
    schedule_delayed_work(&dev->workers.stats_work, HZ);
}

/* Calibration work */
static void tplink_calib_work_handler(struct work_struct *work)
{
    struct wifi7_tplink_dev *dev = tplink_dev;
    unsigned long flags;
    int ret;

    if (!dev->initialized)
        return;

    spin_lock_irqsave(&dev->lock, flags);

    /* Perform calibration */
    ret = wifi7_rf_calibrate(dev->dev);
    if (ret == 0) {
        tplink_set_bits32(dev, TPLINK_REG_STATUS, TPLINK_STATUS_CALIBRATED);
        dev_info(dev->dev->dev, "Calibration completed successfully\n");
    } else {
        dev_err(dev->dev->dev, "Calibration failed: %d\n", ret);
    }

    spin_unlock_irqrestore(&dev->lock, flags);
}

/* Interrupt handler */
static irqreturn_t tplink_interrupt(int irq, void *data)
{
    struct wifi7_tplink_dev *dev = data;
    u32 status, mask;

    /* Read interrupt status and mask */
    status = tplink_read32(dev, TPLINK_REG_INT_STATUS);
    mask = tplink_read32(dev, TPLINK_REG_INT_MASK);
    status &= mask;

    if (!status)
        return IRQ_NONE;

    /* Handle interrupts */
    if (status & TPLINK_INT_RX_DONE) {
        /* Handle RX completion */
    }

    if (status & TPLINK_INT_TX_DONE) {
        /* Handle TX completion */
    }

    if (status & TPLINK_INT_ERROR) {
        /* Handle error condition */
        dev_err(dev->dev->dev, "Hardware error detected\n");
    }

    if (status & TPLINK_INT_TEMP_HIGH) {
        /* Handle high temperature */
        dev_warn(dev->dev->dev, "High temperature warning\n");
    }

    if (status & TPLINK_INT_RADAR) {
        /* Handle radar detection */
        dev_info(dev->dev->dev, "Radar detected\n");
    }

    if (status & TPLINK_INT_AFC_EVENT) {
        /* Handle AFC event */
    }

    if (status & TPLINK_INT_MESH_EVENT) {
        /* Handle mesh event */
    }

    if (status & TPLINK_INT_AI_EVENT) {
        /* Handle AI event */
    }

    /* Clear handled interrupts */
    tplink_write32(dev, TPLINK_REG_INT_STATUS, status);

    return IRQ_HANDLED;
}

/* Device initialization */
int wifi7_tplink_init(struct wifi7_dev *dev)
{
    struct wifi7_tplink_dev *tdev;
    int ret;

    /* Allocate device context */
    tdev = kzalloc(sizeof(*tdev), GFP_KERNEL);
    if (!tdev)
        return -ENOMEM;

    tdev->dev = dev;
    spin_lock_init(&tdev->lock);
    tplink_dev = tdev;

    /* Initialize work queues */
    INIT_DELAYED_WORK(&tdev->workers.stats_work, tplink_stats_work_handler);
    INIT_DELAYED_WORK(&tdev->workers.temp_work, tplink_temp_work_handler);
    INIT_DELAYED_WORK(&tdev->workers.calib_work, tplink_calib_work_handler);

    /* Map device memory */
    tdev->mmio = ioremap(pci_resource_start(dev->pdev, 0),
                        pci_resource_len(dev->pdev, 0));
    if (!tdev->mmio) {
        ret = -ENOMEM;
        goto err_free;
    }

    /* Read hardware information */
    tdev->hw_info.chip_id = tplink_read32(tdev, TPLINK_REG_CHIP_ID);
    tdev->hw_info.hw_version = tplink_read32(tdev, TPLINK_REG_HW_VERSION);
    tdev->hw_info.fw_version = tplink_read32(tdev, TPLINK_REG_FW_VERSION);

    /* Set default configuration */
    tdev->config.model = TPLINK_MODEL_BE900;
    tdev->config.num_radios = 4;
    tdev->config.max_spatial_streams = 16;
    tdev->config.max_bandwidth = 320;
    tdev->config.capabilities = TPLINK_CAP_320MHZ | TPLINK_CAP_4K_QAM |
                               TPLINK_CAP_16_SS | TPLINK_CAP_MLO |
                               TPLINK_CAP_EHT_MU | TPLINK_CAP_AFC |
                               TPLINK_CAP_MESH | TPLINK_CAP_GAMING |
                               TPLINK_CAP_AI | TPLINK_CAP_QOS;

    /* Initialize hardware */
    tplink_write32(tdev, TPLINK_REG_CONTROL, TPLINK_CTRL_POWER_ON);
    msleep(100);

    tplink_write32(tdev, TPLINK_REG_INT_MASK, 0xFFFFFFFF);
    tplink_write32(tdev, TPLINK_REG_INT_STATUS, 0xFFFFFFFF);

    /* Request interrupt */
    ret = request_irq(dev->pdev->irq, tplink_interrupt, IRQF_SHARED,
                     "wifi7_tplink", tdev);
    if (ret) {
        dev_err(dev->dev, "Failed to request IRQ: %d\n", ret);
        goto err_unmap;
    }

    /* Enable interrupts */
    tplink_set_bits32(tdev, TPLINK_REG_CONTROL, TPLINK_CTRL_INT_ENABLE);

    /* Start workers */
    schedule_delayed_work(&tdev->workers.stats_work, HZ);
    schedule_delayed_work(&tdev->workers.temp_work, HZ);
    schedule_delayed_work(&tdev->workers.calib_work, HZ * 10);

    tdev->initialized = true;
    dev_info(dev->dev, "TP-Link WiFi 7 router initialized\n");

    return 0;

err_unmap:
    iounmap(tdev->mmio);
err_free:
    kfree(tdev);
    return ret;
}
EXPORT_SYMBOL(wifi7_tplink_init);

void wifi7_tplink_deinit(struct wifi7_dev *dev)
{
    struct wifi7_tplink_dev *tdev = tplink_dev;

    if (!tdev)
        return;

    tdev->initialized = false;

    /* Cancel workers */
    cancel_delayed_work_sync(&tdev->workers.stats_work);
    cancel_delayed_work_sync(&tdev->workers.temp_work);
    cancel_delayed_work_sync(&tdev->workers.calib_work);

    /* Disable interrupts */
    tplink_clear_bits32(tdev, TPLINK_REG_CONTROL, TPLINK_CTRL_INT_ENABLE);
    free_irq(dev->pdev->irq, tdev);

    /* Power down device */
    tplink_clear_bits32(tdev, TPLINK_REG_CONTROL, TPLINK_CTRL_POWER_ON);

    iounmap(tdev->mmio);
    kfree(tdev);
    tplink_dev = NULL;

    dev_info(dev->dev, "TP-Link WiFi 7 router deinitialized\n");
}
EXPORT_SYMBOL(wifi7_tplink_deinit);

/* Device probe */
int wifi7_tplink_probe(struct wifi7_dev *dev)
{
    int ret;

    ret = pci_enable_device(dev->pdev);
    if (ret) {
        dev_err(dev->dev, "Failed to enable PCI device\n");
        return ret;
    }

    ret = pci_request_regions(dev->pdev, "wifi7_tplink");
    if (ret) {
        dev_err(dev->dev, "Failed to request PCI regions\n");
        goto err_disable;
    }

    ret = wifi7_tplink_init(dev);
    if (ret)
        goto err_release;

    return 0;

err_release:
    pci_release_regions(dev->pdev);
err_disable:
    pci_disable_device(dev->pdev);
    return ret;
}
EXPORT_SYMBOL(wifi7_tplink_probe);

int wifi7_tplink_remove(struct wifi7_dev *dev)
{
    wifi7_tplink_deinit(dev);
    pci_release_regions(dev->pdev);
    pci_disable_device(dev->pdev);
    return 0;
}
EXPORT_SYMBOL(wifi7_tplink_remove);

/* Device start/stop */
int wifi7_tplink_start(struct wifi7_dev *dev)
{
    struct wifi7_tplink_dev *tdev = tplink_dev;
    int ret;

    if (!tdev || !tdev->initialized)
        return -EINVAL;

    /* Initialize radios */
    ret = wifi7_rf_init(dev);
    if (ret) {
        dev_err(dev->dev, "Failed to initialize radios\n");
        return ret;
    }

    /* Start MAC */
    ret = wifi7_mac_start(dev);
    if (ret) {
        dev_err(dev->dev, "Failed to start MAC\n");
        goto err_rf;
    }

    /* Start PHY */
    ret = wifi7_phy_start(dev);
    if (ret) {
        dev_err(dev->dev, "Failed to start PHY\n");
        goto err_mac;
    }

    tplink_set_bits32(tdev, TPLINK_REG_CONTROL,
                      TPLINK_CTRL_LED_ENABLE |
                      TPLINK_CTRL_AFC_ENABLE |
                      TPLINK_CTRL_MESH_ENABLE);

    dev_info(dev->dev, "TP-Link WiFi 7 router started\n");
    return 0;

err_mac:
    wifi7_mac_stop(dev);
err_rf:
    wifi7_rf_deinit(dev);
    return ret;
}
EXPORT_SYMBOL(wifi7_tplink_start);

void wifi7_tplink_stop(struct wifi7_dev *dev)
{
    struct wifi7_tplink_dev *tdev = tplink_dev;

    if (!tdev || !tdev->initialized)
        return;

    /* Stop all subsystems */
    wifi7_phy_stop(dev);
    wifi7_mac_stop(dev);
    wifi7_rf_deinit(dev);

    tplink_clear_bits32(tdev, TPLINK_REG_CONTROL,
                       TPLINK_CTRL_LED_ENABLE |
                       TPLINK_CTRL_AFC_ENABLE |
                       TPLINK_CTRL_MESH_ENABLE);

    dev_info(dev->dev, "TP-Link WiFi 7 router stopped\n");
}
EXPORT_SYMBOL(wifi7_tplink_stop);

/* Configuration functions */
int wifi7_tplink_set_config(struct wifi7_dev *dev,
                           struct wifi7_tplink_config *config)
{
    struct wifi7_tplink_dev *tdev = tplink_dev;
    unsigned long flags;

    if (!tdev || !tdev->initialized)
        return -EINVAL;

    spin_lock_irqsave(&tdev->lock, flags);
    memcpy(&tdev->config, config, sizeof(*config));
    spin_unlock_irqrestore(&tdev->lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_tplink_set_config);

int wifi7_tplink_get_config(struct wifi7_dev *dev,
                           struct wifi7_tplink_config *config)
{
    struct wifi7_tplink_dev *tdev = tplink_dev;
    unsigned long flags;

    if (!tdev || !tdev->initialized)
        return -EINVAL;

    spin_lock_irqsave(&tdev->lock, flags);
    memcpy(config, &tdev->config, sizeof(*config));
    spin_unlock_irqrestore(&tdev->lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_tplink_get_config);

/* Statistics functions */
int wifi7_tplink_get_stats(struct wifi7_dev *dev,
                          struct wifi7_tplink_stats *stats)
{
    struct wifi7_tplink_dev *tdev = tplink_dev;
    unsigned long flags;

    if (!tdev || !tdev->initialized)
        return -EINVAL;

    spin_lock_irqsave(&tdev->lock, flags);
    memcpy(stats, &tdev->stats, sizeof(*stats));
    spin_unlock_irqrestore(&tdev->lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_tplink_get_stats);

int wifi7_tplink_clear_stats(struct wifi7_dev *dev)
{
    struct wifi7_tplink_dev *tdev = tplink_dev;
    unsigned long flags;

    if (!tdev || !tdev->initialized)
        return -EINVAL;

    spin_lock_irqsave(&tdev->lock, flags);
    memset(&tdev->stats, 0, sizeof(tdev->stats));
    spin_unlock_irqrestore(&tdev->lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_tplink_clear_stats);

/* Feature control functions */
int wifi7_tplink_set_gaming_mode(struct wifi7_dev *dev, bool enable)
{
    struct wifi7_tplink_dev *tdev = tplink_dev;

    if (!tdev || !tdev->initialized)
        return -EINVAL;

    if (enable)
        tplink_set_bits32(tdev, TPLINK_REG_CONTROL, TPLINK_CTRL_GAMING_MODE);
    else
        tplink_clear_bits32(tdev, TPLINK_REG_CONTROL, TPLINK_CTRL_GAMING_MODE);

    tdev->features.gaming_enabled = enable;
    return 0;
}
EXPORT_SYMBOL(wifi7_tplink_set_gaming_mode);

int wifi7_tplink_set_mesh_mode(struct wifi7_dev *dev, bool enable)
{
    struct wifi7_tplink_dev *tdev = tplink_dev;

    if (!tdev || !tdev->initialized)
        return -EINVAL;

    if (enable)
        tplink_set_bits32(tdev, TPLINK_REG_CONTROL, TPLINK_CTRL_MESH_ENABLE);
    else
        tplink_clear_bits32(tdev, TPLINK_REG_CONTROL, TPLINK_CTRL_MESH_ENABLE);

    tdev->features.mesh_enabled = enable;
    return 0;
}
EXPORT_SYMBOL(wifi7_tplink_set_mesh_mode);

int wifi7_tplink_set_ai_optimization(struct wifi7_dev *dev, bool enable)
{
    struct wifi7_tplink_dev *tdev = tplink_dev;

    if (!tdev || !tdev->initialized)
        return -EINVAL;

    if (enable)
        tplink_set_bits32(tdev, TPLINK_REG_CONTROL, TPLINK_CTRL_AI_OPT);
    else
        tplink_clear_bits32(tdev, TPLINK_REG_CONTROL, TPLINK_CTRL_AI_OPT);

    tdev->features.ai_enabled = enable;
    return 0;
}
EXPORT_SYMBOL(wifi7_tplink_set_ai_optimization);

/* Module parameters */
static bool gaming_mode = false;
module_param(gaming_mode, bool, 0644);
MODULE_PARM_DESC(gaming_mode, "Enable gaming mode");

static bool mesh_mode = true;
module_param(mesh_mode, bool, 0644);
MODULE_PARM_DESC(mesh_mode, "Enable mesh networking");

static bool ai_optimization = true;
module_param(ai_optimization, bool, 0644);
MODULE_PARM_DESC(ai_optimization, "Enable AI optimization");

/* Module initialization */
static int __init wifi7_tplink_init_module(void)
{
    pr_info("TP-Link WiFi 7 router driver loaded\n");
    return 0;
}

static void __exit wifi7_tplink_exit_module(void)
{
    pr_info("TP-Link WiFi 7 router driver unloaded\n");
}

module_init(wifi7_tplink_init_module);
module_exit(wifi7_tplink_exit_module);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("TP-Link WiFi 7 Router Support");
MODULE_VERSION("1.0"); 