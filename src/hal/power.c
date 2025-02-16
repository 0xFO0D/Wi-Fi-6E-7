#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include "../../include/core/wifi67.h"
#include "power.h"

#define WIFI67_PWR_TIMEOUT_MS 100
#define WIFI67_PWR_RETRY_COUNT 3

struct wifi67_power {
    spinlock_t lock;
    struct {
        u8 state;
        ktime_t sleep_time;
        struct wifi67_power_stats stats;
    } radio[WIFI67_MAX_RADIOS];
};

int wifi67_power_init(struct wifi67_priv *priv)
{
    struct wifi67_power *power;
    int i;

    power = kzalloc(sizeof(*power), GFP_KERNEL);
    if (!power)
        return -ENOMEM;

    spin_lock_init(&power->lock);
    
    for (i = 0; i < WIFI67_MAX_RADIOS; i++) {
        power->radio[i].state = WIFI67_RADIO_PWR_ON;
        power->radio[i].sleep_time = 0;
    }

    priv->power = power;

    /* Reset power management hardware */
    wifi67_hw_write32(priv, WIFI67_REG_PWR_CTRL,
                     WIFI67_PWR_CTRL_RESET);
    udelay(100);
    wifi67_hw_write32(priv, WIFI67_REG_PWR_CTRL,
                     WIFI67_PWR_CTRL_ENABLE);

    return 0;
}

void wifi67_power_deinit(struct wifi67_priv *priv)
{
    if (priv->power) {
        /* Ensure all radios are awake */
        int i;
        for (i = 0; i < WIFI67_MAX_RADIOS; i++)
            wifi67_radio_wake(priv, i);
            
        kfree(priv->power);
        priv->power = NULL;
    }
}

static bool wifi67_power_wait_ready(struct wifi67_priv *priv)
{
    unsigned long timeout = jiffies + msecs_to_jiffies(WIFI67_PWR_TIMEOUT_MS);
    u32 status;

    do {
        status = wifi67_hw_read32(priv, WIFI67_REG_PWR_STATUS);
        if (!(status & WIFI67_PWR_STATUS_BUSY))
            return true;
        usleep_range(100, 200);
    } while (!time_after(jiffies, timeout));

    return false;
}

int wifi67_radio_sleep(struct wifi67_priv *priv, u8 radio_id, u8 sleep_mode)
{
    struct wifi67_power *power = priv->power;
    unsigned long flags;
    u32 val;
    int ret = 0;

    if (!power || radio_id >= WIFI67_MAX_RADIOS)
        return -EINVAL;

    spin_lock_irqsave(&power->lock, flags);

    if (power->radio[radio_id].state != WIFI67_RADIO_PWR_ON) {
        ret = -EBUSY;
        goto out;
    }

    /* Program sleep registers */
    val = wifi67_hw_read32(priv, WIFI67_REG_RADIO_PWR + radio_id * 0x100);
    val &= ~0xFF;
    val |= WIFI67_RADIO_PWR_SLEEP;
    wifi67_hw_write32(priv, WIFI67_REG_RADIO_PWR + radio_id * 0x100, val);

    val = sleep_mode;
    wifi67_hw_write32(priv, WIFI67_REG_RADIO_SLEEP + radio_id * 0x100, val);

    /* Trigger sleep transition */
    val = wifi67_hw_read32(priv, WIFI67_REG_PWR_CTRL);
    val |= WIFI67_PWR_CTRL_SLEEP;
    wifi67_hw_write32(priv, WIFI67_REG_PWR_CTRL, val);

    /* Wait for transition to complete */
    if (!wifi67_power_wait_ready(priv)) {
        ret = -ETIMEDOUT;
        goto out;
    }

    /* Update state */
    power->radio[radio_id].state = WIFI67_RADIO_PWR_SLEEP;
    power->radio[radio_id].sleep_time = ktime_get();
    power->radio[radio_id].stats.sleep_count++;

out:
    spin_unlock_irqrestore(&power->lock, flags);
    return ret;
}

int wifi67_radio_wake(struct wifi67_priv *priv, u8 radio_id)
{
    struct wifi67_power *power = priv->power;
    unsigned long flags;
    u32 val;
    int ret = 0;

    if (!power || radio_id >= WIFI67_MAX_RADIOS)
        return -EINVAL;

    spin_lock_irqsave(&power->lock, flags);

    if (power->radio[radio_id].state != WIFI67_RADIO_PWR_SLEEP) {
        ret = -EINVAL;
        goto out;
    }

    /* Program wake registers */
    val = wifi67_hw_read32(priv, WIFI67_REG_RADIO_PWR + radio_id * 0x100);
    val &= ~0xFF;
    val |= WIFI67_RADIO_PWR_ON;
    wifi67_hw_write32(priv, WIFI67_REG_RADIO_PWR + radio_id * 0x100, val);

    /* Trigger wake transition */
    val = wifi67_hw_read32(priv, WIFI67_REG_PWR_CTRL);
    val &= ~WIFI67_PWR_CTRL_SLEEP;
    wifi67_hw_write32(priv, WIFI67_REG_PWR_CTRL, val);

    /* Wait for transition to complete */
    if (!wifi67_power_wait_ready(priv)) {
        ret = -ETIMEDOUT;
        goto out;
    }

    /* Update state */
    power->radio[radio_id].state = WIFI67_RADIO_PWR_ON;
    power->radio[radio_id].stats.wake_count++;
    power->radio[radio_id].stats.last_sleep_duration = 
        ktime_ms_delta(ktime_get(), power->radio[radio_id].sleep_time);
    power->radio[radio_id].stats.total_sleep_time +=
        power->radio[radio_id].stats.last_sleep_duration;

out:
    spin_unlock_irqrestore(&power->lock, flags);
    return ret;
}

int wifi67_get_power_stats(struct wifi67_priv *priv, u8 radio_id,
                         struct wifi67_power_stats *stats)
{
    struct wifi67_power *power = priv->power;
    unsigned long flags;
    u32 val;

    if (!power || !stats || radio_id >= WIFI67_MAX_RADIOS)
        return -EINVAL;

    spin_lock_irqsave(&power->lock, flags);

    /* Copy cached stats */
    memcpy(stats, &power->radio[radio_id].stats,
           sizeof(struct wifi67_power_stats));

    /* Read current voltage/current */
    val = wifi67_hw_read32(priv, WIFI67_REG_RADIO_VOLT + radio_id * 0x100);
    stats->voltage_level = val & 0xFFFF;
    stats->current_draw = (val >> 16) & 0xFFFF;

    spin_unlock_irqrestore(&power->lock, flags);
    return 0;
}

EXPORT_SYMBOL(wifi67_power_init);
EXPORT_SYMBOL(wifi67_power_deinit);
EXPORT_SYMBOL(wifi67_radio_sleep);
EXPORT_SYMBOL(wifi67_radio_wake);
EXPORT_SYMBOL(wifi67_get_power_stats); 