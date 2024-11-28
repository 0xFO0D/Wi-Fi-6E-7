#ifndef _WIFI67_PHY_CORE_H_
#define _WIFI67_PHY_CORE_H_

#include <linux/types.h>
#include "../core/wifi67.h"

struct phy_stats {
    u64 rx_packets;
    u64 tx_packets;
    u32 rx_errors;
    u32 tx_errors;
    u32 rx_dropped;
    u32 tx_retries;
    u32 crc_errors;
    u32 phy_errors;
} __packed __aligned(8);

struct phy_calibration {
    u32 tx_iq_cal;
    u32 rx_iq_cal;
    u32 tx_dc_offset;
    u32 rx_dc_offset;
    u32 pll_cal;
    u32 temp_comp;
} __packed;

int wifi67_phy_init(struct wifi67_priv *priv);
void wifi67_phy_deinit(struct wifi67_priv *priv);
int wifi67_phy_start(struct wifi67_priv *priv);
void wifi67_phy_stop(struct wifi67_priv *priv);
int wifi67_phy_config_channel(struct wifi67_priv *priv, u32 freq, u32 bandwidth);
int wifi67_phy_set_txpower(struct wifi67_priv *priv, int dbm);
int wifi67_phy_get_rssi(struct wifi67_priv *priv);

#endif /* _WIFI67_PHY_CORE_H_ */ 