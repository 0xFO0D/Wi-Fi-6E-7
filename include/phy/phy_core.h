#ifndef _WIFI67_PHY_CORE_H_
#define _WIFI67_PHY_CORE_H_

#include <linux/types.h>
#include "../core/wifi67_forward.h"

/* PHY capabilities */
#define WIFI67_PHY_CAP_BE          BIT(0)
#define WIFI67_PHY_CAP_320MHZ      BIT(1)
#define WIFI67_PHY_CAP_4K_QAM      BIT(2)
#define WIFI67_PHY_CAP_MULTI_RU    BIT(3)
#define WIFI67_PHY_CAP_PUNC        BIT(4)
#define WIFI67_PHY_CAP_MLO         BIT(5)

/* PHY states */
enum wifi67_phy_state {
    WIFI67_PHY_OFF,
    WIFI67_PHY_STARTING,
    WIFI67_PHY_READY,
    WIFI67_PHY_TX,
    WIFI67_PHY_RX,
    WIFI67_PHY_SLEEP,
};

/* PHY configuration */
struct wifi67_phy_config {
    u32 capabilities;
    u8 bands;          /* Supported bands */
    u8 streams;        /* Number of spatial streams */
    u16 channels;      /* Supported channels */
    u8 bandwidth;      /* Current bandwidth */
    bool mlo_enabled;  /* Multi-link operation */
};

/* PHY statistics */
struct wifi67_phy_stats {
    u64 tx_packets;
    u64 rx_packets;
    u64 tx_bytes;
    u64 rx_bytes;
    u32 tx_errors;
    u32 rx_errors;
    u32 noise_floor;
    u32 cca_busy;
};

/* Main PHY structure */
struct wifi67_phy {
    struct wifi67_phy_config config;
    struct wifi67_phy_stats stats;
    enum wifi67_phy_state state;
    
    /* Hardware registers */
    void __iomem *regs;
    
    /* Lock for PHY access */
    spinlock_t lock;
    
    /* Power management */
    bool ps_enabled;
    u32 ps_state;
};

/* Function declarations */
int wifi67_phy_init(struct wifi67_priv *priv);
void wifi67_phy_deinit(struct wifi67_priv *priv);
int wifi67_phy_start(struct wifi67_priv *priv);
void wifi67_phy_stop(struct wifi67_priv *priv);
int wifi67_phy_config(struct wifi67_priv *priv, struct ieee80211_conf *conf);
int wifi67_phy_set_channel(struct wifi67_priv *priv,
                          struct ieee80211_channel *channel,
                          enum nl80211_channel_type channel_type);
void wifi67_phy_set_power(struct wifi67_priv *priv, bool enable);

#endif /* _WIFI67_PHY_CORE_H_ */ 