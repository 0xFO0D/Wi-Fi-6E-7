#ifndef _WIFI67_STATS_H_
#define _WIFI67_STATS_H_

#include <linux/types.h>

struct wifi67_stats {
    u64 tx_packets;
    u64 tx_bytes;
    u64 tx_errors;
    u64 rx_packets;
    u64 rx_bytes;
    u64 rx_errors;
    u32 tx_fifo_errors;
    u32 rx_fifo_errors;
    u32 rx_crc_errors;
};

#endif /* _WIFI67_STATS_H_ */ 