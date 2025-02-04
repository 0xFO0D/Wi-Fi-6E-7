/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include "wifi7_phy.h"
#include "wifi7_power.h"

/* Forward declarations */
static void wifi7_power_monitor_work(struct work_struct *work);
static void wifi7_power_cal_work(struct work_struct *work);
static int wifi7_power_update_voltage(struct wifi7_power_context *power,
                                    u16 voltage_mv);
static int wifi7_power_update_frequency(struct wifi7_power_context *power,
                                      u16 freq_mhz);
static void wifi7_power_handle_temp_event(struct wifi7_power_context *power,
                                        int temp);

/* Allocate power management context */
struct wifi7_power_context *wifi7_power_alloc(struct wifi7_phy_dev *phy)
{
    struct wifi7_power_context *power;
    int i;

    if (!phy)
        return NULL;

    power = kzalloc(sizeof(*power), GFP_KERNEL);
    if (!power)
        return NULL;

    power->phy = phy;
    atomic_set(&power->power_state, WIFI7_POWER_STATE_ACTIVE);
    spin_lock_init(&power->chain_lock);
    spin_lock_init(&power->cal_lock);
    spin_lock_init(&power->power_lock);

    /* Initialize chains */
    for (i = 0; i < WIFI7_MAX_TX_CHAINS; i++) {
        power->chains[i].enabled = false;
        power->chains[i].current_power = WIFI7_DEFAULT_TX_POWER_DBM;
        power->chains[i].max_power = WIFI7_MAX_TX_POWER_DBM;
    }

    /* Initialize calibration data */
    for (i = 0; i < WIFI7_CAL_MAX; i++) {
        power->cal_data[i].type = i;
        power->cal_data[i].valid = false;
        power->cal_data[i].in_progress = false;
        
        /* Set calibration intervals */
        switch (i) {
        case WIFI7_CAL_AGC:
        case WIFI7_CAL_DC_OFFSET:
            power->cal_data[i].interval_ms = WIFI7_CAL_INTERVAL_SHORT_MS;
            break;
        case WIFI7_CAL_IQ_IMBALANCE:
        case WIFI7_CAL_PHASE_NOISE:
            power->cal_data[i].interval_ms = WIFI7_CAL_INTERVAL_NORMAL_MS;
            break;
        case WIFI7_CAL_TEMP_COMP:
            power->cal_data[i].interval_ms = WIFI7_CAL_INTERVAL_LONG_MS;
            break;
        }
    }

    /* Create workqueues */
    power->cal_wq = alloc_workqueue("wifi7_cal_wq",
                                   WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
    if (!power->cal_wq)
        goto err_free_power;

    power->power_wq = alloc_workqueue("wifi7_power_wq",
                                     WQ_FREEZABLE | WQ_MEM_RECLAIM, 0);
    if (!power->power_wq)
        goto err_free_cal_wq;

    INIT_DELAYED_WORK(&power->cal_work, wifi7_power_cal_work);
    INIT_DELAYED_WORK(&power->power_work, wifi7_power_monitor_work);

    /* Set default power profile */
    power->profile.state = WIFI7_POWER_STATE_ACTIVE;
    power->profile.voltage_mv = WIFI7_MAX_VOLTAGE_MV;
    power->profile.frequency_mhz = WIFI7_MAX_FREQ_MHZ;
    power->profile.tx_chains_active = 0;
    power->profile.rx_chains_active = 0;
    power->profile.sleep_capable = true;

    return power;

err_free_cal_wq:
    destroy_workqueue(power->cal_wq);
err_free_power:
    kfree(power);
    return NULL;
}
EXPORT_SYMBOL_GPL(wifi7_power_alloc);

void wifi7_power_free(struct wifi7_power_context *power)
{
    if (!power)
        return;

    if (power->cal_wq) {
        cancel_delayed_work_sync(&power->cal_work);
        destroy_workqueue(power->cal_wq);
    }

    if (power->power_wq) {
        cancel_delayed_work_sync(&power->power_work);
        destroy_workqueue(power->power_wq);
    }

    kfree(power);
}
EXPORT_SYMBOL_GPL(wifi7_power_free);

/* Power state management */
int wifi7_power_set_state(struct wifi7_power_context *power, u8 state)
{
    unsigned long flags;
    int ret = 0;

    if (!power)
        return -EINVAL;

    if (state > WIFI7_POWER_STATE_DEEP_SLEEP)
        return -EINVAL;

    spin_lock_irqsave(&power->power_lock, flags);

    if (atomic_read(&power->power_state) == state)
        goto out_unlock;

    /* Check if state transition is allowed */
    if (state == WIFI7_POWER_STATE_DEEP_SLEEP && !power->profile.sleep_capable) {
        ret = -EPERM;
        goto out_unlock;
    }

    /* Perform state transition */
    switch (state) {
    case WIFI7_POWER_STATE_ACTIVE:
        /* Restore full power operation */
        ret = wifi7_power_update_voltage(power, power->profile.voltage_mv);
        if (ret)
            goto out_unlock;
        ret = wifi7_power_update_frequency(power, power->profile.frequency_mhz);
        if (ret)
            goto out_unlock;
        break;

    case WIFI7_POWER_STATE_SLEEP:
        /* Reduce voltage and frequency */
        ret = wifi7_power_update_frequency(power, WIFI7_MIN_FREQ_MHZ);
        if (ret)
            goto out_unlock;
        ret = wifi7_power_update_voltage(power, WIFI7_MIN_VOLTAGE_MV);
        if (ret)
            goto out_unlock;
        break;

    case WIFI7_POWER_STATE_DEEP_SLEEP:
        /* Prepare for deep sleep */
        ret = wifi7_power_update_frequency(power, 0);
        if (ret)
            goto out_unlock;
        ret = wifi7_power_update_voltage(power, 0);
        if (ret)
            goto out_unlock;
        break;
    }

    atomic_set(&power->power_state, state);
    power->stats.state_changes++;
    power->stats.last_state_change = ktime_get();

out_unlock:
    spin_unlock_irqrestore(&power->power_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_power_set_state);

/* Chain control */
int wifi7_power_enable_chain(struct wifi7_power_context *power,
                            u8 chain_idx,
                            bool enable)
{
    unsigned long flags;
    int ret = 0;

    if (!power || chain_idx >= WIFI7_MAX_TX_CHAINS)
        return -EINVAL;

    spin_lock_irqsave(&power->chain_lock, flags);

    if (power->chains[chain_idx].enabled == enable)
        goto out_unlock;

    /* Update chain state */
    power->chains[chain_idx].enabled = enable;
    if (enable) {
        power->profile.tx_chains_active++;
        /* Initialize power tracking */
        power->chains[chain_idx].tracking.samples = 0;
        power->chains[chain_idx].tracking.avg_power = 0;
        power->chains[chain_idx].tracking.peak_power = 0;
        power->chains[chain_idx].tracking.overpower_count = 0;
    } else {
        power->profile.tx_chains_active--;
    }

    /* Notify hardware */
    if (power->phy->ops && power->phy->ops->set_chain_state)
        ret = power->phy->ops->set_chain_state(power->phy, chain_idx, enable);

out_unlock:
    spin_unlock_irqrestore(&power->chain_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_power_enable_chain);

/* Calibration control */
static void wifi7_power_cal_work(struct work_struct *work)
{
    struct wifi7_power_context *power = container_of(work,
                                                   struct wifi7_power_context,
                                                   cal_work.work);
    unsigned long flags;
    int i, ret;

    spin_lock_irqsave(&power->cal_lock, flags);

    /* Check each calibration type */
    for (i = 0; i < WIFI7_CAL_MAX; i++) {
        struct wifi7_cal_data *cal = &power->cal_data[i];

        if (cal->in_progress)
            continue;

        /* Check if calibration is needed */
        if (time_after(jiffies, cal->last_cal_time +
                      msecs_to_jiffies(cal->interval_ms))) {
            cal->in_progress = true;
            spin_unlock_irqrestore(&power->cal_lock, flags);

            /* Perform calibration */
            ret = wifi7_power_start_cal(power, cal->type);
            if (ret) {
                power->stats.cal_failures++;
                pr_err("Calibration failed for type %d: %d\n", i, ret);
            } else {
                power->stats.cal_attempts++;
            }

            spin_lock_irqsave(&power->cal_lock, flags);
            cal->in_progress = false;
            cal->last_cal_time = jiffies;
        }
    }

    spin_unlock_irqrestore(&power->cal_lock, flags);

    /* Reschedule calibration work */
    queue_delayed_work(power->cal_wq, &power->cal_work,
                      msecs_to_jiffies(WIFI7_CAL_INTERVAL_SHORT_MS));
}

int wifi7_power_start_cal(struct wifi7_power_context *power,
                         enum wifi7_cal_type type)
{
    int ret = 0;

    if (!power || type >= WIFI7_CAL_MAX)
        return -EINVAL;

    /* Perform calibration based on type */
    switch (type) {
    case WIFI7_CAL_AGC:
        if (power->phy->ops && power->phy->ops->calibrate_agc)
            ret = power->phy->ops->calibrate_agc(power->phy);
        break;

    case WIFI7_CAL_DC_OFFSET:
        if (power->phy->ops && power->phy->ops->calibrate_dc)
            ret = power->phy->ops->calibrate_dc(power->phy);
        break;

    case WIFI7_CAL_IQ_IMBALANCE:
        if (power->phy->ops && power->phy->ops->calibrate_iq)
            ret = power->phy->ops->calibrate_iq(power->phy);
        break;

    case WIFI7_CAL_PHASE_NOISE:
        if (power->phy->ops && power->phy->ops->calibrate_phase)
            ret = power->phy->ops->calibrate_phase(power->phy);
        break;

    case WIFI7_CAL_TEMP_COMP:
        if (power->phy->ops && power->phy->ops->calibrate_temp)
            ret = power->phy->ops->calibrate_temp(power->phy);
        break;
    }

    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_power_start_cal);

/* Temperature management */
static void wifi7_power_handle_temp_event(struct wifi7_power_context *power,
                                        int temp)
{
    unsigned long flags;
    int old_temp;

    spin_lock_irqsave(&power->power_lock, flags);
    
    old_temp = power->temperature;
    power->temperature = temp;

    /* Check temperature thresholds */
    if (temp >= WIFI7_TEMP_SHUTDOWN) {
        /* Emergency shutdown */
        wifi7_power_set_state(power, WIFI7_POWER_STATE_DEEP_SLEEP);
        power->stats.temp_critical++;
    } else if (temp >= WIFI7_TEMP_CRITICAL) {
        /* Critical - reduce power significantly */
        int i;
        for (i = 0; i < WIFI7_MAX_TX_CHAINS; i++) {
            if (power->chains[i].enabled)
                wifi7_power_set_chain_power(power, i,
                                          WIFI7_MIN_TX_POWER_DBM);
        }
        power->stats.temp_critical++;
    } else if (temp >= WIFI7_TEMP_WARNING) {
        /* Warning - start reducing power */
        int i;
        for (i = 0; i < WIFI7_MAX_TX_CHAINS; i++) {
            if (power->chains[i].enabled)
                wifi7_power_set_chain_power(power, i,
                                          power->chains[i].current_power / 2);
        }
        power->stats.temp_warnings++;
    } else if (temp <= WIFI7_TEMP_NORMAL && old_temp > WIFI7_TEMP_NORMAL) {
        /* Temperature back to normal - restore power */
        int i;
        for (i = 0; i < WIFI7_MAX_TX_CHAINS; i++) {
            if (power->chains[i].enabled)
                wifi7_power_set_chain_power(power, i,
                                          power->chains[i].max_power);
        }
    }

    spin_unlock_irqrestore(&power->power_lock, flags);
}

/* Power monitoring work */
static void wifi7_power_monitor_work(struct work_struct *work)
{
    struct wifi7_power_context *power = container_of(work,
                                                   struct wifi7_power_context,
                                                   power_work.work);
    int temp;

    /* Read current temperature */
    if (power->thermal_zone) {
        int ret = thermal_zone_get_temp(power->thermal_zone, &temp);
        if (ret == 0)
            wifi7_power_handle_temp_event(power, temp);
    }

    /* Reschedule monitoring */
    queue_delayed_work(power->power_wq, &power->power_work,
                      msecs_to_jiffies(1000));  /* Check every second */
}

/* Debug interface */
void wifi7_power_dump_stats(struct wifi7_power_context *power)
{
    if (!power)
        return;

    pr_info("WiFi 7 Power Management Statistics:\n");
    pr_info("State changes: %u\n", power->stats.state_changes);
    pr_info("Voltage changes: %u\n", power->stats.voltage_changes);
    pr_info("Frequency changes: %u\n", power->stats.freq_changes);
    pr_info("Temperature warnings: %u\n", power->stats.temp_warnings);
    pr_info("Temperature critical: %u\n", power->stats.temp_critical);
    pr_info("Calibration attempts: %u\n", power->stats.cal_attempts);
    pr_info("Calibration failures: %u\n", power->stats.cal_failures);

    pr_info("\nCurrent state:\n");
    pr_info("Power state: %d\n", atomic_read(&power->power_state));
    pr_info("Temperature: %d\n", power->temperature);
    pr_info("Voltage: %u mV\n", power->profile.voltage_mv);
    pr_info("Frequency: %u MHz\n", power->profile.frequency_mhz);
    pr_info("Active TX chains: %u\n", power->profile.tx_chains_active);
    pr_info("Active RX chains: %u\n", power->profile.rx_chains_active);
}
EXPORT_SYMBOL_GPL(wifi7_power_dump_stats);

/* Module initialization */
static int __init wifi7_power_init(void)
{
    pr_info("WiFi 7 Power Management initialized\n");
    return 0;
}

static void __exit wifi7_power_exit(void)
{
    pr_info("WiFi 7 Power Management unloaded\n");
}

module_init(wifi7_power_init);
module_exit(wifi7_power_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Power Management and Calibration");
MODULE_VERSION("1.0"); 