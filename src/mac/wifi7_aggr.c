/*
 * WiFi 7 Frame Aggregation
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
#include "wifi7_aggr.h"
#include "wifi7_mac.h"

/* Helper functions */
static inline bool is_ampdu_allowed(struct wifi7_aggr_queue *queue,
                                  struct sk_buff *skb)
{
    if (!(queue->flags & WIFI7_AGGR_FLAG_AMPDU))
        return false;
        
    if (queue->n_frames >= queue->max_subframes)
        return false;
        
    if (queue->bytes + skb->len > queue->max_ampdu_len)
        return false;
        
    return true;
}

static inline bool is_amsdu_allowed(struct wifi7_aggr_queue *queue,
                                  struct sk_buff *skb)
{
    if (!(queue->flags & WIFI7_AGGR_FLAG_AMSDU))
        return false;
        
    if (queue->n_frames >= queue->max_subframes)
        return false;
        
    if (queue->bytes + skb->len > queue->max_amsdu_len)
        return false;
        
    return true;
}

static inline void update_aggr_stats(struct wifi7_aggr_queue *queue,
                                   struct wifi7_aggr_desc *desc)
{
    if (desc->type == WIFI7_AGGR_POLICY_AMPDU) {
        if (desc->acknowledged)
            queue->ampdu_tx++;
        else if (desc->retry_count > 0)
            queue->ampdu_retry++;
        else
            queue->ampdu_fail++;
    } else if (desc->type == WIFI7_AGGR_POLICY_AMSDU) {
        if (desc->acknowledged)
            queue->amsdu_tx++;
        else if (desc->retry_count > 0)
            queue->amsdu_retry++;
        else
            queue->amsdu_fail++;
    }
}

/* Queue management */
static struct wifi7_aggr_queue *wifi7_aggr_get_queue(struct wifi7_aggr *aggr,
                                                   u8 queue_id)
{
    if (queue_id >= aggr->n_queues)
        return NULL;
        
    return &aggr->queues[queue_id];
}

static void wifi7_aggr_queue_work(struct work_struct *work)
{
    struct wifi7_aggr_queue *queue = container_of(to_delayed_work(work),
                                                struct wifi7_aggr_queue,
                                                aggr_work);
    struct wifi7_aggr_desc desc;
    unsigned long flags;
    
    spin_lock_irqsave(&queue->lock, flags);
    
    if (!queue->active)
        goto out_unlock;
        
    /* Initialize descriptor */
    memset(&desc, 0, sizeof(desc));
    desc.type = queue->flags & WIFI7_AGGR_FLAG_AMPDU ?
                WIFI7_AGGR_POLICY_AMPDU : WIFI7_AGGR_POLICY_AMSDU;
    desc.policy = WIFI7_AGGR_POLICY_DYNAMIC;
    desc.state = WIFI7_AGGR_STATE_STARTING;
    desc.flags = queue->flags;
    desc.start_time = ktime_get();
    
    /* Process frames */
    // TODO: Process frames from queue
    
    /* Update timing */
    desc.end_time = ktime_get();
    desc.duration = ktime_us_delta(desc.end_time, desc.start_time);
    
    /* Update statistics */
    update_aggr_stats(queue, &desc);
    
    /* Schedule next run */
    if (queue->active && queue->aggr_interval > 0)
        schedule_delayed_work(&queue->aggr_work,
                            msecs_to_jiffies(queue->aggr_interval));
                            
out_unlock:
    spin_unlock_irqrestore(&queue->lock, flags);
}

/* Frame processing */
static int wifi7_aggr_process_ampdu(struct wifi7_aggr *aggr,
                                  struct sk_buff *skb,
                                  struct wifi7_aggr_desc *desc)
{
    struct wifi7_aggr_subframe *frame;
    
    if (desc->n_frames >= WIFI7_AGGR_MAX_SUBFRAMES)
        return -ENOSPC;
        
    /* Add frame to descriptor */
    frame = &desc->frames[desc->n_frames++];
    frame->skb = skb;
    frame->len = skb->len;
    frame->tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
    frame->ac = ieee80211_ac_from_tid(frame->tid);
    frame->seq = 0; // TODO: Assign sequence number
    frame->qos = true;
    frame->retry = false;
    frame->more_frag = false;
    frame->flags = 0;
    
    /* Update descriptor */
    desc->len += frame->len;
    desc->tid_bitmap |= BIT(frame->tid);
    desc->n_tids = hweight8(desc->tid_bitmap);
    
    if (desc->n_tids == 1)
        desc->primary_tid = frame->tid;
        
    return 0;
}

static int wifi7_aggr_process_amsdu(struct wifi7_aggr *aggr,
                                  struct sk_buff *skb,
                                  struct wifi7_aggr_desc *desc)
{
    struct wifi7_aggr_subframe *frame;
    
    if (desc->n_frames >= WIFI7_AGGR_MAX_SUBFRAMES)
        return -ENOSPC;
        
    /* Add frame to descriptor */
    frame = &desc->frames[desc->n_frames++];
    frame->skb = skb;
    frame->len = skb->len;
    frame->tid = skb->priority & IEEE80211_QOS_CTL_TID_MASK;
    frame->ac = ieee80211_ac_from_tid(frame->tid);
    frame->seq = 0; // TODO: Assign sequence number
    frame->qos = true;
    frame->retry = false;
    frame->more_frag = false;
    frame->flags = 0;
    
    /* Update descriptor */
    desc->len += frame->len;
    desc->tid_bitmap |= BIT(frame->tid);
    desc->n_tids = hweight8(desc->tid_bitmap);
    
    if (desc->n_tids == 1)
        desc->primary_tid = frame->tid;
        
    return 0;
}

/* Public API Implementation */
int wifi7_aggr_init(struct wifi7_dev *dev)
{
    struct wifi7_aggr *aggr;
    int ret;
    
    aggr = kzalloc(sizeof(*aggr), GFP_KERNEL);
    if (!aggr)
        return -ENOMEM;
        
    /* Set capabilities */
    aggr->capabilities = WIFI7_AGGR_CAP_AMPDU |
                        WIFI7_AGGR_CAP_AMSDU |
                        WIFI7_AGGR_CAP_MULTI_TID |
                        WIFI7_AGGR_CAP_DYNAMIC |
                        WIFI7_AGGR_CAP_REORDER |
                        WIFI7_AGGR_CAP_IMMEDIATE |
                        WIFI7_AGGR_CAP_COMPRESSED;
                        
    /* Set default parameters */
    aggr->policy = WIFI7_AGGR_POLICY_DYNAMIC;
    aggr->timeout = 10;
    aggr->max_tids = WIFI7_AGGR_MAX_TIDS;
    aggr->max_queues = WIFI7_AGGR_MAX_QUEUES;
    
    /* Initialize locks */
    spin_lock_init(&aggr->queue_lock);
    
    /* Create workqueue */
    aggr->wq = create_singlethread_workqueue("wifi7_aggr");
    if (!aggr->wq) {
        ret = -ENOMEM;
        goto err_free_aggr;
    }
    
    dev->aggr = aggr;
    return 0;
    
err_free_aggr:
    kfree(aggr);
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_aggr_init);

void wifi7_aggr_deinit(struct wifi7_dev *dev)
{
    struct wifi7_aggr *aggr = dev->aggr;
    int i;
    
    if (!aggr)
        return;
        
    /* Stop all queues */
    for (i = 0; i < aggr->n_queues; i++)
        wifi7_aggr_queue_deinit(dev, i);
        
    /* Destroy workqueue */
    destroy_workqueue(aggr->wq);
    
    kfree(aggr);
    dev->aggr = NULL;
}
EXPORT_SYMBOL_GPL(wifi7_aggr_deinit);

int wifi7_aggr_queue_init(struct wifi7_dev *dev, u8 queue_id)
{
    struct wifi7_aggr *aggr = dev->aggr;
    struct wifi7_aggr_queue *queue;
    
    if (!aggr || queue_id >= aggr->max_queues)
        return -EINVAL;
        
    queue = &aggr->queues[queue_id];
    
    /* Initialize queue */
    queue->queue_id = queue_id;
    queue->tid = 0;
    queue->ac = 0;
    queue->flags = WIFI7_AGGR_FLAG_AMPDU |
                  WIFI7_AGGR_FLAG_AMSDU |
                  WIFI7_AGGR_FLAG_IMMEDIATE |
                  WIFI7_AGGR_FLAG_COMPRESSED;
                  
    /* Set parameters */
    queue->max_ampdu_len = WIFI7_AGGR_MAX_AMPDU_LEN;
    queue->max_amsdu_len = WIFI7_AGGR_MAX_AMSDU_LEN;
    queue->max_subframes = WIFI7_AGGR_MAX_SUBFRAMES;
    queue->density = 0;
    queue->spacing = 0;
    
    /* Initialize state */
    queue->active = true;
    queue->n_frames = 0;
    queue->bytes = 0;
    
    /* Initialize timing */
    queue->last_aggr = ktime_get();
    queue->aggr_interval = 1; /* 1ms */
    
    /* Initialize work */
    INIT_DELAYED_WORK(&queue->aggr_work, wifi7_aggr_queue_work);
    spin_lock_init(&queue->lock);
    
    /* Schedule work */
    if (queue->active && queue->aggr_interval > 0)
        schedule_delayed_work(&queue->aggr_work,
                            msecs_to_jiffies(queue->aggr_interval));
                            
    if (queue_id >= aggr->n_queues)
        aggr->n_queues = queue_id + 1;
        
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_aggr_queue_init);

void wifi7_aggr_queue_deinit(struct wifi7_dev *dev, u8 queue_id)
{
    struct wifi7_aggr *aggr = dev->aggr;
    struct wifi7_aggr_queue *queue;
    
    if (!aggr || queue_id >= aggr->n_queues)
        return;
        
    queue = &aggr->queues[queue_id];
    
    /* Stop queue */
    queue->active = false;
    cancel_delayed_work_sync(&queue->aggr_work);
}
EXPORT_SYMBOL_GPL(wifi7_aggr_queue_deinit);

int wifi7_aggr_add_frame(struct wifi7_dev *dev,
                        struct sk_buff *skb,
                        struct wifi7_aggr_desc *desc)
{
    struct wifi7_aggr *aggr = dev->aggr;
    int ret;
    
    if (!aggr || !skb || !desc)
        return -EINVAL;
        
    /* Process frame based on type */
    switch (desc->type) {
    case WIFI7_AGGR_POLICY_AMPDU:
        ret = wifi7_aggr_process_ampdu(aggr, skb, desc);
        break;
    case WIFI7_AGGR_POLICY_AMSDU:
        ret = wifi7_aggr_process_amsdu(aggr, skb, desc);
        break;
    default:
        ret = -EINVAL;
        break;
    }
    
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_aggr_add_frame);

int wifi7_aggr_process(struct wifi7_dev *dev,
                      struct wifi7_aggr_desc *desc)
{
    struct wifi7_aggr *aggr = dev->aggr;
    int ret = 0;
    
    if (!aggr || !desc)
        return -EINVAL;
        
    /* Process aggregation */
    desc->state = WIFI7_AGGR_STATE_ACTIVE;
    
    /* Request block ack if needed */
    if (desc->n_frames > 1) {
        desc->ba_req = true;
        desc->ba_policy = desc->flags & WIFI7_AGGR_FLAG_IMMEDIATE ?
                         WIFI7_AGGR_FLAG_IMMEDIATE : 0;
        desc->ba_timeout = aggr->timeout;
        desc->ba_size = desc->n_frames;
    }
    
    desc->complete = true;
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_aggr_process);

int wifi7_aggr_complete(struct wifi7_dev *dev,
                       struct wifi7_aggr_desc *desc)
{
    struct wifi7_aggr *aggr = dev->aggr;
    struct wifi7_aggr_queue *queue;
    int i;
    
    if (!aggr || !desc)
        return -EINVAL;
        
    /* Update queue statistics */
    for (i = 0; i < aggr->n_queues; i++) {
        queue = &aggr->queues[i];
        if (queue->tid == desc->primary_tid) {
            update_aggr_stats(queue, desc);
            break;
        }
    }
    
    /* Free descriptor */
    for (i = 0; i < desc->n_frames; i++) {
        if (desc->frames[i].skb)
            dev_kfree_skb(desc->frames[i].skb);
    }
    
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_aggr_complete);

/* Module initialization */
static int __init wifi7_aggr_init_module(void)
{
    pr_info("WiFi 7 Frame Aggregation initialized\n");
    return 0;
}

static void __exit wifi7_aggr_exit_module(void)
{
    pr_info("WiFi 7 Frame Aggregation unloaded\n");
}

module_init(wifi7_aggr_init_module);
module_exit(wifi7_aggr_exit_module);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Frame Aggregation");
MODULE_VERSION("1.0"); 