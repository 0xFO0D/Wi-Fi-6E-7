#ifndef _WIFI67_TYPES_H_
#define _WIFI67_TYPES_H_

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <net/mac80211.h>

/* Forward declarations */
struct wifi67_priv;
struct wifi67_mac;
struct wifi67_phy;
struct wifi67_crypto_ctx;

/* Common structures */
struct wifi67_stats {
    u64 tx_packets;
    u64 tx_bytes;
    u64 rx_packets;
    u64 rx_bytes;
    u32 tx_errors;
    u32 rx_errors;
};

#endif /* _WIFI67_TYPES_H_ */ 