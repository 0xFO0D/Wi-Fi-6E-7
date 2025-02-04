#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include "wifi7_spectrum.h"
#include "wifi7_phy.h"
#include "../hal/wifi7_rf.h"
#include "../regulatory/wifi7_reg.h"

#define AFC_RESPONSE_TIMEOUT_MS 100
#define PSD_UPDATE_DELAY_US 50
#define TXPOWER_STEP_DBM 2

static u32 calculate_psd_mask(u32 freq, u32 width, const bool *mask)
{
    u32 psd = 0;
    int i, start_idx;
    
    start_idx = (freq - (width / 2)) / WIFI7_MIN_CHANNEL_WIDTH;
    
    for (i = 0; i < width / WIFI7_MIN_CHANNEL_WIDTH; i++) {
        if (mask[start_idx + i])
            psd |= BIT(i);
    }
    
    return psd;
}

static int configure_rf_channel(struct wifi7_phy_dev *dev,
                              u32 freq, u32 width)
{
    struct wifi7_rf_ops *rf_ops = dev->rf_ops;
    u32 actual_freq;
    int ret;

    if (!rf_ops || !rf_ops->set_frequency)
        return -ENOTSUPP;

    actual_freq = clamp(freq, rf_ops->freq_range[0], 
                             rf_ops->freq_range[1]);

    ret = rf_ops->set_frequency(dev, actual_freq);
    if (ret)
        return ret;

    if (rf_ops->set_bandwidth)
        ret = rf_ops->set_bandwidth(dev, width);

    udelay(PSD_UPDATE_DELAY_US);
    return ret;
}

static int update_tx_power(struct wifi7_phy_dev *dev,
                          u8 power, u8 afc_limit)
{
    struct wifi7_rf_ops *rf_ops = dev->rf_ops;
    u8 actual_power;
    int ret;

    if (!rf_ops || !rf_ops->set_tx_power)
        return -ENOTSUPP;

    actual_power = min_t(u8, power, afc_limit);
    actual_power = rounddown(actual_power, TXPOWER_STEP_DBM);

    ret = rf_ops->set_tx_power(dev, actual_power);
    if (ret)
        return ret;

    dev->curr_power = actual_power;
    return 0;
}

int wifi7_spectrum_init(struct wifi7_phy_dev *dev)
{
    struct wifi7_spectrum_info *spec;

    if (!dev)
        return -EINVAL;

    spec = kzalloc(sizeof(*spec), GFP_KERNEL);
    if (!spec)
        return -ENOMEM;

    spec->center_freq = dev->default_freq;
    spec->bandwidth = WIFI7_MIN_CHANNEL_WIDTH;
    spec->tx_power = dev->default_power;
    spec->afc_power_limit = WIFI7_AFC_MAX_POWER;
    spec->dynamic_bw = true;

    dev->spectrum = spec;
    return 0;
}

void wifi7_spectrum_deinit(struct wifi7_phy_dev *dev)
{
    if (dev && dev->spectrum) {
        kfree(dev->spectrum);
        dev->spectrum = NULL;
    }
}

int wifi7_spectrum_configure(struct wifi7_phy_dev *dev,
                           struct wifi7_spectrum_info *info)
{
    struct wifi7_spectrum_info *spec = dev->spectrum;
    u32 psd_mask;
    int ret;

    if (!dev || !spec || !info)
        return -EINVAL;

    if (info->bandwidth > WIFI7_MAX_CHANNEL_WIDTH ||
        info->bandwidth < WIFI7_MIN_CHANNEL_WIDTH ||
        info->bandwidth % WIFI7_MIN_CHANNEL_WIDTH)
        return -EINVAL;

    ret = configure_rf_channel(dev, info->center_freq, 
                             info->bandwidth);
    if (ret)
        return ret;

    if (memcmp(spec->psd_mask, info->psd_mask,
               sizeof(spec->psd_mask))) {
        psd_mask = calculate_psd_mask(info->center_freq,
                                    info->bandwidth,
                                    info->psd_mask);
        ret = dev->rf_ops->update_psd_mask(dev, psd_mask);
        if (ret)
            return ret;
        memcpy(spec->psd_mask, info->psd_mask,
               sizeof(spec->psd_mask));
    }

    ret = update_tx_power(dev, info->tx_power,
                         info->afc_power_limit);
    if (ret)
        return ret;

    memcpy(spec, info, sizeof(*spec));
    return 0;
}

int wifi7_spectrum_update_afc(struct wifi7_phy_dev *dev,
                            struct wifi7_afc_req *req)
{
    struct wifi7_spectrum_info *spec = dev->spectrum;
    struct wifi7_regulatory *reg = dev->regulatory;
    u32 freq_range[2];
    u8 max_power;
    int ret;

    if (!dev || !spec || !req)
        return -EINVAL;

    freq_range[0] = req->freq_start;
    freq_range[1] = req->freq_end;

    ret = wifi7_regulatory_check_freq_range(reg, freq_range);
    if (ret)
        return ret;

    ret = wifi7_regulatory_get_max_power(reg, freq_range,
                                       req->geo_area,
                                       &max_power);
    if (ret)
        return ret;

    spec->afc_power_limit = min_t(u8, max_power,
                                 req->max_power);

    ret = update_tx_power(dev, spec->tx_power,
                         spec->afc_power_limit);
    if (ret)
        return ret;

    return 0;
} 