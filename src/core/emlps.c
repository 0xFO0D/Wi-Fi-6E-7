#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <linux/pm_qos.h>
#include "../../include/core/wifi67.h"
#include "../../include/core/emlps.h"

struct wifi67_emlps {
    spinlock_t lock;
    u8 state;
    u32 timeout;
    u32 awake_window;
    bool force_active;
    struct delayed_work ps_work;
    struct pm_qos_request pm_qos;
    u32 active_links;
    ktime_t last_activity;
};

static struct wifi67_emlps *emlps_alloc(void)
{
    struct wifi67_emlps *emlps;

    emlps = kzalloc(sizeof(*emlps), GFP_KERNEL);
    if (!emlps)
        return NULL;

    spin_lock_init(&emlps->lock);
    INIT_DELAYED_WORK(&emlps->ps_work, wifi67_emlps_work_handler);
    emlps->last_activity = ktime_get();
    return emlps;
}

int wifi67_emlps_init(struct wifi67_priv *priv)
{
    struct wifi67_emlps *emlps;

    emlps = emlps_alloc();
    if (!emlps)
        return -ENOMEM;

    emlps->state = WIFI67_EMLPS_DISABLED;
    emlps->timeout = WIFI67_EMLPS_DEFAULT_TIMEOUT;
    emlps->awake_window = WIFI67_EMLPS_DEFAULT_WINDOW;
    emlps->force_active = false;
    emlps->active_links = 0;

    pm_qos_add_request(&emlps->pm_qos, PM_QOS_CPU_DMA_LATENCY,
                      PM_QOS_DEFAULT_VALUE);

    priv->emlps = emlps;
    return 0;
}

void wifi67_emlps_deinit(struct wifi67_priv *priv)
{
    struct wifi67_emlps *emlps = priv->emlps;

    if (!emlps)
        return;

    cancel_delayed_work_sync(&emlps->ps_work);
    pm_qos_remove_request(&emlps->pm_qos);
    kfree(emlps);
    priv->emlps = NULL;
}

static void wifi67_emlps_work_handler(struct work_struct *work)
{
    struct wifi67_emlps *emlps = container_of(to_delayed_work(work),
                                            struct wifi67_emlps,
                                            ps_work);
    unsigned long flags;
    ktime_t now = ktime_get();
    s64 idle_time;

    spin_lock_irqsave(&emlps->lock, flags);

    if (emlps->state != WIFI67_EMLPS_ENABLED || emlps->force_active)
        goto reschedule;

    idle_time = ktime_ms_delta(now, emlps->last_activity);
    
    if (idle_time >= emlps->timeout) {
        u32 links = emlps->active_links;
        while (links) {
            int link = ffs(links) - 1;
            wifi67_hw_link_ps_enter(priv, link);
            links &= ~BIT(link);
        }
        pm_qos_update_request(&emlps->pm_qos, PM_QOS_DEFAULT_VALUE);
    }

reschedule:
    spin_unlock_irqrestore(&emlps->lock, flags);
    schedule_delayed_work(&emlps->ps_work,
                         msecs_to_jiffies(emlps->timeout));
}

int wifi67_emlps_enable(struct wifi67_priv *priv)
{
    struct wifi67_emlps *emlps = priv->emlps;
    unsigned long flags;
    int ret = 0;

    if (!emlps)
        return -EINVAL;

    spin_lock_irqsave(&emlps->lock, flags);

    if (emlps->state != WIFI67_EMLPS_DISABLED) {
        ret = -EBUSY;
        goto out;
    }

    emlps->state = WIFI67_EMLPS_ENABLED;
    emlps->last_activity = ktime_get();
    schedule_delayed_work(&emlps->ps_work,
                         msecs_to_jiffies(emlps->timeout));

out:
    spin_unlock_irqrestore(&emlps->lock, flags);
    return ret;
}

void wifi67_emlps_disable(struct wifi67_priv *priv)
{
    struct wifi67_emlps *emlps = priv->emlps;
    unsigned long flags;
    u32 links;

    if (!emlps)
        return;

    spin_lock_irqsave(&emlps->lock, flags);

    if (emlps->state == WIFI67_EMLPS_ENABLED) {
        emlps->state = WIFI67_EMLPS_DISABLED;
        cancel_delayed_work(&emlps->ps_work);

        links = emlps->active_links;
        while (links) {
            int link = ffs(links) - 1;
            wifi67_hw_link_ps_exit(priv, link);
            links &= ~BIT(link);
        }
    }

    pm_qos_update_request(&emlps->pm_qos, 0);
    spin_unlock_irqrestore(&emlps->lock, flags);
}

void wifi67_emlps_activity(struct wifi67_priv *priv, u8 link_id)
{
    struct wifi67_emlps *emlps = priv->emlps;
    unsigned long flags;

    if (!emlps || emlps->state != WIFI67_EMLPS_ENABLED)
        return;

    spin_lock_irqsave(&emlps->lock, flags);
    emlps->last_activity = ktime_get();
    
    if (!(emlps->active_links & BIT(link_id))) {
        wifi67_hw_link_ps_exit(priv, link_id);
        emlps->active_links |= BIT(link_id);
        pm_qos_update_request(&emlps->pm_qos, 0);
    }
    
    spin_unlock_irqrestore(&emlps->lock, flags);
}

int wifi67_emlps_set_params(struct wifi67_priv *priv,
                          struct wifi67_emlps_params *params)
{
    struct wifi67_emlps *emlps = priv->emlps;
    unsigned long flags;

    if (!emlps || !params)
        return -EINVAL;

    spin_lock_irqsave(&emlps->lock, flags);
    emlps->timeout = params->timeout;
    emlps->awake_window = params->awake_window;
    emlps->force_active = params->force_active;
    spin_unlock_irqrestore(&emlps->lock, flags);

    return 0;
}

EXPORT_SYMBOL(wifi67_emlps_init);
EXPORT_SYMBOL(wifi67_emlps_deinit);
EXPORT_SYMBOL(wifi67_emlps_enable);
EXPORT_SYMBOL(wifi67_emlps_disable);
EXPORT_SYMBOL(wifi67_emlps_activity);
EXPORT_SYMBOL(wifi67_emlps_set_params); 