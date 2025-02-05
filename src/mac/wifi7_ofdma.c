/*
 * WiFi 7 OFDMA Resource Unit Management
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
#include "wifi7_ofdma.h"
#include "wifi7_mac.h"

/* RU tone counts for each type */
static const u16 ru_tone_counts[] = {
    26,    /* 26-tone RU */
    52,    /* 52-tone RU */
    106,   /* 106-tone RU */
    242,   /* 242-tone RU */
    484,   /* 484-tone RU */
    996,   /* 996-tone RU */
    1992,  /* 2x996-tone RU */
    3984   /* 4x996-tone RU */
};

/* Helper functions */
static inline u16 get_ru_tones(u8 type)
{
    if (type >= ARRAY_SIZE(ru_tone_counts))
        return 0;
        
    return ru_tone_counts[type];
}

static inline bool is_ru_valid(struct wifi7_ofdma_ru *ru)
{
    if (ru->type >= WIFI7_OFDMA_MAX_RU)
        return false;
        
    if (ru->tones != get_ru_tones(ru->type))
        return false;
        
    if (ru->end_tone <= ru->start_tone)
        return false;
        
    return true;
}

static inline bool is_user_valid(struct wifi7_ofdma_user *user)
{
    if (user->spatial_streams > WIFI7_OFDMA_MAX_SS)
        return false;
        
    if (user->mcs >= WIFI7_OFDMA_MAX_MCS)
        return false;
        
    if (user->power >= WIFI7_OFDMA_MAX_POWER)
        return false;
        
    return true;
}

/* Resource allocation */
static int wifi7_ofdma_alloc_ru_fixed(struct wifi7_ofdma *ofdma,
                                    struct wifi7_ofdma_alloc *alloc)
{
    int i, j;
    u16 total_tones = 0;
    u8 current_tone = 0;
    
    /* Calculate total tones needed */
    for (i = 0; i < alloc->num_rus; i++) {
        struct wifi7_ofdma_ru *ru = &alloc->rus[i];
        total_tones += get_ru_tones(ru->type);
    }
    
    /* Check if allocation is possible */
    if (total_tones > 3984) /* Max for 320 MHz */
        return -EINVAL;
        
    /* Assign tone ranges */
    for (i = 0; i < alloc->num_rus; i++) {
        struct wifi7_ofdma_ru *ru = &alloc->rus[i];
        
        ru->start_tone = current_tone;
        ru->tones = get_ru_tones(ru->type);
        ru->end_tone = ru->start_tone + ru->tones - 1;
        
        current_tone += ru->tones;
        
        /* Check for overlaps */
        for (j = 0; j < i; j++) {
            struct wifi7_ofdma_ru *other = &alloc->rus[j];
            if (ru->start_tone <= other->end_tone &&
                other->start_tone <= ru->end_tone)
                return -EINVAL;
        }
    }
    
    return 0;
}

static int wifi7_ofdma_alloc_ru_dynamic(struct wifi7_ofdma *ofdma,
                                      struct wifi7_ofdma_alloc *alloc)
{
    int i;
    u8 ru_sizes[WIFI7_OFDMA_MAX_RU] = {0};
    u16 total_tones = 0;
    
    /* Count RU sizes needed */
    for (i = 0; i < alloc->num_users; i++) {
        struct wifi7_ofdma_user *user = &alloc->users[i];
        u8 ru_type;
        
        /* Determine RU size based on MCS and spatial streams */
        if (user->mcs >= 10 && user->spatial_streams >= 4)
            ru_type = WIFI7_OFDMA_RU_996;
        else if (user->mcs >= 8 && user->spatial_streams >= 2)
            ru_type = WIFI7_OFDMA_RU_484;
        else if (user->mcs >= 6)
            ru_type = WIFI7_OFDMA_RU_242;
        else if (user->mcs >= 4)
            ru_type = WIFI7_OFDMA_RU_106;
        else
            ru_type = WIFI7_OFDMA_RU_52;
            
        ru_sizes[ru_type]++;
        total_tones += get_ru_tones(ru_type);
    }
    
    /* Check if allocation is possible */
    if (total_tones > 3984)
        return -EINVAL;
        
    /* Create RUs */
    alloc->num_rus = 0;
    for (i = 0; i < WIFI7_OFDMA_MAX_RU; i++) {
        while (ru_sizes[i]--) {
            struct wifi7_ofdma_ru *ru = &alloc->rus[alloc->num_rus++];
            ru->type = i;
            ru->index = alloc->num_rus - 1;
            ru->tones = get_ru_tones(i);
            ru->flags = alloc->flags;
        }
    }
    
    /* Assign tone ranges */
    return wifi7_ofdma_alloc_ru_fixed(ofdma, alloc);
}

/* User management */
static int wifi7_ofdma_assign_users(struct wifi7_ofdma *ofdma,
                                  struct wifi7_ofdma_alloc *alloc)
{
    int i;
    u8 current_ru = 0;
    
    for (i = 0; i < alloc->num_users; i++) {
        struct wifi7_ofdma_user *user = &alloc->users[i];
        
        if (current_ru >= alloc->num_rus)
            return -ENOSPC;
            
        user->ru_index = current_ru++;
    }
    
    return 0;
}

/* Trigger frame generation */
static int wifi7_ofdma_gen_trigger(struct wifi7_ofdma *ofdma,
                                 struct wifi7_ofdma_trigger *trigger,
                                 struct wifi7_ofdma_alloc *alloc)
{
    int i;
    
    trigger->type = alloc->flags & WIFI7_OFDMA_FLAG_UL ?
                   WIFI7_OFDMA_FLAG_UL : WIFI7_OFDMA_FLAG_DL;
    trigger->num_users = alloc->num_users;
    trigger->duration = alloc->duration;
    trigger->cs_required = 1;
    trigger->mu_mimo = alloc->flags & WIFI7_OFDMA_FLAG_MU ? 1 : 0;
    trigger->gi_ltf = 0; /* TODO: Set based on mode */
    trigger->ru_allocation = 0; /* TODO: Encode RU allocation */
    trigger->flags = alloc->flags;
    
    /* Copy user info */
    for (i = 0; i < alloc->num_users; i++)
        trigger->users[i] = alloc->users[i];
        
    return 0;
}

/* Scheduling work */
static void wifi7_ofdma_schedule_work(struct work_struct *work)
{
    struct wifi7_ofdma *ofdma = container_of(to_delayed_work(work),
                                           struct wifi7_ofdma,
                                           schedule_work);
    struct wifi7_ofdma_alloc *alloc = &ofdma->current_alloc;
    unsigned long flags;
    int ret;
    
    spin_lock_irqsave(&ofdma->alloc_lock, flags);
    
    /* Allocate RUs */
    if (alloc->flags & WIFI7_OFDMA_FLAG_DYNAMIC)
        ret = wifi7_ofdma_alloc_ru_dynamic(ofdma, alloc);
    else
        ret = wifi7_ofdma_alloc_ru_fixed(ofdma, alloc);
        
    if (ret)
        goto out_unlock;
        
    /* Assign users */
    ret = wifi7_ofdma_assign_users(ofdma, alloc);
    if (ret)
        goto out_unlock;
        
    /* Generate trigger frame if needed */
    if (alloc->flags & (WIFI7_OFDMA_FLAG_UL | WIFI7_OFDMA_FLAG_TRIGGER)) {
        spin_lock(&ofdma->trigger_lock);
        ret = wifi7_ofdma_gen_trigger(ofdma, &ofdma->trigger, alloc);
        spin_unlock(&ofdma->trigger_lock);
    }
    
out_unlock:
    spin_unlock_irqrestore(&ofdma->alloc_lock, flags);
    
    /* Schedule next run */
    schedule_delayed_work(&ofdma->schedule_work, HZ/10);
}

/* Public API Implementation */
int wifi7_ofdma_init(struct wifi7_dev *dev)
{
    struct wifi7_ofdma *ofdma;
    int ret;
    
    ofdma = kzalloc(sizeof(*ofdma), GFP_KERNEL);
    if (!ofdma)
        return -ENOMEM;
        
    /* Set capabilities */
    ofdma->capabilities = WIFI7_OFDMA_CAP_UL |
                         WIFI7_OFDMA_CAP_DL |
                         WIFI7_OFDMA_CAP_MU |
                         WIFI7_OFDMA_CAP_TRIGGER |
                         WIFI7_OFDMA_CAP_PUNCTURE |
                         WIFI7_OFDMA_CAP_DYNAMIC |
                         WIFI7_OFDMA_CAP_ADAPTIVE |
                         WIFI7_OFDMA_CAP_FEEDBACK |
                         WIFI7_OFDMA_CAP_POWER |
                         WIFI7_OFDMA_CAP_SPATIAL |
                         WIFI7_OFDMA_CAP_QOS |
                         WIFI7_OFDMA_CAP_MULTI_TID;
                         
    /* Set configuration */
    ofdma->max_users = WIFI7_OFDMA_MAX_USERS;
    ofdma->max_rus = WIFI7_OFDMA_MAX_RU;
    ofdma->min_ru_size = WIFI7_OFDMA_RU_26;
    ofdma->max_ru_size = WIFI7_OFDMA_RU_4x996;
    
    /* Initialize locks */
    spin_lock_init(&ofdma->alloc_lock);
    spin_lock_init(&ofdma->trigger_lock);
    
    /* Create workqueue */
    ofdma->wq = create_singlethread_workqueue("wifi7_ofdma");
    if (!ofdma->wq) {
        ret = -ENOMEM;
        goto err_free_ofdma;
    }
    
    /* Initialize work */
    INIT_DELAYED_WORK(&ofdma->schedule_work, wifi7_ofdma_schedule_work);
    
    dev->ofdma = ofdma;
    return 0;
    
err_free_ofdma:
    kfree(ofdma);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_ofdma_init);

void wifi7_ofdma_deinit(struct wifi7_dev *dev)
{
    struct wifi7_ofdma *ofdma = dev->ofdma;
    
    if (!ofdma)
        return;
        
    /* Cancel work */
    cancel_delayed_work_sync(&ofdma->schedule_work);
    
    /* Destroy workqueue */
    destroy_workqueue(ofdma->wq);
    
    kfree(ofdma);
    dev->ofdma = NULL;
}
EXPORT_SYMBOL_GPL(wifi7_ofdma_deinit);

int wifi7_ofdma_alloc_ru(struct wifi7_dev *dev,
                        struct wifi7_ofdma_alloc *alloc)
{
    struct wifi7_ofdma *ofdma = dev->ofdma;
    unsigned long flags;
    int ret;
    
    if (!ofdma || !alloc)
        return -EINVAL;
        
    spin_lock_irqsave(&ofdma->alloc_lock, flags);
    
    /* Validate allocation */
    if (alloc->num_rus > ofdma->max_rus ||
        alloc->num_users > ofdma->max_users) {
        ret = -EINVAL;
        goto out_unlock;
    }
    
    /* Copy allocation */
    memcpy(&ofdma->current_alloc, alloc, sizeof(*alloc));
    
    /* Start scheduling */
    schedule_delayed_work(&ofdma->schedule_work, 0);
    
    ret = 0;
    
out_unlock:
    spin_unlock_irqrestore(&ofdma->alloc_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_ofdma_alloc_ru);

void wifi7_ofdma_free_ru(struct wifi7_dev *dev,
                        struct wifi7_ofdma_alloc *alloc)
{
    struct wifi7_ofdma *ofdma = dev->ofdma;
    unsigned long flags;
    
    if (!ofdma || !alloc)
        return;
        
    spin_lock_irqsave(&ofdma->alloc_lock, flags);
    
    /* Clear allocation */
    if (memcmp(&ofdma->current_alloc, alloc, sizeof(*alloc)) == 0)
        memset(&ofdma->current_alloc, 0, sizeof(ofdma->current_alloc));
        
    spin_unlock_irqrestore(&ofdma->alloc_lock, flags);
}
EXPORT_SYMBOL_GPL(wifi7_ofdma_free_ru);

int wifi7_ofdma_add_user(struct wifi7_dev *dev,
                        struct wifi7_ofdma_user *user)
{
    struct wifi7_ofdma *ofdma = dev->ofdma;
    unsigned long flags;
    int ret;
    
    if (!ofdma || !user)
        return -EINVAL;
        
    spin_lock_irqsave(&ofdma->alloc_lock, flags);
    
    /* Validate user */
    if (!is_user_valid(user)) {
        ret = -EINVAL;
        goto out_unlock;
    }
    
    /* Add user */
    if (ofdma->current_alloc.num_users >= ofdma->max_users) {
        ret = -ENOSPC;
        goto out_unlock;
    }
    
    memcpy(&ofdma->current_alloc.users[ofdma->current_alloc.num_users++],
           user, sizeof(*user));
           
    ret = 0;
    
out_unlock:
    spin_unlock_irqrestore(&ofdma->alloc_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_ofdma_add_user);

void wifi7_ofdma_del_user(struct wifi7_dev *dev, u8 user_id)
{
    struct wifi7_ofdma *ofdma = dev->ofdma;
    unsigned long flags;
    int i;
    
    if (!ofdma)
        return;
        
    spin_lock_irqsave(&ofdma->alloc_lock, flags);
    
    /* Find and remove user */
    for (i = 0; i < ofdma->current_alloc.num_users; i++) {
        if (ofdma->current_alloc.users[i].user_id == user_id) {
            memmove(&ofdma->current_alloc.users[i],
                   &ofdma->current_alloc.users[i + 1],
                   (ofdma->current_alloc.num_users - i - 1) *
                   sizeof(struct wifi7_ofdma_user));
            ofdma->current_alloc.num_users--;
            break;
        }
    }
    
    spin_unlock_irqrestore(&ofdma->alloc_lock, flags);
}
EXPORT_SYMBOL_GPL(wifi7_ofdma_del_user);

/* Module initialization */
static int __init wifi7_ofdma_init_module(void)
{
    pr_info("WiFi 7 OFDMA initialized\n");
    return 0;
}

static void __exit wifi7_ofdma_exit_module(void)
{
    pr_info("WiFi 7 OFDMA unloaded\n");
}

module_init(wifi7_ofdma_init_module);
module_exit(wifi7_ofdma_exit_module);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 OFDMA");
MODULE_VERSION("1.0"); 