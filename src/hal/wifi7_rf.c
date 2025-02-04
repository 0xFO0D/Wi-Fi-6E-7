#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/crc32.h>
#include "wifi7_rf.h"

#define RF_CAL_MAGIC 0x52464341  /* "RFCA" */
#define RF_TEMP_UPDATE_MS 1000
#define RF_MAX_RETRIES 3

struct wifi7_rf_priv {
    struct wifi7_rf_ops *ops;
    struct wifi7_rf_gain_table *gain_table;
    struct wifi7_rf_temp_comp temp_comp;
    struct delayed_work temp_work;
    spinlock_t lock;
    u32 current_freq;
    u32 current_bw;
    u8 current_power;
    bool calibrated;
};

static void rf_temp_comp_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct wifi7_rf_priv *priv = container_of(dwork,
                                            struct wifi7_rf_priv,
                                            temp_work);
    struct wifi7_phy_dev *dev = container_of(priv,
                                          struct wifi7_phy_dev,
                                          rf_priv);
    unsigned long flags;
    int ret;

    spin_lock_irqsave(&priv->lock, flags);
    ret = priv->ops->update_temp_comp(dev);
    spin_unlock_irqrestore(&priv->lock, flags);

    if (!ret)
        schedule_delayed_work(&priv->temp_work,
                            msecs_to_jiffies(RF_TEMP_UPDATE_MS));
}

static int validate_cal_data(struct wifi7_rf_cal_data *data)
{
    u32 crc, stored_crc;
    
    stored_crc = data->checksum;
    data->checksum = 0;
    
    crc = crc32(0, (void *)data, sizeof(*data));
    data->checksum = stored_crc;
    
    return (crc == stored_crc) ? 0 : -EINVAL;
}

static void update_cal_checksum(struct wifi7_rf_cal_data *data)
{
    data->checksum = 0;
    data->checksum = crc32(0, (void *)data, sizeof(*data));
}

int wifi7_rf_register_ops(struct wifi7_phy_dev *dev,
                         struct wifi7_rf_ops *ops)
{
    struct wifi7_rf_priv *priv;
    int ret;

    if (!dev || !ops)
        return -EINVAL;

    priv = kzalloc(sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    priv->gain_table = kzalloc(sizeof(*priv->gain_table) *
                              RF_MAX_GAIN_IDX, GFP_KERNEL);
    if (!priv->gain_table) {
        ret = -ENOMEM;
        goto err_free_priv;
    }

    spin_lock_init(&priv->lock);
    INIT_DELAYED_WORK(&priv->temp_work, rf_temp_comp_work);
    priv->ops = ops;
    dev->rf_priv = priv;

    if (ops->init) {
        ret = ops->init(dev);
        if (ret)
            goto err_free_gain;
    }

    schedule_delayed_work(&priv->temp_work,
                         msecs_to_jiffies(RF_TEMP_UPDATE_MS));

    return 0;

err_free_gain:
    kfree(priv->gain_table);
err_free_priv:
    kfree(priv);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_rf_register_ops);

void wifi7_rf_unregister_ops(struct wifi7_phy_dev *dev)
{
    struct wifi7_rf_priv *priv = dev->rf_priv;

    if (!priv)
        return;

    cancel_delayed_work_sync(&priv->temp_work);

    if (priv->ops && priv->ops->deinit)
        priv->ops->deinit(dev);

    kfree(priv->gain_table);
    kfree(priv);
    dev->rf_priv = NULL;
}
EXPORT_SYMBOL_GPL(wifi7_rf_unregister_ops);

static int rf_set_frequency(struct wifi7_phy_dev *dev, u32 freq)
{
    struct wifi7_rf_priv *priv = dev->rf_priv;
    unsigned long flags;
    int ret = 0, retries = 0;

    if (!priv->ops->set_frequency)
        return -ENOTSUPP;

    spin_lock_irqsave(&priv->lock, flags);

    while (retries++ < RF_MAX_RETRIES) {
        ret = priv->ops->set_frequency(dev, freq);
        if (!ret) {
            priv->current_freq = freq;
            break;
        }
        udelay(RF_PLL_LOCK_TIMEOUT_US);
    }

    spin_unlock_irqrestore(&priv->lock, flags);
    return ret;
}

static int rf_set_bandwidth(struct wifi7_phy_dev *dev, u32 bw)
{
    struct wifi7_rf_priv *priv = dev->rf_priv;
    unsigned long flags;
    int ret;

    if (!priv->ops->set_bandwidth)
        return -ENOTSUPP;

    if (!(bw & priv->ops->supported_bw))
        return -EINVAL;

    spin_lock_irqsave(&priv->lock, flags);
    ret = priv->ops->set_bandwidth(dev, bw);
    if (!ret)
        priv->current_bw = bw;
    spin_unlock_irqrestore(&priv->lock, flags);

    return ret;
}

static int rf_set_tx_power(struct wifi7_phy_dev *dev, u8 power)
{
    struct wifi7_rf_priv *priv = dev->rf_priv;
    unsigned long flags;
    int ret;

    if (!priv->ops->set_tx_power)
        return -ENOTSUPP;

    spin_lock_irqsave(&priv->lock, flags);
    ret = priv->ops->set_tx_power(dev, power);
    if (!ret)
        priv->current_power = power;
    spin_unlock_irqrestore(&priv->lock, flags);

    return ret;
}

static int rf_update_psd_mask(struct wifi7_phy_dev *dev, u32 mask)
{
    struct wifi7_rf_priv *priv = dev->rf_priv;
    unsigned long flags;
    int ret;

    if (!priv->ops->update_psd_mask)
        return -ENOTSUPP;

    spin_lock_irqsave(&priv->lock, flags);
    ret = priv->ops->update_psd_mask(dev, mask);
    spin_unlock_irqrestore(&priv->lock, flags);

    return ret;
}

static int rf_calibrate(struct wifi7_phy_dev *dev, u32 cal_mask)
{
    struct wifi7_rf_priv *priv = dev->rf_priv;
    struct wifi7_rf_cal_data cal_data;
    unsigned long flags;
    int ret;

    if (!priv->ops->calibrate || !priv->ops->save_cal_data)
        return -ENOTSUPP;

    spin_lock_irqsave(&priv->lock, flags);
    ret = priv->ops->calibrate(dev, cal_mask);
    if (!ret) {
        memset(&cal_data, 0, sizeof(cal_data));
        cal_data.timestamp = jiffies_to_msecs(jiffies);
        cal_data.temperature = priv->temp_comp.last_temp;
        cal_data.channel_freq = priv->current_freq;
        update_cal_checksum(&cal_data);
        ret = priv->ops->save_cal_data(dev, &cal_data);
        if (!ret)
            priv->calibrated = true;
    }
    spin_unlock_irqrestore(&priv->lock, flags);

    return ret;
}

static int rf_load_calibration(struct wifi7_phy_dev *dev)
{
    struct wifi7_rf_priv *priv = dev->rf_priv;
    struct wifi7_rf_cal_data cal_data;
    unsigned long flags;
    int ret;

    if (!priv->ops->load_cal_data)
        return -ENOTSUPP;

    spin_lock_irqsave(&priv->lock, flags);
    ret = priv->ops->load_cal_data(dev, &cal_data);
    if (!ret && !validate_cal_data(&cal_data))
        priv->calibrated = true;
    spin_unlock_irqrestore(&priv->lock, flags);

    return ret;
}

/* Module initialization */
static int __init wifi7_rf_init(void)
{
    pr_info("WiFi 7 RF HAL initialized\n");
    return 0;
}

static void __exit wifi7_rf_exit(void)
{
    pr_info("WiFi 7 RF HAL unloaded\n");
}

module_init(wifi7_rf_init);
module_exit(wifi7_rf_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 RF Hardware Abstraction Layer");
MODULE_VERSION("1.0"); 