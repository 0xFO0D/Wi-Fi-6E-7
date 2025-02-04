#ifndef __WIFI7_SPECTRUM_H
#define __WIFI7_SPECTRUM_H

#include <linux/types.h>
#include <linux/bitops.h>
#include "../core/wifi7_core.h"

#define WIFI7_MAX_CHANNEL_WIDTH    320
#define WIFI7_MIN_CHANNEL_WIDTH     20
#define WIFI7_PUNC_PATTERN_MAX      16
#define WIFI7_MAX_RU_ALLOC         996
#define WIFI7_AFC_MAX_POWER         30

struct wifi7_spectrum_info {
    u32 center_freq;
    u32 bandwidth;
    u8 ru_pattern;
    u8 punc_pattern;
    u8 tx_power;
    u8 afc_power_limit;
    bool dynamic_bw;
    bool psd_mask[WIFI7_MAX_CHANNEL_WIDTH];
};

struct wifi7_afc_req {
    u32 freq_start;
    u32 freq_end;
    u32 max_power;
    u8 geo_area[32];
    u64 timestamp;
};

struct wifi7_spectrum_ops {
    int (*set_channel)(struct wifi7_phy_dev *dev, u32 freq, u32 width);
    int (*set_ru_pattern)(struct wifi7_phy_dev *dev, u8 pattern);
    int (*update_psd)(struct wifi7_phy_dev *dev, const bool *mask);
    int (*get_afc_limits)(struct wifi7_phy_dev *dev, struct wifi7_afc_req *req);
};

int wifi7_spectrum_init(struct wifi7_phy_dev *dev);
void wifi7_spectrum_deinit(struct wifi7_phy_dev *dev);
int wifi7_spectrum_configure(struct wifi7_phy_dev *dev, 
                           struct wifi7_spectrum_info *info);
int wifi7_spectrum_update_afc(struct wifi7_phy_dev *dev,
                            struct wifi7_afc_req *req);

#endif /* __WIFI7_SPECTRUM_H */ 