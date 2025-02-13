#include <linux/ieee80211.h>
#include <net/mac80211.h>
#include "../../include/core/wifi67.h"
#include "../../include/core/mlo.h"

struct wifi67_mlo_link *wifi67_mlo_alloc_link(struct wifi67_priv *priv)
{
    struct wifi67_mlo_link *link;
    
    link = kzalloc(sizeof(*link), GFP_KERNEL);
    if (!link)
        return NULL;
        
    link->priv = priv;
    INIT_LIST_HEAD(&link->list);
    return link;
}

int wifi67_mlo_setup_link(struct wifi67_priv *priv, struct wifi67_mlo_link *link,
                         u8 link_id, u8 band)
{
    if (link_id >= priv->hw_cap.max_mlo_links)
        return -EINVAL;
        
    link->link_id = link_id;
    link->band = band;
    link->state = WIFI67_MLO_LINK_SETUP;
    
    list_add_tail(&link->list, &priv->mlo_links);
    return 0;
}

void wifi67_mlo_remove_link(struct wifi67_mlo_link *link)
{
    list_del(&link->list);
    kfree(link);
}

int wifi67_mlo_init(struct wifi67_priv *priv)
{
    INIT_LIST_HEAD(&priv->mlo_links);
    spin_lock_init(&priv->mlo_lock);
    return 0;
}

void wifi67_mlo_deinit(struct wifi67_priv *priv)
{
    struct wifi67_mlo_link *link, *tmp;
    
    list_for_each_entry_safe(link, tmp, &priv->mlo_links, list)
        wifi67_mlo_remove_link(link);
}

int wifi67_mlo_activate_link(struct wifi67_mlo_link *link)
{
    if (!link || link->state != WIFI67_MLO_LINK_SETUP)
        return -EINVAL;

    link->state = WIFI67_MLO_LINK_ACTIVE;
    link->flags |= WIFI67_MLO_LINK_FLAG_ACTIVE;
    return 0;
}

int wifi67_mlo_deactivate_link(struct wifi67_mlo_link *link)
{
    if (!link || link->state != WIFI67_MLO_LINK_ACTIVE)
        return -EINVAL;

    link->state = WIFI67_MLO_LINK_IDLE;
    link->flags &= ~WIFI67_MLO_LINK_FLAG_ACTIVE;
    return 0;
}

int wifi67_mlo_handle_link_error(struct wifi67_mlo_link *link)
{
    unsigned long flags;
    int ret = 0;

    if (!link)
        return -EINVAL;

    spin_lock_irqsave(&link->priv->mlo_lock, flags);
    
    if (link->state == WIFI67_MLO_LINK_ERROR) {
        ret = -EALREADY;
        goto out;
    }

    link->state = WIFI67_MLO_LINK_ERROR;
    link->flags |= WIFI67_MLO_LINK_FLAG_ERROR;
    
    if (link->flags & WIFI67_MLO_LINK_FLAG_ACTIVE)
        link->flags &= ~WIFI67_MLO_LINK_FLAG_ACTIVE;

out:
    spin_unlock_irqrestore(&link->priv->mlo_lock, flags);
    return ret;
}

struct wifi67_mlo_link *wifi67_mlo_get_link_by_id(struct wifi67_priv *priv,
                                                 u8 link_id)
{
    struct wifi67_mlo_link *link;

    list_for_each_entry(link, &priv->mlo_links, list) {
        if (link->link_id == link_id)
            return link;
    }
    return NULL;
} 