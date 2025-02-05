/*
 * WiFi 7 Power Management Implementation
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include <linux/random.h>
#include <linux/crc32.h>
#include <linux/completion.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>

#include "wifi7_power.h"
#include "../core/wifi7_core.h"

/* Helper Functions */

static bool wifi7_pm_is_valid_state(u8 state)
{
    return (state <= WIFI7_PM_STATE_POWER_DOWN);
}

static bool wifi7_pm_can_transition(struct wifi7_pm *pm,
                                  u8 current_state,
                                  u8 target_state)
{
    if (!wifi7_pm_is_valid_state(current_state) ||
        !wifi7_pm_is_valid_state(target_state))
        return false;

    /* Prevent invalid transitions */
    if (current_state == target_state)
        return false;

    if (current_state == WIFI7_PM_STATE_POWER_DOWN)
        return false;

    return true;
}

static void wifi7_pm_update_power_stats(struct wifi7_pm *pm)
{
    ktime_t now = ktime_get();
    u32 time_diff;

    time_diff = ktime_to_ms(ktime_sub(now, pm->timing.last_activity));

    switch (pm->state) {
    case WIFI7_PM_STATE_AWAKE:
        pm->stats.awake_time += time_diff;
        break;
    case WIFI7_PM_STATE_DOZE:
        pm->stats.doze_time += time_diff;
        break;
    case WIFI7_PM_STATE_SLEEP:
        pm->stats.sleep_time += time_diff;
        break;
    case WIFI7_PM_STATE_DEEP_SLEEP:
        pm->stats.deep_sleep_time += time_diff;
        break;
    }

    pm->timing.last_activity = now;
}

/* TWT Functions */

static int wifi7_pm_twt_validate_flow(struct wifi7_pm_twt_flow *flow)
{
    if (!flow)
        return -EINVAL;

    if (flow->wake_interval_exp > 31)
        return -EINVAL;

    if (flow->wake_duration == 0)
        return -EINVAL;

    return 0;
}

static void wifi7_pm_twt_work_handler(struct work_struct *work)
{
    struct wifi7_pm *pm = container_of(work, struct wifi7_pm,
                                     twt_work.work);
    unsigned long flags;
    int i;

    spin_lock_irqsave(&pm->twt_lock, flags);

    for (i = 0; i < pm->num_twt_flows; i++) {
        struct wifi7_pm_twt_flow *flow = &pm->twt_flows[i];
        
        if (!flow->active)
            continue;

        /* Check if it's time to wake up for this flow */
        if (ktime_to_ms(ktime_get()) >= flow->target_wake_time) {
            /* Transition to awake state if needed */
            if (pm->state != WIFI7_PM_STATE_AWAKE) {
                spin_unlock_irqrestore(&pm->twt_lock, flags);
                wifi7_pm_set_state(pm->dev, WIFI7_PM_STATE_AWAKE);
                spin_lock_irqsave(&pm->twt_lock, flags);
            }

            /* Calculate next wake time */
            flow->target_wake_time += (1 << flow->wake_interval_exp) *
                                    flow->wake_interval_mantissa;
        }
    }

    spin_unlock_irqrestore(&pm->twt_lock, flags);

    /* Reschedule work */
    if (pm->num_twt_flows > 0)
        queue_delayed_work(pm->wq, &pm->twt_work,
                          msecs_to_jiffies(100));
}

/* Queue Functions */

static int wifi7_pm_queue_validate(struct wifi7_pm_queue *queue)
{
    if (!queue)
        return -EINVAL;

    if (queue->queue_id >= WIFI7_PM_MAX_PS_QUEUES)
        return -EINVAL;

    if (queue->service_period == 0)
        return -EINVAL;

    return 0;
}

static void wifi7_pm_process_queues(struct wifi7_pm *pm)
{
    unsigned long flags;
    int i;

    spin_lock_irqsave(&pm->queue_lock, flags);

    for (i = 0; i < pm->num_queues; i++) {
        struct wifi7_pm_queue *queue = &pm->queues[i];
        struct sk_buff *skb;

        spin_lock(&queue->lock);

        while ((skb = __skb_dequeue(&queue->skb_queue))) {
            /* Process frame based on power save mode */
            if (queue->uapsd) {
                /* U-APSD handling */
            } else if (queue->wmm_ps) {
                /* WMM-PS handling */
            } else {
                /* Legacy PS handling */
            }
        }

        spin_unlock(&queue->lock);
    }

    spin_unlock_irqrestore(&pm->queue_lock, flags);
}

/* Power Management Work Handler */

static void wifi7_pm_work_handler(struct work_struct *work)
{
    struct wifi7_pm *pm = container_of(work, struct wifi7_pm,
                                     ps_work.work);
    unsigned long flags;

    spin_lock_irqsave(&pm->state_lock, flags);

    /* Check if state transition is needed */
    if (pm->state != pm->target_state) {
        if (wifi7_pm_can_transition(pm, pm->state, pm->target_state)) {
            u8 old_state = pm->state;
            pm->state = pm->target_state;

            /* Update statistics */
            switch (pm->state) {
            case WIFI7_PM_STATE_AWAKE:
                pm->stats.awake_count++;
                break;
            case WIFI7_PM_STATE_DOZE:
                pm->stats.doze_count++;
                break;
            case WIFI7_PM_STATE_SLEEP:
                pm->stats.sleep_count++;
                break;
            case WIFI7_PM_STATE_DEEP_SLEEP:
                pm->stats.deep_sleep_count++;
                break;
            }

            wifi7_pm_update_power_stats(pm);
        } else {
            pm->stats.transition_failures++;
        }
    }

    spin_unlock_irqrestore(&pm->state_lock, flags);

    /* Process power save queues */
    wifi7_pm_process_queues(pm);

    /* Reschedule work */
    if (pm->ps_enabled)
        queue_delayed_work(pm->wq, &pm->ps_work,
                          msecs_to_jiffies(100));
}

/* Monitor Work Handler */

static void wifi7_pm_monitor_work_handler(struct work_struct *work)
{
    struct wifi7_pm *pm = container_of(work, struct wifi7_pm,
                                     monitor_work.work);
    unsigned long flags;
    ktime_t now = ktime_get();

    spin_lock_irqsave(&pm->timing_lock, flags);

    /* Check for beacon misses */
    if (ktime_to_ms(ktime_sub(now, pm->timing.last_beacon)) >
        2 * pm->timing.beacon_interval)
        pm->stats.beacon_misses++;

    /* Update power consumption statistics */
    wifi7_pm_update_power_stats(pm);

    spin_unlock_irqrestore(&pm->timing_lock, flags);

    /* Reschedule work */
    queue_delayed_work(pm->wq, &pm->monitor_work,
                      msecs_to_jiffies(1000));
}

/* Public API Functions */

int wifi7_pm_init(struct wifi7_dev *dev)
{
    struct wifi7_pm *pm;
    int ret = 0;

    if (!dev)
        return -EINVAL;

    pm = kzalloc(sizeof(*pm), GFP_KERNEL);
    if (!pm)
        return -ENOMEM;

    /* Initialize locks */
    spin_lock_init(&pm->state_lock);
    spin_lock_init(&pm->twt_lock);
    spin_lock_init(&pm->queue_lock);
    spin_lock_init(&pm->timing_lock);
    spin_lock_init(&pm->power_lock);

    /* Initialize work queue */
    pm->wq = create_singlethread_workqueue("wifi7_pm");
    if (!pm->wq) {
        ret = -ENOMEM;
        goto err_free;
    }

    /* Initialize work items */
    INIT_DELAYED_WORK(&pm->ps_work, wifi7_pm_work_handler);
    INIT_DELAYED_WORK(&pm->twt_work, wifi7_pm_twt_work_handler);
    INIT_DELAYED_WORK(&pm->monitor_work, wifi7_pm_monitor_work_handler);

    /* Set initial state */
    pm->state = WIFI7_PM_STATE_AWAKE;
    pm->target_state = WIFI7_PM_STATE_AWAKE;
    pm->ps_enabled = false;

    /* Initialize timing */
    pm->timing.beacon_interval = 100;
    pm->timing.dtim_period = 1;
    pm->timing.listen_interval = 1;
    pm->timing.last_activity = ktime_get();
    pm->timing.last_beacon = ktime_get();

    /* Initialize capabilities */
    pm->capabilities = WIFI7_PM_CAP_PSM | WIFI7_PM_CAP_UAPSD |
                      WIFI7_PM_CAP_WMM_PS | WIFI7_PM_CAP_TWT |
                      WIFI7_PM_CAP_MLO_PS | WIFI7_PM_CAP_DYNAMIC |
                      WIFI7_PM_CAP_ADAPTIVE;

    /* Store device reference */
    pm->dev = dev;
    dev->pm = pm;

    return 0;

err_free:
    kfree(pm);
    return ret;
}

void wifi7_pm_deinit(struct wifi7_dev *dev)
{
    struct wifi7_pm *pm;

    if (!dev || !dev->pm)
        return;

    pm = dev->pm;

    /* Cancel and flush work items */
    cancel_delayed_work_sync(&pm->ps_work);
    cancel_delayed_work_sync(&pm->twt_work);
    cancel_delayed_work_sync(&pm->monitor_work);

    /* Destroy work queue */
    destroy_workqueue(pm->wq);

    /* Free memory */
    kfree(pm);
    dev->pm = NULL;
}

int wifi7_pm_start(struct wifi7_dev *dev)
{
    struct wifi7_pm *pm;

    if (!dev || !dev->pm)
        return -EINVAL;

    pm = dev->pm;

    /* Enable power save */
    pm->ps_enabled = true;

    /* Schedule work items */
    queue_delayed_work(pm->wq, &pm->ps_work, 0);
    queue_delayed_work(pm->wq, &pm->monitor_work, 0);

    return 0;
}

void wifi7_pm_stop(struct wifi7_dev *dev)
{
    struct wifi7_pm *pm;

    if (!dev || !dev->pm)
        return;

    pm = dev->pm;

    /* Disable power save */
    pm->ps_enabled = false;

    /* Cancel work items */
    cancel_delayed_work_sync(&pm->ps_work);
    cancel_delayed_work_sync(&pm->twt_work);
    cancel_delayed_work_sync(&pm->monitor_work);

    /* Set awake state */
    wifi7_pm_set_state(dev, WIFI7_PM_STATE_AWAKE);
}

int wifi7_pm_set_state(struct wifi7_dev *dev, u8 state)
{
    struct wifi7_pm *pm;
    unsigned long flags;

    if (!dev || !dev->pm)
        return -EINVAL;

    if (!wifi7_pm_is_valid_state(state))
        return -EINVAL;

    pm = dev->pm;

    spin_lock_irqsave(&pm->state_lock, flags);
    pm->target_state = state;
    spin_unlock_irqrestore(&pm->state_lock, flags);

    /* Schedule state transition work */
    queue_delayed_work(pm->wq, &pm->ps_work, 0);

    return 0;
}

int wifi7_pm_get_state(struct wifi7_dev *dev, u8 *state)
{
    struct wifi7_pm *pm;
    unsigned long flags;

    if (!dev || !dev->pm || !state)
        return -EINVAL;

    pm = dev->pm;

    spin_lock_irqsave(&pm->state_lock, flags);
    *state = pm->state;
    spin_unlock_irqrestore(&pm->state_lock, flags);

    return 0;
}

int wifi7_pm_add_twt_flow(struct wifi7_dev *dev,
                         struct wifi7_pm_twt_flow *flow)
{
    struct wifi7_pm *pm;
    unsigned long flags;
    int ret;

    if (!dev || !dev->pm)
        return -EINVAL;

    ret = wifi7_pm_twt_validate_flow(flow);
    if (ret)
        return ret;

    pm = dev->pm;

    spin_lock_irqsave(&pm->twt_lock, flags);

    if (pm->num_twt_flows >= WIFI7_PM_MAX_TWT_FLOWS) {
        spin_unlock_irqrestore(&pm->twt_lock, flags);
        return -ENOSPC;
    }

    /* Add flow */
    memcpy(&pm->twt_flows[pm->num_twt_flows], flow,
           sizeof(*flow));
    pm->num_twt_flows++;

    /* Start TWT work if this is the first flow */
    if (pm->num_twt_flows == 1)
        queue_delayed_work(pm->wq, &pm->twt_work, 0);

    spin_unlock_irqrestore(&pm->twt_lock, flags);

    return 0;
}

int wifi7_pm_del_twt_flow(struct wifi7_dev *dev, u8 flow_id)
{
    struct wifi7_pm *pm;
    unsigned long flags;
    int i;

    if (!dev || !dev->pm)
        return -EINVAL;

    pm = dev->pm;

    spin_lock_irqsave(&pm->twt_lock, flags);

    /* Find and remove flow */
    for (i = 0; i < pm->num_twt_flows; i++) {
        if (pm->twt_flows[i].flow_id == flow_id) {
            /* Shift remaining flows */
            if (i < pm->num_twt_flows - 1)
                memmove(&pm->twt_flows[i],
                       &pm->twt_flows[i + 1],
                       sizeof(struct wifi7_pm_twt_flow) *
                       (pm->num_twt_flows - i - 1));

            pm->num_twt_flows--;

            /* Stop TWT work if no more flows */
            if (pm->num_twt_flows == 0)
                cancel_delayed_work_sync(&pm->twt_work);

            spin_unlock_irqrestore(&pm->twt_lock, flags);
            return 0;
        }
    }

    spin_unlock_irqrestore(&pm->twt_lock, flags);
    return -ENOENT;
}

int wifi7_pm_queue_init(struct wifi7_dev *dev, u8 queue_id)
{
    struct wifi7_pm *pm;
    struct wifi7_pm_queue *queue;
    unsigned long flags;

    if (!dev || !dev->pm)
        return -EINVAL;

    if (queue_id >= WIFI7_PM_MAX_PS_QUEUES)
        return -EINVAL;

    pm = dev->pm;
    queue = &pm->queues[queue_id];

    spin_lock_irqsave(&pm->queue_lock, flags);

    if (queue_id >= pm->num_queues)
        pm->num_queues = queue_id + 1;

    /* Initialize queue */
    memset(queue, 0, sizeof(*queue));
    queue->queue_id = queue_id;
    spin_lock_init(&queue->lock);
    skb_queue_head_init(&queue->skb_queue);

    spin_unlock_irqrestore(&pm->queue_lock, flags);

    return 0;
}

void wifi7_pm_queue_deinit(struct wifi7_dev *dev, u8 queue_id)
{
    struct wifi7_pm *pm;
    struct wifi7_pm_queue *queue;
    unsigned long flags;

    if (!dev || !dev->pm)
        return;

    if (queue_id >= WIFI7_PM_MAX_PS_QUEUES)
        return;

    pm = dev->pm;
    queue = &pm->queues[queue_id];

    spin_lock_irqsave(&pm->queue_lock, flags);

    /* Clear queue */
    spin_lock(&queue->lock);
    __skb_queue_purge(&queue->skb_queue);
    spin_unlock(&queue->lock);

    if (queue_id == pm->num_queues - 1)
        pm->num_queues--;

    spin_unlock_irqrestore(&pm->queue_lock, flags);
}

int wifi7_pm_set_timing(struct wifi7_dev *dev,
                       struct wifi7_pm_timing *timing)
{
    struct wifi7_pm *pm;
    unsigned long flags;

    if (!dev || !dev->pm || !timing)
        return -EINVAL;

    pm = dev->pm;

    spin_lock_irqsave(&pm->timing_lock, flags);
    memcpy(&pm->timing, timing, sizeof(*timing));
    spin_unlock_irqrestore(&pm->timing_lock, flags);

    return 0;
}

int wifi7_pm_get_timing(struct wifi7_dev *dev,
                       struct wifi7_pm_timing *timing)
{
    struct wifi7_pm *pm;
    unsigned long flags;

    if (!dev || !dev->pm || !timing)
        return -EINVAL;

    pm = dev->pm;

    spin_lock_irqsave(&pm->timing_lock, flags);
    memcpy(timing, &pm->timing, sizeof(*timing));
    spin_unlock_irqrestore(&pm->timing_lock, flags);

    return 0;
}

int wifi7_pm_set_power(struct wifi7_dev *dev,
                      struct wifi7_pm_power *power)
{
    struct wifi7_pm *pm;
    unsigned long flags;

    if (!dev || !dev->pm || !power)
        return -EINVAL;

    pm = dev->pm;

    spin_lock_irqsave(&pm->power_lock, flags);
    memcpy(&pm->power, power, sizeof(*power));
    spin_unlock_irqrestore(&pm->power_lock, flags);

    return 0;
}

int wifi7_pm_get_power(struct wifi7_dev *dev,
                      struct wifi7_pm_power *power)
{
    struct wifi7_pm *pm;
    unsigned long flags;

    if (!dev || !dev->pm || !power)
        return -EINVAL;

    pm = dev->pm;

    spin_lock_irqsave(&pm->power_lock, flags);
    memcpy(power, &pm->power, sizeof(*power));
    spin_unlock_irqrestore(&pm->power_lock, flags);

    return 0;
}

int wifi7_pm_get_stats(struct wifi7_dev *dev,
                      struct wifi7_pm_stats *stats)
{
    struct wifi7_pm *pm;

    if (!dev || !dev->pm || !stats)
        return -EINVAL;

    pm = dev->pm;

    /* No locking needed as stats are atomic */
    memcpy(stats, &pm->stats, sizeof(*stats));

    return 0;
}

int wifi7_pm_clear_stats(struct wifi7_dev *dev)
{
    struct wifi7_pm *pm;

    if (!dev || !dev->pm)
        return -EINVAL;

    pm = dev->pm;

    /* No locking needed as stats are atomic */
    memset(&pm->stats, 0, sizeof(pm->stats));

    return 0;
}

/* Module Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Power Management");
MODULE_VERSION("1.0"); 