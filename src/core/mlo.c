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

int wifi67_mlo_map_tid(struct wifi67_mlo_link *link, u8 tid,
                      u8 primary_link, u8 secondary_links)
{
    unsigned long flags;

    if (!link || tid >= WIFI67_MLO_MAX_TIDS)
        return -EINVAL;

    if (primary_link >= link->priv->hw_cap.max_mlo_links)
        return -EINVAL;

    spin_lock_irqsave(&link->priv->mlo_lock, flags);
    link->tid_maps[tid].primary_link = primary_link;
    link->tid_maps[tid].secondary_links = secondary_links;
    link->tid_maps[tid].flags |= WIFI67_MLO_LINK_FLAG_ACTIVE;
    spin_unlock_irqrestore(&link->priv->mlo_lock, flags);

    return 0;
}

int wifi67_mlo_unmap_tid(struct wifi67_mlo_link *link, u8 tid)
{
    unsigned long flags;

    if (!link || tid >= WIFI67_MLO_MAX_TIDS)
        return -EINVAL;

    spin_lock_irqsave(&link->priv->mlo_lock, flags);
    link->tid_maps[tid].primary_link = 0;
    link->tid_maps[tid].secondary_links = 0;
    link->tid_maps[tid].flags &= ~WIFI67_MLO_LINK_FLAG_ACTIVE;
    spin_unlock_irqrestore(&link->priv->mlo_lock, flags);

    return 0;
}

u8 wifi67_mlo_get_link_for_tid(struct wifi67_mlo_link *link, u8 tid)
{
    u8 target_link;
    unsigned long flags;

    if (!link || tid >= WIFI67_MLO_MAX_TIDS)
        return 0;

    spin_lock_irqsave(&link->priv->mlo_lock, flags);
    
    if (!(link->tid_maps[tid].flags & WIFI67_MLO_LINK_FLAG_ACTIVE)) {
        target_link = link->link_id;
    } else {
        target_link = link->tid_maps[tid].primary_link;
        
        if (link->tid_maps[tid].secondary_links != 0) {
            u8 active_links = link->tid_maps[tid].secondary_links;
            u8 num_active = hweight8(active_links);
            
            if (num_active > 0) {
                u8 selected = prandom_u32() % (num_active + 1);
                if (selected > 0) {
                    int i;
                    for_each_set_bit(i, (unsigned long *)&active_links, 8) {
                        selected--;
                        if (selected == 0) {
                            target_link = i;
                            break;
                        }
                    }
                }
            }
        }
    }
    
    spin_unlock_irqrestore(&link->priv->mlo_lock, flags);
    return target_link;
} 