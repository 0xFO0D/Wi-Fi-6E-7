/*
 * WiFi 7 Block Acknowledgment
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_BA_H
#define __WIFI7_BA_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include "../core/wifi7_core.h"

/* Block ack parameters */
#define WIFI7_BA_MAX_TID          8
#define WIFI7_BA_MAX_SESSIONS    16
#define WIFI7_BA_MAX_FRAMES     256
#define WIFI7_BA_MAX_REORDER    128
#define WIFI7_BA_MAX_TIMEOUT    100  /* ms */
#define WIFI7_BA_MIN_TIMEOUT     10  /* ms */
#define WIFI7_BA_MAX_WINDOW     64
#define WIFI7_BA_MIN_WINDOW      8

/* Block ack flags */
#define WIFI7_BA_FLAG_IMMEDIATE  BIT(0)  /* Immediate BA */
#define WIFI7_BA_FLAG_COMPRESSED BIT(1)  /* Compressed bitmap */
#define WIFI7_BA_FLAG_MULTI_TID  BIT(2)  /* Multi-TID BA */
#define WIFI7_BA_FLAG_GCR        BIT(3)  /* GCR BA */
#define WIFI7_BA_FLAG_ALL_ACK    BIT(4)  /* All-ack context */
#define WIFI7_BA_FLAG_DL_MU      BIT(5)  /* DL MU-MIMO BA */
#define WIFI7_BA_FLAG_UL_MU      BIT(6)  /* UL MU-MIMO BA */
#define WIFI7_BA_FLAG_PROTECTED  BIT(7)  /* Protected BA */

/* Block ack states */
#define WIFI7_BA_STATE_IDLE      0  /* No BA session */
#define WIFI7_BA_STATE_INIT      1  /* BA session initializing */
#define WIFI7_BA_STATE_ACTIVE    2  /* BA session active */
#define WIFI7_BA_STATE_SUSPEND   3  /* BA session suspended */
#define WIFI7_BA_STATE_TEARDOWN  4  /* BA session tearing down */

/* Block ack frame types */
#define WIFI7_BA_FRAME_REQ      0  /* BA request */
#define WIFI7_BA_FRAME_RESP     1  /* BA response */
#define WIFI7_BA_FRAME_ADDBA    2  /* ADDBA request */
#define WIFI7_BA_FRAME_ADDBA_RESP 3  /* ADDBA response */
#define WIFI7_BA_FRAME_DELBA    4  /* DELBA */

/* Block ack policy */
#define WIFI7_BA_POLICY_NORMAL   0  /* Normal ack */
#define WIFI7_BA_POLICY_NO_ACK   1  /* No ack */
#define WIFI7_BA_POLICY_BLOCK    2  /* Block ack */
#define WIFI7_BA_POLICY_DELAYED  3  /* Delayed block ack */

/* Block ack reason codes */
#define WIFI7_BA_REASON_NONE     0  /* No specific reason */
#define WIFI7_BA_REASON_TIMEOUT  1  /* BA timeout */
#define WIFI7_BA_REASON_RESET    2  /* BA reset */
#define WIFI7_BA_REASON_UNSPEC   3  /* Unspecified reason */
#define WIFI7_BA_REASON_RESOURCE 4  /* Resource limitation */
#define WIFI7_BA_REASON_POLICY   5  /* Policy change */

/* Block ack frame header */
struct wifi7_ba_frame_hdr {
    __le16 frame_control;
    __le16 duration;
    u8 ra[ETH_ALEN];
    u8 ta[ETH_ALEN];
    __le16 ba_control;
    __le16 ba_info;
    u8 bitmap[32];
} __packed;

/* Block ack session info */
struct wifi7_ba_session {
    u8 tid;                    /* Traffic ID */
    u8 state;                  /* Session state */
    u16 timeout;              /* BA timeout in ms */
    u16 buffer_size;          /* BA buffer size */
    u16 ssn;                  /* Starting sequence number */
    u16 head_seq;             /* Head sequence number */
    u16 tail_seq;             /* Tail sequence number */
    u32 flags;                /* BA flags */
    
    /* Reordering buffer */
    struct sk_buff_head reorder_queue;
    struct sk_buff *reorder_buf[WIFI7_BA_MAX_REORDER];
    unsigned long reorder_bitmap[BITS_TO_LONGS(WIFI7_BA_MAX_REORDER)];
    
    /* Statistics */
    u32 rx_mpdu;              /* Received MPDUs */
    u32 tx_mpdu;              /* Transmitted MPDUs */
    u32 rx_reorder;           /* Reordered MPDUs */
    u32 rx_drop;              /* Dropped MPDUs */
    u32 rx_dup;               /* Duplicate MPDUs */
    u32 rx_ooo;               /* Out of order MPDUs */
    u32 tx_retry;             /* Retried MPDUs */
    u32 tx_fail;              /* Failed MPDUs */
    
    /* Timers */
    struct timer_list reorder_timer;
    struct timer_list session_timer;
    
    /* Locks */
    spinlock_t lock;
    
    /* Peer info */
    u8 peer_addr[ETH_ALEN];
    bool active;
};

/* Block ack device info */
struct wifi7_ba {
    /* Session management */
    struct wifi7_ba_session sessions[WIFI7_BA_MAX_SESSIONS];
    u8 num_sessions;
    spinlock_t lock;
    
    /* Configuration */
    u16 timeout;              /* Default BA timeout */
    u16 buffer_size;          /* Default buffer size */
    u32 flags;                /* BA capabilities */
    bool active;              /* BA enabled */
    
    /* Statistics */
    struct {
        u32 tx_ba_frames;     /* Transmitted BA frames */
        u32 rx_ba_frames;     /* Received BA frames */
        u32 tx_addba;         /* Transmitted ADDBA */
        u32 rx_addba;         /* Received ADDBA */
        u32 tx_delba;         /* Transmitted DELBA */
        u32 rx_delba;         /* Received DELBA */
        u32 timeouts;         /* BA timeouts */
        u32 resets;           /* BA resets */
        u32 failures;         /* BA failures */
    } stats;
};

/* Function prototypes */
int wifi7_ba_init(struct wifi7_dev *dev);
void wifi7_ba_deinit(struct wifi7_dev *dev);

int wifi7_ba_start(struct wifi7_dev *dev);
void wifi7_ba_stop(struct wifi7_dev *dev);

int wifi7_ba_session_start(struct wifi7_dev *dev, u8 tid,
                          const u8 *peer, u16 timeout,
                          u16 buf_size, u32 flags);
void wifi7_ba_session_stop(struct wifi7_dev *dev, u8 tid,
                          const u8 *peer, u8 reason);

int wifi7_ba_rx_frame(struct wifi7_dev *dev,
                     struct sk_buff *skb);
int wifi7_ba_tx_frame(struct wifi7_dev *dev,
                     struct sk_buff *skb);

int wifi7_ba_get_stats(struct wifi7_dev *dev,
                      struct wifi7_ba_session *stats);
int wifi7_ba_clear_stats(struct wifi7_dev *dev);

#endif /* __WIFI7_BA_H */ 