#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ieee80211.h>
#include <linux/nl80211.h>
#include <net/cfg80211.h>
#include "../../include/core/bands.h"

/* Channel flags */
#define CHAN_FLAGS (IEEE80211_CHAN_INDOOR_ONLY | \
                   IEEE80211_CHAN_IR_CONCURRENT | \
                   IEEE80211_CHAN_NO_160MHZ)

/* 5 GHz band channels */
static const struct ieee80211_channel wifi67_5ghz_channels[] = {
    {
        .center_freq = 5180,
        .hw_value = 36,
        .max_power = 20,
        .max_antenna_gain = 0,
        .max_reg_power = 20,
        .flags = CHAN_FLAGS,
        .orig_flags = CHAN_FLAGS,
        .band = NL80211_BAND_5GHZ,
        .dfs_state_entered = 0,
    },
    {
        .center_freq = 5200,
        .hw_value = 40,
        .max_power = 20,
        .max_antenna_gain = 0,
        .max_reg_power = 20,
        .flags = CHAN_FLAGS,
        .orig_flags = CHAN_FLAGS,
        .band = NL80211_BAND_5GHZ,
        .dfs_state_entered = 0,
    },
    {
        .center_freq = 5220,
        .hw_value = 44,
        .max_power = 20,
        .max_antenna_gain = 0,
        .max_reg_power = 20,
        .flags = CHAN_FLAGS,
        .orig_flags = CHAN_FLAGS,
        .band = NL80211_BAND_5GHZ,
        .dfs_state_entered = 0,
    },
    {
        .center_freq = 5240,
        .hw_value = 48,
        .max_power = 20,
        .max_antenna_gain = 0,
        .max_reg_power = 20,
        .flags = CHAN_FLAGS,
        .orig_flags = CHAN_FLAGS,
        .band = NL80211_BAND_5GHZ,
        .dfs_state_entered = 0,
    },
};

/* 5 GHz band rates */
static const struct ieee80211_rate wifi67_5ghz_rates[] = {
    { .bitrate = 60, .hw_value = 0x0b },
    { .bitrate = 90, .hw_value = 0x0f },
    { .bitrate = 120, .hw_value = 0x0a },
    { .bitrate = 180, .hw_value = 0x0e },
    { .bitrate = 240, .hw_value = 0x09 },
    { .bitrate = 360, .hw_value = 0x0d },
    { .bitrate = 480, .hw_value = 0x08 },
    { .bitrate = 540, .hw_value = 0x0c },
};

/* 6 GHz band channels */
static const struct ieee80211_channel wifi67_6ghz_channels[] = {
    {
        .center_freq = 5955,
        .hw_value = 1,
        .max_power = 20,
        .max_antenna_gain = 0,
        .max_reg_power = 20,
        .flags = CHAN_FLAGS,
        .orig_flags = CHAN_FLAGS,
        .band = NL80211_BAND_6GHZ,
        .dfs_state_entered = 0,
    },
    {
        .center_freq = 5975,
        .hw_value = 5,
        .max_power = 20,
        .max_antenna_gain = 0,
        .max_reg_power = 20,
        .flags = CHAN_FLAGS,
        .orig_flags = CHAN_FLAGS,
        .band = NL80211_BAND_6GHZ,
        .dfs_state_entered = 0,
    },
    {
        .center_freq = 5995,
        .hw_value = 9,
        .max_power = 20,
        .max_antenna_gain = 0,
        .max_reg_power = 20,
        .flags = CHAN_FLAGS,
        .orig_flags = CHAN_FLAGS,
        .band = NL80211_BAND_6GHZ,
        .dfs_state_entered = 0,
    },
    {
        .center_freq = 6015,
        .hw_value = 13,
        .max_power = 20,
        .max_antenna_gain = 0,
        .max_reg_power = 20,
        .flags = CHAN_FLAGS,
        .orig_flags = CHAN_FLAGS,
        .band = NL80211_BAND_6GHZ,
        .dfs_state_entered = 0,
    },
};

/* 6 GHz band rates */
static const struct ieee80211_rate wifi67_6ghz_rates[] = {
    { .bitrate = 60, .hw_value = 0x1b },
    { .bitrate = 120, .hw_value = 0x1a },
    { .bitrate = 240, .hw_value = 0x19 },
    { .bitrate = 480, .hw_value = 0x18 },
};

/* Band definitions */
struct wifi67_band wifi67_band_5ghz = {
    .band_id = NL80211_BAND_5GHZ,
    .flags = 0,
    .n_channels = ARRAY_SIZE(wifi67_5ghz_channels),
    .channels = wifi67_5ghz_channels,
    .n_bitrates = ARRAY_SIZE(wifi67_5ghz_rates),
    .bitrates = wifi67_5ghz_rates,
    .ht_supported = true,
    .vht_supported = true,
    .he_supported = true,
    .eht_supported = false,
};
EXPORT_SYMBOL(wifi67_band_5ghz);

struct wifi67_band wifi67_band_6ghz = {
    .band_id = NL80211_BAND_6GHZ,
    .flags = 0,
    .n_channels = ARRAY_SIZE(wifi67_6ghz_channels),
    .channels = wifi67_6ghz_channels,
    .n_bitrates = ARRAY_SIZE(wifi67_6ghz_rates),
    .bitrates = wifi67_6ghz_rates,
    .ht_supported = false,
    .vht_supported = false,
    .he_supported = true,
    .eht_supported = true,
};
EXPORT_SYMBOL(wifi67_band_6ghz);

MODULE_LICENSE("GPL"); 