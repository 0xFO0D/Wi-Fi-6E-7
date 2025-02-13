#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include "../../include/core/wifi67.h"
#include "../../include/core/emlsr.h"

struct wifi67_emlsr {
    spinlock_t lock;
    u8 state;
    u8 active_link;
    u32 transition_delay;
    bool pad_enabled;
    struct delayed_work switch_work;
};

static struct wifi67_emlsr *emlsr_alloc(void)
{
    struct wifi67_emlsr *emlsr;

    emlsr = kzalloc(sizeof(*emlsr), GFP_KERNEL);
    if (!emlsr)
        return NULL;

    spin_lock_init(&emlsr->lock);
    INIT_DELAYED_WORK(&emlsr->switch_work, wifi67_emlsr_switch_handler);
    return emlsr;
}

int wifi67_emlsr_init(struct wifi67_priv *priv)
{
    struct wifi67_emlsr *emlsr;

    emlsr = emlsr_alloc();
    if (!emlsr)
        return -ENOMEM;

    emlsr->state = WIFI67_EMLSR_DISABLED;
    emlsr->transition_delay = 0;
    emlsr->pad_enabled = false;
    priv->emlsr = emlsr;

    return 0;
}

void wifi67_emlsr_deinit(struct wifi67_priv *priv)
{
    if (!priv->emlsr)
        return;

    cancel_delayed_work_sync(&priv->emlsr->switch_work);
    kfree(priv->emlsr);
    priv->emlsr = NULL;
}

static void wifi67_emlsr_switch_handler(struct work_struct *work)
{
    struct wifi67_emlsr *emlsr = container_of(to_delayed_work(work),
                                            struct wifi67_emlsr,
                                            switch_work);
    unsigned long flags;
    u8 next_link;

    spin_lock_irqsave(&emlsr->lock, flags);

    if (emlsr->state != WIFI67_EMLSR_ENABLED)
        goto out;

    next_link = (emlsr->active_link + 1) % WIFI67_MAX_LINKS;
    wifi67_hw_switch_link(priv, next_link);
    emlsr->active_link = next_link;

out:
    spin_unlock_irqrestore(&emlsr->lock, flags);
}

int wifi67_emlsr_enable(struct wifi67_priv *priv)
{
    struct wifi67_emlsr *emlsr = priv->emlsr;
    unsigned long flags;
    int ret = 0;

    if (!emlsr)
        return -EINVAL;

    spin_lock_irqsave(&emlsr->lock, flags);

    if (emlsr->state != WIFI67_EMLSR_DISABLED) {
        ret = -EBUSY;
        goto out;
    }

    emlsr->state = WIFI67_EMLSR_ENABLED;
    emlsr->active_link = 0;
    schedule_delayed_work(&emlsr->switch_work,
                         msecs_to_jiffies(emlsr->transition_delay));

out:
    spin_unlock_irqrestore(&emlsr->lock, flags);
    return ret;
}

void wifi67_emlsr_disable(struct wifi67_priv *priv)
{
    struct wifi67_emlsr *emlsr = priv->emlsr;
    unsigned long flags;

    if (!emlsr)
        return;

    spin_lock_irqsave(&emlsr->lock, flags);
    emlsr->state = WIFI67_EMLSR_DISABLED;
    cancel_delayed_work(&emlsr->switch_work);
    spin_unlock_irqrestore(&emlsr->lock, flags);
}

int wifi67_emlsr_set_params(struct wifi67_priv *priv,
                          struct wifi67_emlsr_params *params)
{
    struct wifi67_emlsr *emlsr = priv->emlsr;
    unsigned long flags;

    if (!emlsr || !params)
        return -EINVAL;

    spin_lock_irqsave(&emlsr->lock, flags);
    emlsr->transition_delay = params->transition_delay;
    emlsr->pad_enabled = params->pad_enabled;
    spin_unlock_irqrestore(&emlsr->lock, flags);

    return 0;
}

EXPORT_SYMBOL(wifi67_emlsr_init);
EXPORT_SYMBOL(wifi67_emlsr_deinit);
EXPORT_SYMBOL(wifi67_emlsr_enable);
EXPORT_SYMBOL(wifi67_emlsr_disable);
EXPORT_SYMBOL(wifi67_emlsr_set_params); 