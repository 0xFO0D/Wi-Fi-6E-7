/*
 * WiFi 7 Block Acknowledgment
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include "wifi7_ba.h"
#include "wifi7_mac.h"

/* Helper functions */
static inline u16 seq_to_index(u16 seq)
{
    return seq & (WIFI7_BA_MAX_REORDER - 1);
}

static inline bool is_seq_valid(u16 seq, u16 head_seq, u16 tail_seq)
{
    return ((seq - head_seq) & 0xFFF) <= ((tail_seq - head_seq) & 0xFFF);
}

static void wifi7_ba_flush_reorder_buffer(struct wifi7_ba_session *session,
                                        u16 seq)
{
    struct sk_buff *skb;
    u16 idx;
    
    while (session->head_seq != seq) {
        idx = seq_to_index(session->head_seq);
        skb = session->reorder_buf[idx];
        
        if (skb) {
            session->reorder_buf[idx] = NULL;
            clear_bit(idx, session->reorder_bitmap);
            skb_queue_tail(&session->reorder_queue, skb);
            session->rx_reorder++;
        } else {
            session->rx_drop++;
        }
        
        session->head_seq = (session->head_seq + 1) & 0xFFF;
    }
}

static void wifi7_ba_reorder_timer(struct timer_list *t)
{
    struct wifi7_ba_session *session = from_timer(session, t, reorder_timer);
    unsigned long flags;
    
    spin_lock_irqsave(&session->lock, flags);
    
    if (session->state == WIFI7_BA_STATE_ACTIVE) {
        /* Flush reorder buffer up to head */
        wifi7_ba_flush_reorder_buffer(session, session->head_seq);
        
        /* Restart timer if more packets pending */
        if (!bitmap_empty(session->reorder_bitmap,
                         WIFI7_BA_MAX_REORDER)) {
            mod_timer(&session->reorder_timer,
                     jiffies + msecs_to_jiffies(session->timeout));
        }
    }
    
    spin_unlock_irqrestore(&session->lock, flags);
}

static void wifi7_ba_session_timer(struct timer_list *t)
{
    struct wifi7_ba_session *session = from_timer(session, t, session_timer);
    unsigned long flags;
    
    spin_lock_irqsave(&session->lock, flags);
    
    if (session->state == WIFI7_BA_STATE_ACTIVE) {
        /* Session timed out */
        session->state = WIFI7_BA_STATE_TEARDOWN;
        
        /* Flush reorder buffer */
        wifi7_ba_flush_reorder_buffer(session,
                                    (session->tail_seq + 1) & 0xFFF);
        
        /* Stop timers */
        del_timer(&session->reorder_timer);
        
        /* Update stats */
        session->active = false;
    }
    
    spin_unlock_irqrestore(&session->lock, flags);
}

/* Session management */
static struct wifi7_ba_session *wifi7_ba_find_session(struct wifi7_ba *ba,
                                                    u8 tid,
                                                    const u8 *peer)
{
    struct wifi7_ba_session *session;
    int i;
    
    for (i = 0; i < WIFI7_BA_MAX_SESSIONS; i++) {
        session = &ba->sessions[i];
        if (session->active &&
            session->tid == tid &&
            ether_addr_equal(session->peer_addr, peer))
            return session;
    }
    
    return NULL;
}

static struct wifi7_ba_session *wifi7_ba_alloc_session(struct wifi7_ba *ba)
{
    struct wifi7_ba_session *session;
    int i;
    
    for (i = 0; i < WIFI7_BA_MAX_SESSIONS; i++) {
        session = &ba->sessions[i];
        if (!session->active)
            return session;
    }
    
    return NULL;
}

/* Frame handling */
static int wifi7_ba_process_addba_req(struct wifi7_dev *dev,
                                    struct sk_buff *skb)
{
    struct wifi7_ba *ba = dev->ba;
    struct wifi7_ba_session *session;
    struct wifi7_ba_frame_hdr *hdr;
    unsigned long flags;
    int ret = 0;
    
    /* Parse frame */
    hdr = (struct wifi7_ba_frame_hdr *)skb->data;
    
    spin_lock_irqsave(&ba->lock, flags);
    
    /* Find or allocate session */
    session = wifi7_ba_find_session(ba, tid, hdr->ta);
    if (!session) {
        session = wifi7_ba_alloc_session(ba);
        if (!session) {
            ret = -ENOMEM;
            goto out;
        }
    }
    
    /* Initialize session */
    memset(session, 0, sizeof(*session));
    session->tid = tid;
    session->state = WIFI7_BA_STATE_INIT;
    session->timeout = min_t(u16, timeout, WIFI7_BA_MAX_TIMEOUT);
    session->buffer_size = min_t(u16, buf_size, WIFI7_BA_MAX_REORDER);
    session->flags = flags;
    session->ssn = le16_to_cpu(hdr->ba_info) & 0xFFF;
    session->head_seq = session->ssn;
    session->tail_seq = session->ssn;
    ether_addr_copy(session->peer_addr, hdr->ta);
    
    /* Initialize reordering */
    skb_queue_head_init(&session->reorder_queue);
    bitmap_zero(session->reorder_bitmap, WIFI7_BA_MAX_REORDER);
    
    /* Initialize timers */
    timer_setup(&session->reorder_timer, wifi7_ba_reorder_timer, 0);
    timer_setup(&session->session_timer, wifi7_ba_session_timer, 0);
    
    /* Initialize lock */
    spin_lock_init(&session->lock);
    
    session->active = true;
    ba->num_sessions++;
    
    /* Update stats */
    ba->stats.rx_addba++;
    
out:
    spin_unlock_irqrestore(&ba->lock, flags);
    return ret;
}

static int wifi7_ba_process_addba_resp(struct wifi7_dev *dev,
                                     struct sk_buff *skb)
{
    struct wifi7_ba *ba = dev->ba;
    struct wifi7_ba_session *session;
    struct wifi7_ba_frame_hdr *hdr;
    unsigned long flags;
    int ret = 0;
    
    /* Parse frame */
    hdr = (struct wifi7_ba_frame_hdr *)skb->data;
    
    spin_lock_irqsave(&ba->lock, flags);
    
    /* Find session */
    session = wifi7_ba_find_session(ba, tid, hdr->ta);
    if (!session) {
        ret = -ENOENT;
        goto out;
    }
    
    /* Update session */
    if (le16_to_cpu(hdr->ba_control) & IEEE80211_BAR_CTRL_ACK_POLICY_NORMAL) {
        session->state = WIFI7_BA_STATE_ACTIVE;
        mod_timer(&session->session_timer,
                 jiffies + msecs_to_jiffies(session->timeout));
    } else {
        session->state = WIFI7_BA_STATE_TEARDOWN;
        session->active = false;
        ba->num_sessions--;
    }
    
    /* Update stats */
    ba->stats.rx_addba++;
    
out:
    spin_unlock_irqrestore(&ba->lock, flags);
    return ret;
}

static int wifi7_ba_process_delba(struct wifi7_dev *dev,
                                struct sk_buff *skb)
{
    struct wifi7_ba *ba = dev->ba;
    struct wifi7_ba_session *session;
    struct wifi7_ba_frame_hdr *hdr;
    unsigned long flags;
    int ret = 0;
    
    /* Parse frame */
    hdr = (struct wifi7_ba_frame_hdr *)skb->data;
    
    spin_lock_irqsave(&ba->lock, flags);
    
    /* Find session */
    session = wifi7_ba_find_session(ba, tid, hdr->ta);
    if (!session) {
        ret = -ENOENT;
        goto out;
    }
    
    /* Stop session */
    session->state = WIFI7_BA_STATE_TEARDOWN;
    del_timer(&session->reorder_timer);
    del_timer(&session->session_timer);
    
    /* Flush reorder buffer */
    wifi7_ba_flush_reorder_buffer(session,
                                (session->tail_seq + 1) & 0xFFF);
    
    session->active = false;
    ba->num_sessions--;
    
    /* Update stats */
    ba->stats.rx_delba++;
    
out:
    spin_unlock_irqrestore(&ba->lock, flags);
    return ret;
}

/* Public API Implementation */
int wifi7_ba_init(struct wifi7_dev *dev)
{
    struct wifi7_ba *ba;
    int ret;
    
    ba = kzalloc(sizeof(*ba), GFP_KERNEL);
    if (!ba)
        return -ENOMEM;
        
    /* Initialize lock */
    spin_lock_init(&ba->lock);
    
    /* Set defaults */
    ba->timeout = WIFI7_BA_MAX_TIMEOUT;
    ba->buffer_size = WIFI7_BA_MAX_REORDER;
    ba->flags = WIFI7_BA_FLAG_IMMEDIATE |
               WIFI7_BA_FLAG_COMPRESSED |
               WIFI7_BA_FLAG_MULTI_TID;
    ba->active = true;
    
    dev->ba = ba;
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_ba_init);

void wifi7_ba_deinit(struct wifi7_dev *dev)
{
    struct wifi7_ba *ba = dev->ba;
    struct wifi7_ba_session *session;
    int i;
    
    if (!ba)
        return;
        
    /* Stop all sessions */
    for (i = 0; i < WIFI7_BA_MAX_SESSIONS; i++) {
        session = &ba->sessions[i];
        if (session->active) {
            del_timer_sync(&session->reorder_timer);
            del_timer_sync(&session->session_timer);
            skb_queue_purge(&session->reorder_queue);
        }
    }
    
    kfree(ba);
    dev->ba = NULL;
}
EXPORT_SYMBOL_GPL(wifi7_ba_deinit);

int wifi7_ba_rx_frame(struct wifi7_dev *dev, struct sk_buff *skb)
{
    struct wifi7_ba *ba = dev->ba;
    struct wifi7_ba_frame_hdr *hdr;
    int ret = 0;
    
    if (!ba || !ba->active)
        return -EINVAL;
        
    /* Parse frame */
    hdr = (struct wifi7_ba_frame_hdr *)skb->data;
    
    /* Process based on frame type */
    switch (le16_to_cpu(hdr->frame_control) & IEEE80211_FCTL_STYPE) {
    case IEEE80211_STYPE_ACTION:
        switch (skb->data[IEEE80211_ACTION_CAT_OFFSET]) {
        case WLAN_CATEGORY_BACK:
            switch (skb->data[IEEE80211_ACTION_ACT_OFFSET]) {
            case WLAN_ACTION_ADDBA_REQ:
                ret = wifi7_ba_process_addba_req(dev, skb);
                break;
            case WLAN_ACTION_ADDBA_RESP:
                ret = wifi7_ba_process_addba_resp(dev, skb);
                break;
            case WLAN_ACTION_DELBA:
                ret = wifi7_ba_process_delba(dev, skb);
                break;
            default:
                ret = -EINVAL;
                break;
            }
            break;
        default:
            ret = -EINVAL;
            break;
        }
        break;
    default:
        ret = -EINVAL;
        break;
    }
    
    return ret;
}
EXPORT_SYMBOL_GPL(wifi7_ba_rx_frame);

/* Module initialization */
static int __init wifi7_ba_init_module(void)
{
    pr_info("WiFi 7 Block Acknowledgment initialized\n");
    return 0;
}

static void __exit wifi7_ba_exit_module(void)
{
    pr_info("WiFi 7 Block Acknowledgment unloaded\n");
}

module_init(wifi7_ba_init_module);
module_exit(wifi7_ba_exit_module);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Block Acknowledgment");
MODULE_VERSION("1.0"); 