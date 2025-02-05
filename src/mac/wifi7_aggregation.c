/*
 * WiFi 7 Cross-Link Frame Aggregation and Reordering
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
#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include "wifi7_aggregation.h"
#include "wifi7_mac.h"
#include "wifi7_mlo.h"

/* Maximum number of frames in an aggregation */
#define WIFI7_MAX_AGG_FRAMES     256
#define WIFI7_MAX_AGG_SIZE       (4 * 1024 * 1024)  /* 4MB */
#define WIFI7_MAX_AGG_TIMEOUT    (50)  /* 50ms */
#define WIFI7_MAX_REORDER_BUFFER 1024
#define WIFI7_MAX_REORDER_TIMEOUT (100) /* 100ms */

/* Aggregation context for a TID */
struct wifi7_agg_tid_ctx {
    struct rb_root pending_frames;  /* Pending frames tree */
    struct list_head ready_frames;  /* Ready to aggregate frames */
    spinlock_t lock;               /* Context lock */
    u16 ssn;                       /* Starting sequence number */
    u16 next_ssn;                  /* Next expected sequence number */
    u8 tid;                        /* Traffic ID */
    u8 link_mask;                  /* Active links mask */
    u32 max_size;                  /* Maximum aggregation size */
    u32 max_frames;                /* Maximum frames per aggregation */
    u32 timeout;                   /* Aggregation timeout in ms */
    struct delayed_work timeout_work; /* Timeout work */
    struct wifi7_dev *dev;         /* Device pointer */
    atomic_t pending_count;        /* Pending frames count */
    bool active;                   /* Context active flag */
};

/* Reordering context for a TID */
struct wifi7_reorder_tid_ctx {
    struct rb_root reorder_tree;   /* Reordering buffer tree */
    struct list_head ready_frames; /* Ready to deliver frames */
    spinlock_t lock;              /* Context lock */
    u16 head_ssn;                /* Head sequence number */
    u16 tail_ssn;                /* Tail sequence number */
    u8 tid;                      /* Traffic ID */
    u8 link_mask;                /* Active links mask */
    u32 buffer_size;             /* Reorder buffer size */
    u32 timeout;                 /* Reorder timeout in ms */
    struct delayed_work timeout_work; /* Timeout work */
    struct wifi7_dev *dev;        /* Device pointer */
    atomic_t pending_count;       /* Pending frames count */
    bool active;                 /* Context active flag */
};

/* Frame entry in aggregation/reordering trees */
struct wifi7_frame_entry {
    struct rb_node node;         /* RB-tree node */
    struct list_head list;       /* List entry */
    struct sk_buff *skb;         /* Frame buffer */
    u16 ssn;                     /* Sequence number */
    u8 tid;                      /* Traffic ID */
    u8 link_id;                  /* Link ID */
    ktime_t timestamp;           /* Frame timestamp */
    bool ready;                  /* Frame ready flag */
};

/* Global aggregation/reordering contexts */
static struct {
    struct wifi7_agg_tid_ctx agg_contexts[WIFI7_NUM_TIDS];
    struct wifi7_reorder_tid_ctx reorder_contexts[WIFI7_NUM_TIDS];
    spinlock_t lock;
    bool initialized;
} wifi7_agg_ctx;

/* Helper functions */
static inline int frame_entry_cmp(const struct wifi7_frame_entry *a,
                                const struct wifi7_frame_entry *b)
{
    return (int)((s16)(a->ssn - b->ssn));
}

static struct wifi7_frame_entry *frame_entry_search(struct rb_root *root,
                                                  u16 ssn)
{
    struct rb_node *node = root->rb_node;
    struct wifi7_frame_entry *entry;

    while (node) {
        entry = rb_entry(node, struct wifi7_frame_entry, node);
        int cmp = (int)((s16)(ssn - entry->ssn));

        if (cmp < 0)
            node = node->rb_left;
        else if (cmp > 0)
            node = node->rb_right;
        else
            return entry;
    }
    return NULL;
}

static void frame_entry_insert(struct rb_root *root,
                             struct wifi7_frame_entry *new)
{
    struct rb_node **link = &root->rb_node;
    struct rb_node *parent = NULL;
    struct wifi7_frame_entry *entry;
    int cmp;

    while (*link) {
        parent = *link;
        entry = rb_entry(parent, struct wifi7_frame_entry, node);
        cmp = frame_entry_cmp(new, entry);

        if (cmp < 0)
            link = &parent->rb_left;
        else
            link = &parent->rb_right;
    }

    rb_link_node(&new->node, parent, link);
    rb_insert_color(&new->node, root);
}

static void frame_entry_remove(struct rb_root *root,
                             struct wifi7_frame_entry *entry)
{
    rb_erase(&entry->node, root);
}

/* Aggregation timeout handler */
static void wifi7_agg_timeout_handler(struct work_struct *work)
{
    struct wifi7_agg_tid_ctx *ctx = container_of(to_delayed_work(work),
                                               struct wifi7_agg_tid_ctx,
                                               timeout_work);
    struct wifi7_frame_entry *entry;
    struct rb_node *node;
    unsigned long flags;
    LIST_HEAD(expired);

    spin_lock_irqsave(&ctx->lock, flags);

    /* Move expired frames to ready list */
    while ((node = rb_first(&ctx->pending_frames))) {
        entry = rb_entry(node, struct wifi7_frame_entry, node);
        
        if (ktime_to_ms(ktime_sub(ktime_get(), entry->timestamp)) > ctx->timeout) {
            frame_entry_remove(&ctx->pending_frames, entry);
            list_add_tail(&entry->list, &ctx->ready_frames);
            atomic_dec(&ctx->pending_count);
        } else {
            break;
        }
    }

    /* Schedule next timeout if needed */
    if (atomic_read(&ctx->pending_count) > 0)
        schedule_delayed_work(&ctx->timeout_work,
                            msecs_to_jiffies(ctx->timeout));

    spin_unlock_irqrestore(&ctx->lock, flags);

    /* Process ready frames */
    wifi7_process_agg_frames(ctx->dev, ctx->tid);
}

/* Reordering timeout handler */
static void wifi7_reorder_timeout_handler(struct work_struct *work)
{
    struct wifi7_reorder_tid_ctx *ctx = container_of(to_delayed_work(work),
                                                   struct wifi7_reorder_tid_ctx,
                                                   timeout_work);
    struct wifi7_frame_entry *entry;
    struct rb_node *node;
    unsigned long flags;
    LIST_HEAD(expired);

    spin_lock_irqsave(&ctx->lock, flags);

    /* Move expired frames to ready list */
    while ((node = rb_first(&ctx->reorder_tree))) {
        entry = rb_entry(node, struct wifi7_frame_entry, node);
        
        if (ktime_to_ms(ktime_sub(ktime_get(), entry->timestamp)) > ctx->timeout) {
            frame_entry_remove(&ctx->reorder_tree, entry);
            list_add_tail(&entry->list, &ctx->ready_frames);
            atomic_dec(&ctx->pending_count);
            ctx->head_ssn = (entry->ssn + 1) & 0xFFF;
        } else {
            break;
        }
    }

    /* Schedule next timeout if needed */
    if (atomic_read(&ctx->pending_count) > 0)
        schedule_delayed_work(&ctx->timeout_work,
                            msecs_to_jiffies(ctx->timeout));

    spin_unlock_irqrestore(&ctx->lock, flags);

    /* Process ready frames */
    wifi7_process_reordered_frames(ctx->dev, ctx->tid);
}

/* Initialize aggregation context */
static int wifi7_agg_init_tid(struct wifi7_dev *dev, u8 tid)
{
    struct wifi7_agg_tid_ctx *ctx = &wifi7_agg_ctx.agg_contexts[tid];

    ctx->pending_frames = RB_ROOT;
    INIT_LIST_HEAD(&ctx->ready_frames);
    spin_lock_init(&ctx->lock);
    ctx->tid = tid;
    ctx->link_mask = 0;
    ctx->max_size = WIFI7_MAX_AGG_SIZE;
    ctx->max_frames = WIFI7_MAX_AGG_FRAMES;
    ctx->timeout = WIFI7_MAX_AGG_TIMEOUT;
    ctx->dev = dev;
    atomic_set(&ctx->pending_count, 0);
    ctx->active = true;

    INIT_DELAYED_WORK(&ctx->timeout_work, wifi7_agg_timeout_handler);

    return 0;
}

/* Initialize reordering context */
static int wifi7_reorder_init_tid(struct wifi7_dev *dev, u8 tid)
{
    struct wifi7_reorder_tid_ctx *ctx = &wifi7_agg_ctx.reorder_contexts[tid];

    ctx->reorder_tree = RB_ROOT;
    INIT_LIST_HEAD(&ctx->ready_frames);
    spin_lock_init(&ctx->lock);
    ctx->tid = tid;
    ctx->link_mask = 0;
    ctx->buffer_size = WIFI7_MAX_REORDER_BUFFER;
    ctx->timeout = WIFI7_MAX_REORDER_TIMEOUT;
    ctx->dev = dev;
    atomic_set(&ctx->pending_count, 0);
    ctx->active = true;

    INIT_DELAYED_WORK(&ctx->timeout_work, wifi7_reorder_timeout_handler);

    return 0;
}

/* Module initialization */
int wifi7_aggregation_init(struct wifi7_dev *dev)
{
    int i, ret;

    if (wifi7_agg_ctx.initialized)
        return -EALREADY;

    spin_lock_init(&wifi7_agg_ctx.lock);

    /* Initialize contexts for each TID */
    for (i = 0; i < WIFI7_NUM_TIDS; i++) {
        ret = wifi7_agg_init_tid(dev, i);
        if (ret)
            goto err_agg;

        ret = wifi7_reorder_init_tid(dev, i);
        if (ret)
            goto err_reorder;
    }

    wifi7_agg_ctx.initialized = true;
    return 0;

err_reorder:
    while (--i >= 0) {
        cancel_delayed_work_sync(&wifi7_agg_ctx.reorder_contexts[i].timeout_work);
        wifi7_agg_ctx.reorder_contexts[i].active = false;
    }
    i = WIFI7_NUM_TIDS;
err_agg:
    while (--i >= 0) {
        cancel_delayed_work_sync(&wifi7_agg_ctx.agg_contexts[i].timeout_work);
        wifi7_agg_ctx.agg_contexts[i].active = false;
    }
    return ret;
}
EXPORT_SYMBOL(wifi7_aggregation_init);

void wifi7_aggregation_deinit(struct wifi7_dev *dev)
{
    int i;
    struct wifi7_frame_entry *entry;
    struct rb_node *node;
    unsigned long flags;

    if (!wifi7_agg_ctx.initialized)
        return;

    /* Clean up each TID context */
    for (i = 0; i < WIFI7_NUM_TIDS; i++) {
        struct wifi7_agg_tid_ctx *agg_ctx = &wifi7_agg_ctx.agg_contexts[i];
        struct wifi7_reorder_tid_ctx *reorder_ctx = &wifi7_agg_ctx.reorder_contexts[i];

        /* Cancel timeout works */
        cancel_delayed_work_sync(&agg_ctx->timeout_work);
        cancel_delayed_work_sync(&reorder_ctx->timeout_work);

        /* Free pending aggregation frames */
        spin_lock_irqsave(&agg_ctx->lock, flags);
        while ((node = rb_first(&agg_ctx->pending_frames))) {
            entry = rb_entry(node, struct wifi7_frame_entry, node);
            frame_entry_remove(&agg_ctx->pending_frames, entry);
            dev_kfree_skb_any(entry->skb);
            kfree(entry);
        }
        spin_unlock_irqrestore(&agg_ctx->lock, flags);

        /* Free pending reorder frames */
        spin_lock_irqsave(&reorder_ctx->lock, flags);
        while ((node = rb_first(&reorder_ctx->reorder_tree))) {
            entry = rb_entry(node, struct wifi7_frame_entry, node);
            frame_entry_remove(&reorder_ctx->reorder_tree, entry);
            dev_kfree_skb_any(entry->skb);
            kfree(entry);
        }
        spin_unlock_irqrestore(&reorder_ctx->lock, flags);

        agg_ctx->active = false;
        reorder_ctx->active = false;
    }

    wifi7_agg_ctx.initialized = false;
}
EXPORT_SYMBOL(wifi7_aggregation_deinit);

/* Add frame to aggregation */
int wifi7_add_agg_frame(struct wifi7_dev *dev, struct sk_buff *skb,
                       u8 tid, u8 link_id)
{
    struct wifi7_agg_tid_ctx *ctx = &wifi7_agg_ctx.agg_contexts[tid];
    struct wifi7_frame_entry *entry;
    unsigned long flags;
    int ret = 0;

    if (!ctx->active)
        return -EINVAL;

    entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
    if (!entry)
        return -ENOMEM;

    entry->skb = skb;
    entry->tid = tid;
    entry->link_id = link_id;
    entry->timestamp = ktime_get();
    entry->ssn = wifi7_get_frame_ssn(skb);

    spin_lock_irqsave(&ctx->lock, flags);

    /* Check if we can add more frames */
    if (atomic_read(&ctx->pending_count) >= ctx->max_frames) {
        ret = -ENOSPC;
        goto out_unlock;
    }

    /* Add frame to pending tree */
    frame_entry_insert(&ctx->pending_frames, entry);
    atomic_inc(&ctx->pending_count);

    /* Schedule timeout work */
    if (atomic_read(&ctx->pending_count) == 1)
        schedule_delayed_work(&ctx->timeout_work,
                            msecs_to_jiffies(ctx->timeout));

    spin_unlock_irqrestore(&ctx->lock, flags);
    return 0;

out_unlock:
    spin_unlock_irqrestore(&ctx->lock, flags);
    kfree(entry);
    return ret;
}
EXPORT_SYMBOL(wifi7_add_agg_frame);

/* Add frame to reordering */
int wifi7_add_reorder_frame(struct wifi7_dev *dev, struct sk_buff *skb,
                          u8 tid, u8 link_id)
{
    struct wifi7_reorder_tid_ctx *ctx = &wifi7_agg_ctx.reorder_contexts[tid];
    struct wifi7_frame_entry *entry;
    unsigned long flags;
    int ret = 0;

    if (!ctx->active)
        return -EINVAL;

    entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
    if (!entry)
        return -ENOMEM;

    entry->skb = skb;
    entry->tid = tid;
    entry->link_id = link_id;
    entry->timestamp = ktime_get();
    entry->ssn = wifi7_get_frame_ssn(skb);

    spin_lock_irqsave(&ctx->lock, flags);

    /* Check if we can add more frames */
    if (atomic_read(&ctx->pending_count) >= ctx->buffer_size) {
        ret = -ENOSPC;
        goto out_unlock;
    }

    /* Add frame to reorder tree */
    frame_entry_insert(&ctx->reorder_tree, entry);
    atomic_inc(&ctx->pending_count);

    /* Schedule timeout work */
    if (atomic_read(&ctx->pending_count) == 1)
        schedule_delayed_work(&ctx->timeout_work,
                            msecs_to_jiffies(ctx->timeout));

    spin_unlock_irqrestore(&ctx->lock, flags);
    return 0;

out_unlock:
    spin_unlock_irqrestore(&ctx->lock, flags);
    kfree(entry);
    return ret;
}
EXPORT_SYMBOL(wifi7_add_reorder_frame);

/* Process aggregated frames */
void wifi7_process_agg_frames(struct wifi7_dev *dev, u8 tid)
{
    struct wifi7_agg_tid_ctx *ctx = &wifi7_agg_ctx.agg_contexts[tid];
    struct wifi7_frame_entry *entry, *tmp;
    unsigned long flags;
    LIST_HEAD(process_list);

    spin_lock_irqsave(&ctx->lock, flags);

    /* Move ready frames to process list */
    list_splice_init(&ctx->ready_frames, &process_list);

    spin_unlock_irqrestore(&ctx->lock, flags);

    /* Process frames */
    list_for_each_entry_safe(entry, tmp, &process_list, list) {
        list_del(&entry->list);
        wifi7_transmit_frame(dev, entry->skb, entry->tid, entry->link_id);
        kfree(entry);
    }
}
EXPORT_SYMBOL(wifi7_process_agg_frames);

/* Process reordered frames */
void wifi7_process_reordered_frames(struct wifi7_dev *dev, u8 tid)
{
    struct wifi7_reorder_tid_ctx *ctx = &wifi7_agg_ctx.reorder_contexts[tid];
    struct wifi7_frame_entry *entry, *tmp;
    unsigned long flags;
    LIST_HEAD(process_list);

    spin_lock_irqsave(&ctx->lock, flags);

    /* Move ready frames to process list */
    list_splice_init(&ctx->ready_frames, &process_list);

    spin_unlock_irqrestore(&ctx->lock, flags);

    /* Process frames */
    list_for_each_entry_safe(entry, tmp, &process_list, list) {
        list_del(&entry->list);
        wifi7_receive_frame(dev, entry->skb, entry->tid, entry->link_id);
        kfree(entry);
    }
}
EXPORT_SYMBOL(wifi7_process_reordered_frames);

/* Module parameters */
module_param(wifi7_max_agg_frames, uint, 0644);
MODULE_PARM_DESC(wifi7_max_agg_frames, "Maximum frames per aggregation");

module_param(wifi7_max_agg_size, uint, 0644);
MODULE_PARM_DESC(wifi7_max_agg_size, "Maximum aggregation size in bytes");

module_param(wifi7_max_agg_timeout, uint, 0644);
MODULE_PARM_DESC(wifi7_max_agg_timeout, "Aggregation timeout in milliseconds");

module_param(wifi7_max_reorder_buffer, uint, 0644);
MODULE_PARM_DESC(wifi7_max_reorder_buffer, "Maximum reorder buffer size");

module_param(wifi7_max_reorder_timeout, uint, 0644);
MODULE_PARM_DESC(wifi7_max_reorder_timeout, "Reorder timeout in milliseconds");

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Cross-Link Frame Aggregation and Reordering");
MODULE_VERSION("1.0"); 