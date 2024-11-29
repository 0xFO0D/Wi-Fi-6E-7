#ifndef _WIFI67_STATS_H_
#define _WIFI67_STATS_H_

#include "wifi67_types.h"

struct mac_stats {
    u64 tx_packets;
    u64 tx_bytes;
    u64 rx_packets;
    u64 rx_bytes;
    u32 tx_errors;
    u32 rx_errors;
    u32 tx_dropped;
    u32 rx_dropped;
};

struct phy_stats {
    u32 crc_errors;
    u32 phy_errors;
    u32 false_ccas;
    u32 plcp_errors;
    u32 clear_channel;
    u32 channel_busy;
    u32 noise_floor;
    u32 evm_errors;
};

struct dma_stats {
    /* TX statistics */
    u64 tx_packets;
    u64 tx_bytes;
    u32 tx_errors;
    u32 tx_fifo_errors;
    u32 tx_heartbeat_errors;
    u32 tx_window_errors;
    u32 tx_aborted_errors;
    u32 tx_carrier_errors;
    u32 tx_desc_errors;
    u32 tx_ring_full;
    
    /* RX statistics */
    u64 rx_packets;
    u64 rx_bytes;
    u32 rx_errors;
    u32 rx_dropped;
    u32 rx_fifo_errors;
    u32 rx_frame_errors;
    u32 rx_crc_errors;
    u32 rx_length_errors;
    u32 rx_desc_errors;
    u32 rx_ring_full;
    
    /* Interrupt statistics */
    u32 tx_complete_irqs;
    u32 rx_complete_irqs;
    u32 tx_error_irqs;
    u32 rx_error_irqs;
    
    /* Coalescing statistics */
    u32 tx_coal_frames;
    u32 rx_coal_frames;
    u32 tx_coal_usecs;
    u32 rx_coal_usecs;
    u32 tx_coal_missed;
    u32 rx_coal_missed;
};

#endif /* _WIFI67_STATS_H_ */ 