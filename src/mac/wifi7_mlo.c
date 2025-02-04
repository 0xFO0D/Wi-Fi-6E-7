#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include "wifi7_mlo.h"
#include "wifi7_mac.h"

int wifi7_mlo_init(struct wifi7_dev *dev)
{
    struct wifi7_mlo_info *mlo;

    mlo = kzalloc(sizeof(*mlo), GFP_KERNEL);
    if (!mlo)
        return -ENOMEM;

    spin_lock_init(&mlo->lock);
    mlo->n_links = 0;
    dev->mlo = mlo;

    return 0;
}

void wifi7_mlo_deinit(struct wifi7_dev *dev)
{
    if (dev->mlo) {
        kfree(dev->mlo);
        dev->mlo = NULL;
    }
}

int wifi7_mlo_setup_link(struct wifi7_dev *dev, u8 link_id, u16 freq)
{
    struct wifi7_mlo_info *mlo = dev->mlo;
    unsigned long flags;
    int ret = 0;

    if (!mlo || link_id >= MLO_MAX_LINKS)
        return -EINVAL;

    spin_lock_irqsave(&mlo->lock, flags);

    if (mlo->n_links >= MLO_MAX_LINKS) {
        ret = -ENOSPC;
        goto out;
    }

    mlo->links[link_id].link_id = link_id;
    mlo->links[link_id].freq = freq;
    mlo->links[link_id].state = 0;
    mlo->n_links++;

    if (wifi7_mac_setup_link(dev, link_id, freq))
        ret = -EFAULT;

out:
    spin_unlock_irqrestore(&mlo->lock, flags);
    return ret;
}

int wifi7_mlo_update_tid_map(struct wifi7_dev *dev, u8 tid, u16 link_mask)
{
    struct wifi7_mlo_info *mlo = dev->mlo;
    unsigned long flags;

    if (!mlo || tid >= 8)
        return -EINVAL;

    spin_lock_irqsave(&mlo->lock, flags);
    mlo->tid_to_link_map[tid] = link_mask;
    spin_unlock_irqrestore(&mlo->lock, flags);

    return 0;
}

int wifi7_mlo_get_link_state(struct wifi7_dev *dev, u8 link_id)
{
    struct wifi7_mlo_info *mlo = dev->mlo;
    unsigned long flags;
    int state;

    if (!mlo || link_id >= MLO_MAX_LINKS)
        return -EINVAL;

    spin_lock_irqsave(&mlo->lock, flags);
    state = mlo->links[link_id].state;
    spin_unlock_irqrestore(&mlo->lock, flags);

    return state;
}

int wifi7_mlo_set_link_state(struct wifi7_dev *dev, u8 link_id, u8 state)
{
    struct wifi7_mlo_info *mlo = dev->mlo;
    unsigned long flags;

    if (!mlo || link_id >= MLO_MAX_LINKS)
        return -EINVAL;

    spin_lock_irqsave(&mlo->lock, flags);
    mlo->links[link_id].state = state;
    spin_unlock_irqrestore(&mlo->lock, flags);

    return 0;
} 