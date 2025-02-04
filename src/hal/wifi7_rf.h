#ifndef __WIFI7_RF_H
#define __WIFI7_RF_H

#include <linux/types.h>
#include "../core/wifi7_core.h"

#define RF_MAX_GAIN_IDX 128
#define RF_TEMP_POINTS   16
#define RF_CAL_CHANNELS  38
#define RF_PLL_LOCK_TIMEOUT_US 100

struct wifi7_rf_gain_table {
    s8 gain_idx;
    u8 lna_gain;
    u8 mixer_gain;
    u8 tia_gain;
    u8 pga_gain;
    s16 total_gain_db;
    u16 rssi_offset;
};

struct wifi7_rf_temp_comp {
    s16 temp_points[RF_TEMP_POINTS];
    s16 gain_delta[RF_TEMP_POINTS];
    s16 phase_delta[RF_TEMP_POINTS];
    u32 last_temp;
    u32 update_interval;
};

struct wifi7_rf_cal_data {
    u32 timestamp;
    u32 temperature;
    u32 channel_freq;
    s16 i_dc_offset;
    s16 q_dc_offset;
    s16 i_gain_imbal;
    s16 q_gain_imbal;
    s16 phase_imbal;
    u32 pa_cal_volts[4];
    u32 checksum;
};

struct wifi7_rf_ops {
    int (*init)(struct wifi7_phy_dev *dev);
    void (*deinit)(struct wifi7_phy_dev *dev);
    
    int (*set_frequency)(struct wifi7_phy_dev *dev, u32 freq);
    int (*set_bandwidth)(struct wifi7_phy_dev *dev, u32 bw);
    int (*set_tx_power)(struct wifi7_phy_dev *dev, u8 power);
    int (*update_psd_mask)(struct wifi7_phy_dev *dev, u32 mask);
    
    int (*set_gain)(struct wifi7_phy_dev *dev, u8 chain, s8 gain_idx);
    int (*get_rssi)(struct wifi7_phy_dev *dev, u8 chain, s16 *rssi);
    
    int (*calibrate)(struct wifi7_phy_dev *dev, u32 cal_mask);
    int (*update_temp_comp)(struct wifi7_phy_dev *dev);
    
    int (*save_cal_data)(struct wifi7_phy_dev *dev,
                        struct wifi7_rf_cal_data *data);
    int (*load_cal_data)(struct wifi7_phy_dev *dev,
                        struct wifi7_rf_cal_data *data);
                        
    u32 freq_range[2];
    u32 supported_bw;
    u8 num_chains;
    bool ext_pa_present;
};

int wifi7_rf_register_ops(struct wifi7_phy_dev *dev,
                         struct wifi7_rf_ops *ops);
void wifi7_rf_unregister_ops(struct wifi7_phy_dev *dev);

#endif /* __WIFI7_RF_H */ 