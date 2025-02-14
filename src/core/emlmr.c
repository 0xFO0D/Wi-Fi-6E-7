#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include "../../include/core/wifi67.h"
#include "../../include/core/emlmr.h"

struct wifi67_emlmr {
    spinlock_t lock;
    u8 state;
    u32 radio_map;
    u32 active_links;
    struct {
        u8 link_id;
        u8 radio_id;
        bool active;
    } link_radio_map[WIFI67_MAX_LINKS];
    u32 transition_delay;
    struct delayed_work switch_work;
};

static struct wifi67_emlmr *emlmr_alloc(void)
{
    struct wifi67_emlmr *emlmr;

    emlmr = kzalloc(sizeof(*emlmr), GFP_KERNEL);
    if (!emlmr)
        return NULL;

    spin_lock_init(&emlmr->lock);
    INIT_DELAYED_WORK(&emlmr->switch_work, wifi67_emlmr_switch_handler);
    return emlmr;
}

int wifi67_emlmr_init(struct wifi67_priv *priv)
{
    struct wifi67_emlmr *emlmr;
    int i;

    emlmr = emlmr_alloc();
    if (!emlmr)
        return -ENOMEM;

    emlmr->state = WIFI67_EMLMR_DISABLED;
    emlmr->radio_map = 0;
    emlmr->active_links = 0;
    emlmr->transition_delay = WIFI67_EMLMR_DEFAULT_DELAY;

    for (i = 0; i < WIFI67_MAX_LINKS; i++) {
        emlmr->link_radio_map[i].link_id = i;
        emlmr->link_radio_map[i].radio_id = WIFI67_INVALID_RADIO;
        emlmr->link_radio_map[i].active = false;
    }

    priv->emlmr = emlmr;
    return 0;
}

void wifi67_emlmr_deinit(struct wifi67_priv *priv)
{
    struct wifi67_emlmr *emlmr = priv->emlmr;

    if (!emlmr)
        return;

    cancel_delayed_work_sync(&emlmr->switch_work);
    kfree(emlmr);
    priv->emlmr = NULL;
}

static void wifi67_emlmr_switch_handler(struct work_struct *work)
{
    struct wifi67_emlmr *emlmr = container_of(to_delayed_work(work),
                                            struct wifi67_emlmr,
                                            switch_work);
    unsigned long flags;
    int i;

    spin_lock_irqsave(&emlmr->lock, flags);

    if (emlmr->state != WIFI67_EMLMR_ENABLED)
        goto out;

    for (i = 0; i < WIFI67_MAX_LINKS; i++) {
        if (!emlmr->link_radio_map[i].active)
            continue;

        if (wifi67_hw_check_link_quality(priv, i) < WIFI67_LINK_QUALITY_THRESHOLD) {
            u8 new_radio = wifi67_hw_find_best_radio(priv, i);
            if (new_radio != WIFI67_INVALID_RADIO && 
                new_radio != emlmr->link_radio_map[i].radio_id) {
                wifi67_hw_switch_link_radio(priv, i, new_radio);
                emlmr->link_radio_map[i].radio_id = new_radio;
            }
        }
    }

out:
    spin_unlock_irqrestore(&emlmr->lock, flags);
    schedule_delayed_work(&emlmr->switch_work,
                         msecs_to_jiffies(emlmr->transition_delay));
}

int wifi67_emlmr_enable(struct wifi67_priv *priv)
{
    struct wifi67_emlmr *emlmr = priv->emlmr;
    unsigned long flags;
    int ret = 0;

    if (!emlmr)
        return -EINVAL;

    spin_lock_irqsave(&emlmr->lock, flags);

    if (emlmr->state != WIFI67_EMLMR_DISABLED) {
        ret = -EBUSY;
        goto out;
    }

    emlmr->state = WIFI67_EMLMR_ENABLED;
    schedule_delayed_work(&emlmr->switch_work,
                         msecs_to_jiffies(emlmr->transition_delay));

out:
    spin_unlock_irqrestore(&emlmr->lock, flags);
    return ret;
}

void wifi67_emlmr_disable(struct wifi67_priv *priv)
{
    struct wifi67_emlmr *emlmr = priv->emlmr;
    unsigned long flags;
    int i;

    if (!emlmr)
        return;

    spin_lock_irqsave(&emlmr->lock, flags);

    if (emlmr->state == WIFI67_EMLMR_ENABLED) {
        emlmr->state = WIFI67_EMLMR_DISABLED;
        cancel_delayed_work(&emlmr->switch_work);

        for (i = 0; i < WIFI67_MAX_LINKS; i++) {
            if (emlmr->link_radio_map[i].active) {
                wifi67_hw_disable_link_radio(priv, i);
                emlmr->link_radio_map[i].active = false;
                emlmr->link_radio_map[i].radio_id = WIFI67_INVALID_RADIO;
            }
        }
        emlmr->active_links = 0;
    }

    spin_unlock_irqrestore(&emlmr->lock, flags);
}

int wifi67_emlmr_add_link(struct wifi67_priv *priv, u8 link_id, u8 radio_id)
{
    struct wifi67_emlmr *emlmr = priv->emlmr;
    unsigned long flags;
    int ret = 0;

    if (!emlmr || link_id >= WIFI67_MAX_LINKS || 
        radio_id >= WIFI67_MAX_RADIOS)
        return -EINVAL;

    spin_lock_irqsave(&emlmr->lock, flags);

    if (emlmr->link_radio_map[link_id].active) {
        ret = -EEXIST;
        goto out;
    }

    ret = wifi67_hw_enable_link_radio(priv, link_id, radio_id);
    if (ret)
        goto out;

    emlmr->link_radio_map[link_id].radio_id = radio_id;
    emlmr->link_radio_map[link_id].active = true;
    emlmr->active_links |= BIT(link_id);
    emlmr->radio_map |= BIT(radio_id);

out:
    spin_unlock_irqrestore(&emlmr->lock, flags);
    return ret;
}

int wifi67_emlmr_remove_link(struct wifi67_priv *priv, u8 link_id)
{
    struct wifi67_emlmr *emlmr = priv->emlmr;
    unsigned long flags;
    int ret = 0;

    if (!emlmr || link_id >= WIFI67_MAX_LINKS)
        return -EINVAL;

    spin_lock_irqsave(&emlmr->lock, flags);

    if (!emlmr->link_radio_map[link_id].active) {
        ret = -ENOENT;
        goto out;
    }

    wifi67_hw_disable_link_radio(priv, link_id);
    emlmr->link_radio_map[link_id].active = false;
    emlmr->link_radio_map[link_id].radio_id = WIFI67_INVALID_RADIO;
    emlmr->active_links &= ~BIT(link_id);

out:
    spin_unlock_irqrestore(&emlmr->lock, flags);
    return ret;
}

int wifi67_emlmr_get_link_radio(struct wifi67_priv *priv, u8 link_id)
{
    struct wifi67_emlmr *emlmr = priv->emlmr;
    unsigned long flags;
    int radio_id;

    if (!emlmr || link_id >= WIFI67_MAX_LINKS)
        return -EINVAL;

    spin_lock_irqsave(&emlmr->lock, flags);
    radio_id = emlmr->link_radio_map[link_id].active ?
               emlmr->link_radio_map[link_id].radio_id :
               WIFI67_INVALID_RADIO;
    spin_unlock_irqrestore(&emlmr->lock, flags);

    return radio_id;
}

EXPORT_SYMBOL(wifi67_emlmr_init);
EXPORT_SYMBOL(wifi67_emlmr_deinit);
EXPORT_SYMBOL(wifi67_emlmr_enable);
EXPORT_SYMBOL(wifi67_emlmr_disable);
EXPORT_SYMBOL(wifi67_emlmr_add_link);
EXPORT_SYMBOL(wifi67_emlmr_remove_link);
EXPORT_SYMBOL(wifi67_emlmr_get_link_radio); 