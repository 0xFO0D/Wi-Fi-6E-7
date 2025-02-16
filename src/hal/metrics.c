#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include "../../include/core/wifi67.h"
#include "metrics.h"

#define WIFI67_METRIC_TIMEOUT_MS 100
#define WIFI67_METRIC_RETRY_COUNT 3

struct wifi67_metrics {
    spinlock_t lock;
    u32 enabled_mask;
    struct {
        struct wifi67_radio_metrics radio[WIFI67_MAX_RADIOS];
        struct wifi67_link_metrics link[WIFI67_MAX_LINKS];
        ktime_t timestamp;
    } cache;
};

int wifi67_metrics_init(struct wifi67_priv *priv)
{
    struct wifi67_metrics *metrics;

    metrics = kzalloc(sizeof(*metrics), GFP_KERNEL);
    if (!metrics)
        return -ENOMEM;

    spin_lock_init(&metrics->lock);
    priv->metrics = metrics;

    /* Reset hardware metrics engine */
    wifi67_hw_write32(priv, WIFI67_REG_METRIC_CTRL,
                     WIFI67_METRIC_CTRL_RESET);
    udelay(100);
    wifi67_hw_write32(priv, WIFI67_REG_METRIC_CTRL, 0);

    return 0;
}

void wifi67_metrics_deinit(struct wifi67_priv *priv)
{
    if (priv->metrics) {
        wifi67_metrics_stop(priv);
        kfree(priv->metrics);
        priv->metrics = NULL;
    }
}

static bool wifi67_metrics_is_ready(struct wifi67_priv *priv)
{
    u32 status = wifi67_hw_read32(priv, WIFI67_REG_METRIC_STATUS);
    return (status & WIFI67_METRIC_STATUS_READY) &&
           !(status & WIFI67_METRIC_STATUS_BUSY);
}

static int wifi67_metrics_wait_ready(struct wifi67_priv *priv)
{
    unsigned long timeout = jiffies + msecs_to_jiffies(WIFI67_METRIC_TIMEOUT_MS);

    while (!wifi67_metrics_is_ready(priv)) {
        if (time_after(jiffies, timeout))
            return -ETIMEDOUT;
        usleep_range(100, 200);
    }

    return 0;
}

int wifi67_metrics_start(struct wifi67_priv *priv)
{
    struct wifi67_metrics *metrics = priv->metrics;
    unsigned long flags;
    u32 val;
    int ret;

    if (!metrics)
        return -EINVAL;

    spin_lock_irqsave(&metrics->lock, flags);

    /* Enable metrics collection */
    val = WIFI67_METRIC_CTRL_ENABLE | WIFI67_METRIC_CTRL_AUTO;
    wifi67_hw_write32(priv, WIFI67_REG_METRIC_CTRL, val);

    /* Wait for hardware to be ready */
    ret = wifi67_metrics_wait_ready(priv);
    if (ret)
        goto out;

    /* Enable interrupts */
    val = WIFI67_METRIC_INT_DONE | WIFI67_METRIC_INT_ERROR;
    wifi67_hw_write32(priv, WIFI67_REG_METRIC_MASK, val);

out:
    spin_unlock_irqrestore(&metrics->lock, flags);
    return ret;
}

void wifi67_metrics_stop(struct wifi67_priv *priv)
{
    struct wifi67_metrics *metrics = priv->metrics;
    unsigned long flags;

    if (!metrics)
        return;

    spin_lock_irqsave(&metrics->lock, flags);

    /* Disable metrics collection */
    wifi67_hw_write32(priv, WIFI67_REG_METRIC_CTRL, 0);
    wifi67_hw_write32(priv, WIFI67_REG_METRIC_MASK, 0);

    spin_unlock_irqrestore(&metrics->lock, flags);
}

static void wifi67_metrics_read_radio(struct wifi67_priv *priv,
                                    u8 radio_id,
                                    struct wifi67_radio_metrics *metrics)
{
    u32 val;

    val = wifi67_hw_read32(priv, WIFI67_REG_RADIO_RSSI + radio_id * 0x100);
    metrics->rssi = (s8)(val & 0xff);
    metrics->noise = (s8)((val >> 8) & 0xff);
    metrics->snr = (val >> 16) & 0xff;

    val = wifi67_hw_read32(priv, WIFI67_REG_RADIO_TEMP + radio_id * 0x100);
    metrics->temperature = val & 0xff;
    metrics->tx_power = (val >> 8) & 0xff;
    metrics->busy_percent = (val >> 16) & 0xff;
}

static void wifi67_metrics_read_link(struct wifi67_priv *priv,
                                   u8 link_id,
                                   struct wifi67_link_metrics *metrics)
{
    u32 val;

    val = wifi67_hw_read32(priv, WIFI67_REG_LINK_QUALITY + link_id * 0x100);
    metrics->quality = val & 0xff;
    metrics->airtime = (val >> 8) & 0xff;
    metrics->latency = (val >> 16) & 0xffff;

    val = wifi67_hw_read32(priv, WIFI67_REG_LINK_JITTER + link_id * 0x100);
    metrics->jitter = val & 0xffff;
    metrics->loss_percent = (val >> 16) & 0xff;
}

int wifi67_get_radio_metrics(struct wifi67_priv *priv, u8 radio_id,
                           struct wifi67_radio_metrics *metrics)
{
    struct wifi67_metrics *m = priv->metrics;
    unsigned long flags;
    int ret = 0;

    if (!m || radio_id >= WIFI67_MAX_RADIOS)
        return -EINVAL;

    spin_lock_irqsave(&m->lock, flags);

    /* Trigger a new sample if auto-sampling is disabled */
    if (!(wifi67_hw_read32(priv, WIFI67_REG_METRIC_CTRL) &
          WIFI67_METRIC_CTRL_AUTO)) {
        wifi67_hw_write32(priv, WIFI67_REG_METRIC_CTRL,
                         WIFI67_METRIC_CTRL_SAMPLE);
        ret = wifi67_metrics_wait_ready(priv);
        if (ret)
            goto out;
    }

    wifi67_metrics_read_radio(priv, radio_id, metrics);

out:
    spin_unlock_irqrestore(&m->lock, flags);
    return ret;
}

int wifi67_get_link_metrics(struct wifi67_priv *priv, u8 link_id,
                          struct wifi67_link_metrics *metrics)
{
    struct wifi67_metrics *m = priv->metrics;
    unsigned long flags;
    int ret = 0;

    if (!m || link_id >= WIFI67_MAX_LINKS)
        return -EINVAL;

    spin_lock_irqsave(&m->lock, flags);

    /* Trigger a new sample if auto-sampling is disabled */
    if (!(wifi67_hw_read32(priv, WIFI67_REG_METRIC_CTRL) &
          WIFI67_METRIC_CTRL_AUTO)) {
        wifi67_hw_write32(priv, WIFI67_REG_METRIC_CTRL,
                         WIFI67_METRIC_CTRL_SAMPLE);
        ret = wifi67_metrics_wait_ready(priv);
        if (ret)
            goto out;
    }

    wifi67_metrics_read_link(priv, link_id, metrics);

out:
    spin_unlock_irqrestore(&m->lock, flags);
    return ret;
}

EXPORT_SYMBOL(wifi67_metrics_init);
EXPORT_SYMBOL(wifi67_metrics_deinit);
EXPORT_SYMBOL(wifi67_metrics_start);
EXPORT_SYMBOL(wifi67_metrics_stop);
EXPORT_SYMBOL(wifi67_get_radio_metrics);
EXPORT_SYMBOL(wifi67_get_link_metrics); 