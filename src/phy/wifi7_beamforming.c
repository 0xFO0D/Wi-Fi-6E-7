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

/* Forward declarations */
static void wifi7_bf_sounding_work(struct work_struct *work);
static int wifi7_bf_update_steering_matrix(struct wifi7_bf_context *bf,
                                         struct wifi7_bf_group *group,
                                         u8 aid);

/* Allocate beamforming context */
struct wifi7_bf_context *wifi7_bf_alloc(struct wifi7_phy_dev *phy)
{
    struct wifi7_bf_context *bf;
    int i;

    if (!phy)
        return NULL;

    bf = kzalloc(sizeof(*bf), GFP_KERNEL);
    if (!bf)
        return NULL;

    bf->phy = phy;
    spin_lock_init(&bf->group_lock);
    atomic_set(&bf->stats.sounding_in_progress, 0);

    /* Initialize groups */
    for (i = 0; i < WIFI7_BF_MAX_USERS; i++) {
        bf->groups[i].group_id = i;
        bf->groups[i].state = WIFI7_BF_GROUP_IDLE;
    }

    /* Create workqueue for async operations */
    bf->bf_wq = alloc_workqueue("wifi7_bf_wq",
                               WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
    if (!bf->bf_wq)
        goto err_free_bf;

    INIT_DELAYED_WORK(&bf->sounding_work, wifi7_bf_sounding_work);

    return bf;

err_free_bf:
    kfree(bf);
    return NULL;
}
EXPORT_SYMBOL_GPL(wifi7_bf_alloc);

void wifi7_bf_free(struct wifi7_bf_context *bf)
{
    int i, j;

    if (!bf)
        return;

    /* Stop all groups */
    for (i = 0; i < WIFI7_BF_MAX_USERS; i++) {
        struct wifi7_bf_group *group = &bf->groups[i];
        
        for (j = 0; j < group->num_users; j++) {
            kfree(group->users[j].csi.elements);
            kfree(group->users[j].steering.elements);
        }
    }

    if (bf->bf_wq) {
        cancel_delayed_work_sync(&bf->sounding_work);
        destroy_workqueue(bf->bf_wq);
    }

    kfree(bf);
}
EXPORT_SYMBOL_GPL(wifi7_bf_free);

/* Group management */
int wifi7_bf_group_add_user(struct wifi7_bf_context *bf,
                           u8 group_id, u8 aid,
                           u8 num_streams)
{
    struct wifi7_bf_group *group;
    unsigned long flags;
    int i, ret = 0;

    if (!bf || group_id >= WIFI7_BF_MAX_USERS ||
        num_streams > WIFI7_BF_MAX_STREAMS)
        return -EINVAL;

    group = &bf->groups[group_id];
    
    spin_lock_irqsave(&bf->group_lock, flags);

    /* Check if user already exists */
    for (i = 0; i < group->num_users; i++) {
        if (group->users[i].aid == aid) {
            ret = -EEXIST;
            goto out_unlock;
        }
    }

    if (group->num_users >= WIFI7_BF_MAX_USERS) {
        ret = -ENOSPC;
        goto out_unlock;
    }

    /* Add new user */
    i = group->num_users++;
    group->users[i].aid = aid;
    group->users[i].num_streams = num_streams;
    group->users[i].feedback_ready = false;

    /* Allocate CSI matrix */
    group->users[i].csi.elements = kzalloc(
        WIFI7_BF_CSI_MAX_TONES * sizeof(*group->users[i].csi.elements),
        GFP_ATOMIC);
    if (!group->users[i].csi.elements) {
        group->num_users--;
        ret = -ENOMEM;
        goto out_unlock;
    }

    /* Allocate steering matrix */
    group->users[i].steering.elements = kzalloc(
        num_streams * WIFI7_BF_MAX_ANTENNAS * 
        sizeof(*group->users[i].steering.elements),
        GFP_ATOMIC);
    if (!group->users[i].steering.elements) {
        kfree(group->users[i].csi.elements);
        group->num_users--;
        ret = -ENOMEM;
        goto out_unlock;
    }

    group->users[i].steering.num_streams = num_streams;
    group->users[i].steering.valid = false;

    /* Update group characteristics */
    if (group->num_users > 1)
        group->mu_mimo_capable = true;

out_unlock:
    spin_unlock_irqrestore(&bf->group_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_bf_group_add_user);

int wifi7_bf_group_remove_user(struct wifi7_bf_context *bf,
                              u8 group_id, u8 aid)
{
    struct wifi7_bf_group *group;
    unsigned long flags;
    int i, j, ret = -ENOENT;

    if (!bf || group_id >= WIFI7_BF_MAX_USERS)
        return -EINVAL;

    group = &bf->groups[group_id];
    
    spin_lock_irqsave(&bf->group_lock, flags);

    /* Find and remove user */
    for (i = 0; i < group->num_users; i++) {
        if (group->users[i].aid == aid) {
            /* Free resources */
            kfree(group->users[i].csi.elements);
            kfree(group->users[i].steering.elements);

            /* Shift remaining users */
            for (j = i + 1; j < group->num_users; j++)
                group->users[j - 1] = group->users[j];

            group->num_users--;
            
            /* Update MU-MIMO capability */
            if (group->num_users <= 1)
                group->mu_mimo_capable = false;

            ret = 0;
            break;
        }
    }

    spin_unlock_irqrestore(&bf->group_lock, flags);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_bf_group_remove_user);

/* Beamforming operations */
static void wifi7_bf_sounding_work(struct work_struct *work)
{
    struct wifi7_bf_context *bf = container_of(work, struct wifi7_bf_context,
                                             sounding_work.work);
    int i;
    bool active = false;

    if (atomic_read(&bf->stats.sounding_in_progress))
        return;

    atomic_set(&bf->stats.sounding_in_progress, 1);

    /* Check each group for sounding needs */
    for (i = 0; i < WIFI7_BF_MAX_USERS; i++) {
        struct wifi7_bf_group *group = &bf->groups[i];
        unsigned long flags;
        int ret;

        spin_lock_irqsave(&bf->group_lock, flags);

        if (group->state != WIFI7_BF_GROUP_ACTIVE ||
            !time_after(jiffies, group->last_sounding +
                       msecs_to_jiffies(group->sounding_interval))) {
            spin_unlock_irqrestore(&bf->group_lock, flags);
            continue;
        }

        active = true;
        group->state = WIFI7_BF_GROUP_SOUNDING;
        spin_unlock_irqrestore(&bf->group_lock, flags);

        /* Send NDP announcement */
        ret = wifi7_bf_send_ndp(bf, i);
        if (ret) {
            pr_err("Failed to send NDP for group %d: %d\n", i, ret);
            group->stats.error_count++;
            group->state = WIFI7_BF_GROUP_ERROR;
            bf->stats.failed_soundings++;
            continue;
        }

        group->stats.sounding_count++;
        bf->stats.total_soundings++;
    }

    atomic_set(&bf->stats.sounding_in_progress, 0);

    /* Reschedule if there are active groups */
    if (active) {
        queue_delayed_work(bf->bf_wq, &bf->sounding_work,
                          msecs_to_jiffies(WIFI7_BF_MIN_SOUNDING_INTERVAL_MS));
    }
}

int wifi7_bf_send_ndp(struct wifi7_bf_context *bf, u8 group_id)
{
    struct wifi7_bf_group *group;
    int ret = 0;

    if (!bf || group_id >= WIFI7_BF_MAX_USERS)
        return -EINVAL;

    group = &bf->groups[group_id];

    /* TODO: Implement actual NDP transmission
     * This requires hardware-specific implementation
     */
    if (bf->phy->ops && bf->phy->ops->send_ndp)
        ret = bf->phy->ops->send_ndp(bf->phy, group);

    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_bf_send_ndp);

int wifi7_bf_process_feedback(struct wifi7_bf_context *bf,
                            u8 group_id, u8 aid,
                            const u8 *feedback_data,
                            size_t len)
{
    struct wifi7_bf_group *group;
    int i, ret = 0;
    bool all_ready = true;

    if (!bf || !feedback_data || group_id >= WIFI7_BF_MAX_USERS)
        return -EINVAL;

    group = &bf->groups[group_id];

    /* Find user */
    for (i = 0; i < group->num_users; i++) {
        if (group->users[i].aid == aid)
            break;
    }
    if (i >= group->num_users)
        return -ENOENT;

    /* TODO: Process compressed feedback data
     * This needs proper implementation based on hardware
     */

    group->users[i].feedback_ready = true;
    group->users[i].csi.timestamp = ktime_get();

    /* Check if all users have provided feedback */
    for (i = 0; i < group->num_users; i++) {
        if (!group->users[i].feedback_ready) {
            all_ready = false;
            break;
        }
    }

    if (all_ready) {
        group->state = WIFI7_BF_GROUP_COMPUTING;
        ret = wifi7_bf_compute_steering(bf, group_id);
        if (ret == 0) {
            group->state = WIFI7_BF_GROUP_ACTIVE;
            bf->stats.successful_soundings++;
        } else {
            group->state = WIFI7_BF_GROUP_ERROR;
            bf->stats.failed_soundings++;
        }
    }

    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_bf_process_feedback);

/* Steering matrix computation */
static int wifi7_bf_update_steering_matrix(struct wifi7_bf_context *bf,
                                         struct wifi7_bf_group *group,
                                         u8 aid)
{
    int i;
    
    /* Find user */
    for (i = 0; i < group->num_users; i++) {
        if (group->users[i].aid == aid)
            break;
    }
    if (i >= group->num_users)
        return -ENOENT;

    /* TODO: Implement actual steering matrix computation
     * This requires complex matrix operations and
     * hardware-specific optimizations
     */

    group->users[i].steering.valid = true;
    group->users[i].steering.last_update = ktime_get();
    group->stats.steering_updates++;

    return 0;
}

int wifi7_bf_compute_steering(struct wifi7_bf_context *bf,
                            u8 group_id)
{
    struct wifi7_bf_group *group;
    int i, ret = 0;

    if (!bf || group_id >= WIFI7_BF_MAX_USERS)
        return -EINVAL;

    group = &bf->groups[group_id];

    /* Update steering matrix for each user */
    for (i = 0; i < group->num_users; i++) {
        ret = wifi7_bf_update_steering_matrix(bf, group,
                                            group->users[i].aid);
        if (ret)
            break;
    }

    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_bf_compute_steering);

/* Debug interface */
void wifi7_bf_dump_stats(struct wifi7_bf_context *bf)
{
    int i;

    if (!bf)
        return;

    pr_info("WiFi 7 Beamforming Statistics:\n");
    pr_info("Total soundings: %u\n", bf->stats.total_soundings);
    pr_info("Successful: %u\n", bf->stats.successful_soundings);
    pr_info("Failed: %u\n", bf->stats.failed_soundings);
    pr_info("Feedback timeouts: %u\n", bf->stats.feedback_timeouts);

    for (i = 0; i < WIFI7_BF_MAX_USERS; i++) {
        struct wifi7_bf_group *group = &bf->groups[i];
        
        if (group->num_users == 0)
            continue;

        pr_info("\nGroup %d:\n", i);
        pr_info("  Users: %d\n", group->num_users);
        pr_info("  State: %d\n", group->state);
        pr_info("  MU-MIMO: %s\n", group->mu_mimo_capable ? "yes" : "no");
        pr_info("  Soundings: %u\n", group->stats.sounding_count);
        pr_info("  Timeouts: %u\n", group->stats.feedback_timeouts);
        pr_info("  Updates: %u\n", group->stats.steering_updates);
        pr_info("  Errors: %u\n", group->stats.error_count);
    }
}
EXPORT_SYMBOL_GPL(wifi7_bf_dump_stats);

void wifi7_bf_dump_csi(struct wifi7_bf_context *bf,
                      u8 group_id, u8 aid)
{
    struct wifi7_bf_group *group;
    int i;

    if (!bf || group_id >= WIFI7_BF_MAX_USERS)
        return;

    group = &bf->groups[group_id];

    /* Find user */
    for (i = 0; i < group->num_users; i++) {
        if (group->users[i].aid == aid)
            break;
    }
    if (i >= group->num_users)
        return;

    pr_info("CSI Data for Group %d, AID %d:\n", group_id, aid);
    pr_info("Timestamp: %lld\n", ktime_to_ns(group->users[i].csi.timestamp));
    pr_info("Num tones: %d\n", group->users[i].csi.num_tones);
    pr_info("TX antennas: %d\n", group->users[i].csi.num_tx_antennas);
    pr_info("RX antennas: %d\n", group->users[i].csi.num_rx_antennas);

    /* TODO: Add detailed CSI matrix dump */
}
EXPORT_SYMBOL_GPL(wifi7_bf_dump_csi);

/* Module initialization */
static int __init wifi7_bf_init(void)
{
    pr_info("WiFi 7 Beamforming initialized\n");
    return 0;
}

static void __exit wifi7_bf_exit(void)
{
    pr_info("WiFi 7 Beamforming unloaded\n");
}

module_init(wifi7_bf_init);
module_exit(wifi7_bf_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Beamforming Support");
MODULE_VERSION("1.0");