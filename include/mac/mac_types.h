#ifndef _WIFI67_MAC_TYPES_H_
#define _WIFI67_MAC_TYPES_H_

#include <linux/types.h>
#include <linux/if_ether.h>
#include "mac_defs.h"

/* MAC Configuration Structure */
struct wifi67_mac_config {
    u32 flags;                  /* Configuration flags */
    u8 bssid[ETH_ALEN];        /* BSSID */
    u8 mac_addr[ETH_ALEN];     /* MAC address */
    u16 aid;                   /* Association ID */
    u16 beacon_int;            /* Beacon interval */
    u16 dtim_period;           /* DTIM period */
    u16 rts_threshold;         /* RTS threshold */
    u16 frag_threshold;        /* Fragmentation threshold */
    u8 retry_short;           /* Short retry limit */
    u8 retry_long;            /* Long retry limit */
    u8 qos_enabled;           /* QoS enabled */
    u8 ht_enabled;            /* HT enabled */
    u8 vht_enabled;           /* VHT enabled */
    u8 he_enabled;            /* HE enabled */
    u8 eht_enabled;           /* EHT enabled */
    u32 basic_rates;          /* Basic rate set */
    u32 supported_rates;      /* Supported rate set */
};

/* MAC Statistics Structure */
struct wifi67_mac_stats {
    /* General statistics */
    u64 tx_packets;           /* Transmitted packets */
    u64 rx_packets;           /* Received packets */
    u64 tx_bytes;             /* Transmitted bytes */
    u64 rx_bytes;             /* Received bytes */
    u32 tx_dropped;           /* Dropped TX packets */
    u32 rx_dropped;           /* Dropped RX packets */
    u32 tx_retries;           /* TX retry count */
    
    /* Error statistics */
    u32 rx_crc_errors;        /* CRC errors */
    u32 rx_decrypt_errors;    /* Decryption errors */
    u32 rx_mic_errors;        /* MIC failures */
    u32 rx_invalid_rate;      /* Invalid rate errors */
    u32 rx_invalid_len;       /* Invalid length errors */
    
    /* Queue statistics */
    u32 tx_queue_full[WIFI67_NUM_TX_QUEUES];  /* Queue full count */
    u32 tx_queue_stopped[WIFI67_NUM_TX_QUEUES]; /* Queue stopped count */
    u32 tx_fifo_underrun;     /* TX FIFO underrun */
    u32 rx_fifo_overrun;      /* RX FIFO overrun */
    
    /* Aggregation statistics */
    u32 tx_ampdu_count;       /* TX A-MPDU count */
    u32 rx_ampdu_count;       /* RX A-MPDU count */
    u32 tx_amsdu_count;       /* TX A-MSDU count */
    u32 rx_amsdu_count;       /* RX A-MSDU count */
    
    /* Management frame statistics */
    u32 tx_mgmt_count;        /* TX management frames */
    u32 rx_mgmt_count;        /* RX management frames */
    u32 beacon_count;         /* Beacon count */
    u32 probe_req_count;      /* Probe request count */
    u32 probe_resp_count;     /* Probe response count */
};

#endif /* _WIFI67_MAC_TYPES_H_ */ 