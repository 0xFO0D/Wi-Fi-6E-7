/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ieee80211.h>
#include "wifi7_phy.h"
#include "wifi7_beamforming.h"
#include "wifi7_mu_mimo.h"

/* Forward declarations */
static void wifi7_mu_scheduling_work(struct work_struct *work);
static int wifi7_mu_check_compatibility(struct wifi7_mu_context *mu,
                                      struct wifi7_mu_group *group,
                                      const struct wifi7_mu_spatial_info *spatial);
static int wifi7_mu_optimize_streams(struct wifi7_mu_context *mu,
                                   struct wifi7_mu_group *group);

/* Allocate MU-MIMO context */
struct wifi7_mu_context *wifi7_mu_alloc(struct wifi7_phy_dev *phy,
                                       struct wifi7_bf_context *bf)
{
    struct wifi7_mu_context *mu;
    int i;

    if (!phy || !bf)
        return NULL;

    mu = kzalloc(sizeof(*mu), GFP_KERNEL);
    if (!mu)
        return NULL;

    mu->phy = phy;
    mu->bf = bf;
    spin_lock_init(&mu->group_lock);
    atomic_set(&mu->stats.groups_active, 0);

    /* Initialize groups */
    for (i = 0; i < WIFI7_MU_MAX_GROUPS; i++) {
        mu->groups[i].group_id = i;
        mu->groups[i].state = WIFI7_MU_GROUP_IDLE;
    }

    /* Create workqueue for scheduling */
    mu->mu_wq = alloc_workqueue("wifi7_mu_wq",
                               WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
    if (!mu->mu_wq)
        goto err_free_mu;

    INIT_DELAYED_WORK(&mu->sched_work, wifi7_mu_scheduling_work);

    return mu;

err_free_mu:
    kfree(mu);
    return NULL;
}
EXPORT_SYMBOL_GPL(wifi7_mu_alloc);

void wifi7_mu_free(struct wifi7_mu_context *mu)
{
    int i, j;

    if (!mu)
        return;

    /* Stop all groups */
    for (i = 0; i < WIFI7_MU_MAX_GROUPS; i++) {
        struct wifi7_mu_group *group = &mu->groups[i];
        
        for (j = 0; j < group->num_users; j++) {
            kfree(group->users[j].streams.stream_map);
        }
    }

    if (mu->mu_wq) {
        cancel_delayed_work_sync(&mu->sched_work);
        destroy_workqueue(mu->mu_wq);
    }

    kfree(mu);
}
EXPORT_SYMBOL_GPL(wifi7_mu_free);

/* Group management */
static int wifi7_mu_check_compatibility(struct wifi7_mu_context *mu,
                                      struct wifi7_mu_group *group,
                                      const struct wifi7_mu_spatial_info *spatial)
{
    int i;
    
    /* Basic signal quality checks */
    if (spatial->rssi < WIFI7_MU_MIN_RSSI_DB ||
        spatial->snr < WIFI7_MU_MIN_SNR_DB)
        return -EINVAL;

    /* Check spatial compatibility with existing users */
    for (i = 0; i < group->num_users; i++) {
        /* Correlation should be low for good spatial separation */
        if (spatial->metrics.correlation > 
            group->users[i].spatial.metrics.correlation)
            return -EINVAL;

        /* Inter-user isolation should be high */
        if (spatial->metrics.isolation < 
            group->users[i].spatial.metrics.isolation)
            return -EINVAL;
    }

    /* Check if adding this user exceeds spatial stream capacity */
    if (group->total_spatial_streams + spatial->metrics.rank >
        WIFI7_MU_MAX_SPATIAL_STREAMS)
        return -ENOSPC;

    return 0;
}

int wifi7_mu_group_add_user(struct wifi7_mu_context *mu,
                           u8 group_id, u8 aid,
                           const struct wifi7_mu_spatial_info *spatial)
{
    struct wifi7_mu_group *group;
    unsigned long flags;
    int i, ret = 0;

    if (!mu || !spatial || group_id >= WIFI7_MU_MAX_GROUPS)
        return -EINVAL;

    group = &mu->groups[group_id];
    
    spin_lock_irqsave(&mu->group_lock, flags);

    /* Check if user already exists */
    for (i = 0; i < group->num_users; i++) {
        if (group->users[i].aid == aid) {
            ret = -EEXIST;
            goto out_unlock;
        }
    }

    if (group->num_users >= WIFI7_MU_MAX_USERS_PER_GROUP) {
        ret = -ENOSPC;
        goto out_unlock;
    }

    /* Check spatial compatibility */
    ret = wifi7_mu_check_compatibility(mu, group, spatial);
    if (ret)
        goto out_unlock;

    /* Add new user */
    i = group->num_users++;
    group->users[i].aid = aid;
    group->users[i].ready = false;
    memcpy(&group->users[i].spatial, spatial,
           sizeof(group->users[i].spatial));

    /* Initialize stream allocation */
    group->users[i].streams.aid = aid;
    group->users[i].streams.num_streams = 0;
    group->users[i].streams.stream_map = NULL;

    /* Update group characteristics */
    group->total_spatial_streams += spatial->metrics.rank;
    if (group->num_users > 1) {
        group->dl_mu_mimo_ready = true;
        if (spatial->metrics.rank >= 2)
            group->ul_mu_mimo_ready = true;
    }

out_unlock:
    spin_unlock_irqrestore(&mu->group_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_mu_group_add_user);

int wifi7_mu_group_remove_user(struct wifi7_mu_context *mu,
                              u8 group_id, u8 aid)
{
    struct wifi7_mu_group *group;
    unsigned long flags;
    int i, j, ret = -ENOENT;

    if (!mu || group_id >= WIFI7_MU_MAX_GROUPS)
        return -EINVAL;

    group = &mu->groups[group_id];
    
    spin_lock_irqsave(&mu->group_lock, flags);

    /* Find and remove user */
    for (i = 0; i < group->num_users; i++) {
        if (group->users[i].aid == aid) {
            /* Update group characteristics */
            group->total_spatial_streams -=
                group->users[i].spatial.metrics.rank;

            /* Free resources */
            kfree(group->users[i].streams.stream_map);

            /* Shift remaining users */
            for (j = i + 1; j < group->num_users; j++)
                group->users[j - 1] = group->users[j];

            group->num_users--;
            
            /* Update MU-MIMO capability */
            if (group->num_users <= 1) {
                group->dl_mu_mimo_ready = false;
                group->ul_mu_mimo_ready = false;
            }

            ret = 0;
            break;
        }
    }

    spin_unlock_irqrestore(&mu->group_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_mu_group_remove_user);

/* Stream management */
static int wifi7_mu_optimize_streams(struct wifi7_mu_context *mu,
                                   struct wifi7_mu_group *group)
{
    int i, j;
    u8 total_streams = 0;
    
    /* First pass: calculate total streams needed */
    for (i = 0; i < group->num_users; i++) {
        struct wifi7_mu_stream_alloc *alloc = &group->users[i].streams;
        total_streams += alloc->num_streams;
    }
    
    if (total_streams > WIFI7_MU_MAX_SPATIAL_STREAMS)
        return -ENOSPC;

    /* Second pass: optimize stream mapping */
    for (i = 0; i < group->num_users; i++) {
        struct wifi7_mu_stream_alloc *alloc = &group->users[i].streams;
        
        /* Allocate stream map if needed */
        if (!alloc->stream_map) {
            alloc->stream_map = kzalloc(
                alloc->num_streams * sizeof(*alloc->stream_map),
                GFP_ATOMIC);
            if (!alloc->stream_map)
                return -ENOMEM;
        }

        /* Map streams to physical antennas */
        for (j = 0; j < alloc->num_streams; j++) {
            /* TODO: Implement proper antenna mapping
             * This needs hardware-specific optimization
             */
            alloc->stream_map[j].stream_idx = j;
            alloc->stream_map[j].antenna_mask = BIT(j);
        }
    }

    return 0;
}

int wifi7_mu_alloc_streams(struct wifi7_mu_context *mu,
                          u8 group_id, u8 aid,
                          u8 num_streams)
{
    struct wifi7_mu_group *group;
    unsigned long flags;
    int i, ret = -ENOENT;

    if (!mu || group_id >= WIFI7_MU_MAX_GROUPS ||
        num_streams > WIFI7_MU_MAX_SPATIAL_STREAMS)
        return -EINVAL;

    group = &mu->groups[group_id];
    
    spin_lock_irqsave(&mu->group_lock, flags);

    /* Find user */
    for (i = 0; i < group->num_users; i++) {
        if (group->users[i].aid == aid) {
            struct wifi7_mu_stream_alloc *alloc = &group->users[i].streams;
            
            /* Free existing allocation if any */
            kfree(alloc->stream_map);
            alloc->stream_map = NULL;
            
            /* Update stream count */
            alloc->num_streams = num_streams;
            
            /* Optimize stream allocation */
            ret = wifi7_mu_optimize_streams(mu, group);
            if (ret == 0)
                group->users[i].ready = true;
            
            break;
        }
    }

    spin_unlock_irqrestore(&mu->group_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_mu_alloc_streams);

/* Scheduling work */
static void wifi7_mu_scheduling_work(struct work_struct *work)
{
    struct wifi7_mu_context *mu = container_of(work, struct wifi7_mu_context,
                                             sched_work.work);
    int i;
    bool active = false;

    /* Check each group for scheduling */
    for (i = 0; i < WIFI7_MU_MAX_GROUPS; i++) {
        struct wifi7_mu_group *group = &mu->groups[i];
        unsigned long flags;
        bool can_schedule = true;

        spin_lock_irqsave(&mu->group_lock, flags);

        if (group->state != WIFI7_MU_GROUP_ACTIVE ||
            !time_after(jiffies, group->last_schedule +
                       usecs_to_jiffies(WIFI7_MU_MIN_SCHED_INTERVAL_US))) {
            spin_unlock_irqrestore(&mu->group_lock, flags);
            continue;
        }

        /* Check if all users are ready */
        for (i = 0; i < group->num_users; i++) {
            if (!group->users[i].ready) {
                can_schedule = false;
                break;
            }
        }

        if (!can_schedule) {
            spin_unlock_irqrestore(&mu->group_lock, flags);
            continue;
        }

        active = true;
        
        /* TODO: Implement actual scheduling
         * This requires hardware-specific implementation
         */

        spin_unlock_irqrestore(&mu->group_lock, flags);
    }

    /* Reschedule if there are active groups */
    if (active) {
        queue_delayed_work(mu->mu_wq, &mu->sched_work,
                          usecs_to_jiffies(WIFI7_MU_MIN_SCHED_INTERVAL_US));
    }
}

/* Group operations */
int wifi7_mu_group_start(struct wifi7_mu_context *mu, u8 group_id)
{
    struct wifi7_mu_group *group;
    unsigned long flags;
    int ret = 0;

    if (!mu || group_id >= WIFI7_MU_MAX_GROUPS)
        return -EINVAL;

    group = &mu->groups[group_id];
    
    spin_lock_irqsave(&mu->group_lock, flags);

    if (group->state == WIFI7_MU_GROUP_ACTIVE) {
        ret = -EBUSY;
        goto out_unlock;
    }

    if (group->num_users == 0) {
        ret = -EINVAL;
        goto out_unlock;
    }

    group->state = WIFI7_MU_GROUP_ACTIVE;
    atomic_inc(&mu->stats.groups_active);

    /* Start scheduling work */
    queue_delayed_work(mu->mu_wq, &mu->sched_work,
                      usecs_to_jiffies(WIFI7_MU_MIN_SCHED_INTERVAL_US));

out_unlock:
    spin_unlock_irqrestore(&mu->group_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_mu_group_start);

void wifi7_mu_group_stop(struct wifi7_mu_context *mu, u8 group_id)
{
    struct wifi7_mu_group *group;
    unsigned long flags;

    if (!mu || group_id >= WIFI7_MU_MAX_GROUPS)
        return;

    group = &mu->groups[group_id];
    
    spin_lock_irqsave(&mu->group_lock, flags);

    if (group->state == WIFI7_MU_GROUP_ACTIVE) {
        group->state = WIFI7_MU_GROUP_IDLE;
        atomic_dec(&mu->stats.groups_active);
    }

    spin_unlock_irqrestore(&mu->group_lock, flags);
}
EXPORT_SYMBOL_GPL(wifi7_mu_group_stop);

/* Transmission control */
int wifi7_mu_tx_prepare(struct wifi7_mu_context *mu,
                       u8 group_id,
                       struct sk_buff *skb)
{
    struct wifi7_mu_group *group;
    int ret = 0;

    if (!mu || !skb || group_id >= WIFI7_MU_MAX_GROUPS)
        return -EINVAL;

    group = &mu->groups[group_id];

    /* TODO: Implement MU-MIMO TX preparation
     * This requires hardware-specific implementation
     */

    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_mu_tx_prepare);

void wifi7_mu_tx_done(struct wifi7_mu_context *mu,
                      u8 group_id,
                      bool success)
{
    struct wifi7_mu_group *group;

    if (!mu || group_id >= WIFI7_MU_MAX_GROUPS)
        return;

    group = &mu->groups[group_id];

    if (success) {
        group->stats.successful_tx++;
        mu->stats.total_tx_success++;
    } else {
        group->stats.failed_tx++;
        mu->stats.total_tx_failed++;
    }
}
EXPORT_SYMBOL_GPL(wifi7_mu_tx_done);

/* Debug interface */
void wifi7_mu_dump_stats(struct wifi7_mu_context *mu)
{
    int i;

    if (!mu)
        return;

    pr_info("WiFi 7 MU-MIMO Statistics:\n");
    pr_info("Active groups: %d\n", atomic_read(&mu->stats.groups_active));
    pr_info("Total TX success: %u\n", mu->stats.total_tx_success);
    pr_info("Total TX failed: %u\n", mu->stats.total_tx_failed);
    pr_info("Spatial streams used: %u\n", mu->stats.spatial_streams_used);
    pr_info("Scheduling conflicts: %u\n", mu->stats.scheduling_conflicts);

    for (i = 0; i < WIFI7_MU_MAX_GROUPS; i++) {
        struct wifi7_mu_group *group = &mu->groups[i];
        
        if (group->num_users == 0)
            continue;

        pr_info("\nGroup %d:\n", i);
        pr_info("  Users: %d\n", group->num_users);
        pr_info("  State: %d\n", group->state);
        pr_info("  Total streams: %d\n", group->total_spatial_streams);
        pr_info("  DL MU-MIMO: %s\n", group->dl_mu_mimo_ready ? "yes" : "no");
        pr_info("  UL MU-MIMO: %s\n", group->ul_mu_mimo_ready ? "yes" : "no");
        pr_info("  Success: %u\n", group->stats.successful_tx);
        pr_info("  Failed: %u\n", group->stats.failed_tx);
        pr_info("  Collisions: %u\n", group->stats.collisions);
        pr_info("  Scheduling errors: %u\n", group->stats.scheduling_errors);
    }
}
EXPORT_SYMBOL_GPL(wifi7_mu_dump_stats);

void wifi7_mu_dump_group(struct wifi7_mu_context *mu, u8 group_id)
{
    struct wifi7_mu_group *group;
    int i, j;

    if (!mu || group_id >= WIFI7_MU_MAX_GROUPS)
        return;

    group = &mu->groups[group_id];

    pr_info("MU-MIMO Group %d Details:\n", group_id);
    pr_info("State: %d\n", group->state);
    pr_info("Users: %d\n", group->num_users);
    pr_info("Total spatial streams: %d\n", group->total_spatial_streams);

    for (i = 0; i < group->num_users; i++) {
        struct wifi7_mu_spatial_info *spatial = &group->users[i].spatial;
        struct wifi7_mu_stream_alloc *streams = &group->users[i].streams;

        pr_info("\nUser %d (AID %d):\n", i, group->users[i].aid);
        pr_info("  RSSI: %d dBm\n", spatial->rssi);
        pr_info("  SNR: %d dB\n", spatial->snr);
        pr_info("  Spatial reuse: %d\n", spatial->spatial_reuse);
        pr_info("  Correlation: %d\n", spatial->metrics.correlation);
        pr_info("  Isolation: %d\n", spatial->metrics.isolation);
        pr_info("  Rank: %d\n", spatial->metrics.rank);

        pr_info("  Streams: %d\n", streams->num_streams);
        pr_info("  MCS: %d\n", streams->mcs);
        pr_info("  Power: %d\n", streams->power_level);

        if (streams->stream_map) {
            pr_info("  Stream mapping:\n");
            for (j = 0; j < streams->num_streams; j++) {
                pr_info("    Stream %d: idx=%d, ant=0x%x\n",
                        j, streams->stream_map[j].stream_idx,
                        streams->stream_map[j].antenna_mask);
            }
        }
    }
}
EXPORT_SYMBOL_GPL(wifi7_mu_dump_group);

/* Module initialization */
static int __init wifi7_mu_init(void)
{
    pr_info("WiFi 7 MU-MIMO initialized\n");
    return 0;
}

static void __exit wifi7_mu_exit(void)
{
    pr_info("WiFi 7 MU-MIMO unloaded\n");
}

module_init(wifi7_mu_init);
module_exit(wifi7_mu_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 MU-MIMO Support");
MODULE_VERSION("1.0"); 