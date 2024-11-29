#include <net/mac80211.h>
#include "../../include/core/bands.h"

#define CHAN2G(_freq, _idx)  { \
    .band = NL80211_BAND_2GHZ, \
    .center_freq = (_freq), \
    .hw_value = (_idx), \
    .max_power = 20, \
}

#define CHAN5G(_freq, _idx)  { \
    .band = NL80211_BAND_5GHZ, \
    .center_freq = (_freq), \
    .hw_value = (_idx), \
    .max_power = 20, \
}

#define CHAN6G(_freq, _idx)  { \
    .band = NL80211_BAND_6GHZ, \
    .center_freq = (_freq), \
    .hw_value = (_idx), \
    .max_power = 20, \
}

#define RATE(_bitrate, _hw_rate, _flags) { \
    .bitrate = (_bitrate), \
    .flags = (_flags), \
    .hw_value = (_hw_rate), \
}

struct ieee80211_channel wifi67_2ghz_channels[] = {
    CHAN2G(2412, 0),
    CHAN2G(2417, 1),
    CHAN2G(2422, 2),
    CHAN2G(2427, 3),
    CHAN2G(2432, 4),
    CHAN2G(2437, 5),
    CHAN2G(2442, 6),
    CHAN2G(2447, 7),
    CHAN2G(2452, 8),
    CHAN2G(2457, 9),
    CHAN2G(2462, 10),
    CHAN2G(2467, 11),
    CHAN2G(2472, 12),
    CHAN2G(2484, 13),
};

struct ieee80211_rate wifi67_2ghz_rates[] = {
    RATE(10, 0x1, 0),
    RATE(20, 0x2, IEEE80211_RATE_SHORT_PREAMBLE),
    RATE(55, 0x4, IEEE80211_RATE_SHORT_PREAMBLE),
    RATE(110, 0x8, IEEE80211_RATE_SHORT_PREAMBLE),
    RATE(60, 0x10, 0),
    RATE(90, 0x20, 0),
    RATE(120, 0x40, 0),
    RATE(180, 0x80, 0),
    RATE(240, 0x100, 0),
    RATE(360, 0x200, 0),
    RATE(480, 0x400, 0),
    RATE(540, 0x800, 0),
};

struct ieee80211_channel wifi67_5ghz_channels[] = {
    CHAN5G(5180, 36),
    CHAN5G(5200, 40),
    CHAN5G(5220, 44),
    CHAN5G(5240, 48),
    CHAN5G(5260, 52),
    CHAN5G(5280, 56),
    CHAN5G(5300, 60),
    CHAN5G(5320, 64),
    CHAN5G(5500, 100),
    CHAN5G(5520, 104),
    CHAN5G(5540, 108),
    CHAN5G(5560, 112),
    CHAN5G(5580, 116),
    CHAN5G(5600, 120),
    CHAN5G(5620, 124),
    CHAN5G(5640, 128),
    CHAN5G(5660, 132),
    CHAN5G(5680, 136),
    CHAN5G(5700, 140),
    CHAN5G(5720, 144),
    CHAN5G(5745, 149),
    CHAN5G(5765, 153),
    CHAN5G(5785, 157),
    CHAN5G(5805, 161),
    CHAN5G(5825, 165),
};

struct ieee80211_rate wifi67_5ghz_rates[] = {
    RATE(60, 0x10, 0),
    RATE(90, 0x20, 0),
    RATE(120, 0x40, 0),
    RATE(180, 0x80, 0),
    RATE(240, 0x100, 0),
    RATE(360, 0x200, 0),
    RATE(480, 0x400, 0),
    RATE(540, 0x800, 0),
};

struct ieee80211_channel wifi67_6ghz_channels[] = {
    CHAN6G(5955, 1),
    CHAN6G(5975, 5),
    CHAN6G(5995, 9),
    CHAN6G(6015, 13),
    CHAN6G(6035, 17),
    CHAN6G(6055, 21),
    CHAN6G(6075, 25),
    CHAN6G(6095, 29),
    CHAN6G(6115, 33),
    CHAN6G(6135, 37),
    CHAN6G(6155, 41),
    CHAN6G(6175, 45),
    CHAN6G(6195, 49),
    CHAN6G(6215, 53),
    CHAN6G(6235, 57),
    CHAN6G(6255, 61),
    CHAN6G(6275, 65),
    CHAN6G(6295, 69),
    CHAN6G(6315, 73),
    CHAN6G(6335, 77),
    CHAN6G(6355, 81),
    CHAN6G(6375, 85),
    CHAN6G(6395, 89),
    CHAN6G(6415, 93),
    CHAN6G(6435, 97),
};

struct ieee80211_rate wifi67_6ghz_rates[] = {
    RATE(60, 0x10, 0),
    RATE(90, 0x20, 0),
    RATE(120, 0x40, 0),
    RATE(180, 0x80, 0),
    RATE(240, 0x100, 0),
    RATE(360, 0x200, 0),
    RATE(480, 0x400, 0),
    RATE(540, 0x800, 0),
};

struct ieee80211_sta_ht_cap wifi67_2ghz_ht_cap = {
    .ht_supported = true,
    .cap = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
           IEEE80211_HT_CAP_SGI_40 |
           IEEE80211_HT_CAP_DSSSCCK40 |
           IEEE80211_HT_CAP_MAX_AMSDU,
    .ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K,
    .ampdu_density = IEEE80211_HT_MPDU_DENSITY_8,
    .mcs = {
        .rx_mask = {0xff, 0xff},
        .rx_highest = cpu_to_le16(300),
        .tx_params = IEEE80211_HT_MCS_TX_DEFINED,
    },
};

struct ieee80211_sta_vht_cap wifi67_2ghz_vht_cap = {
    .vht_supported = true,
    .cap = IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454 |
           IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ |
           IEEE80211_VHT_CAP_RXLDPC |
           IEEE80211_VHT_CAP_SHORT_GI_80 |
           IEEE80211_VHT_CAP_SHORT_GI_160 |
           IEEE80211_VHT_CAP_TXSTBC |
           IEEE80211_VHT_CAP_RXSTBC_1 |
           IEEE80211_VHT_CAP_RXSTBC_2 |
           IEEE80211_VHT_CAP_RXSTBC_3 |
           IEEE80211_VHT_CAP_RXSTBC_4 |
           IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK,
    .vht_mcs = {
        .rx_mcs_map = cpu_to_le16(0xfffa),
        .rx_highest = cpu_to_le16(2400),
        .tx_mcs_map = cpu_to_le16(0xfffa),
        .tx_highest = cpu_to_le16(2400),
    },
};

struct ieee80211_sta_ht_cap wifi67_5ghz_ht_cap = {
    .ht_supported = true,
    .cap = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
           IEEE80211_HT_CAP_SGI_40 |
           IEEE80211_HT_CAP_DSSSCCK40 |
           IEEE80211_HT_CAP_MAX_AMSDU,
    .ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K,
    .ampdu_density = IEEE80211_HT_MPDU_DENSITY_8,
    .mcs = {
        .rx_mask = {0xff, 0xff},
        .rx_highest = cpu_to_le16(300),
        .tx_params = IEEE80211_HT_MCS_TX_DEFINED,
    },
};

struct ieee80211_sta_vht_cap wifi67_5ghz_vht_cap = {
    .vht_supported = true,
    .cap = IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454 |
           IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ |
           IEEE80211_VHT_CAP_RXLDPC |
           IEEE80211_VHT_CAP_SHORT_GI_80 |
           IEEE80211_VHT_CAP_SHORT_GI_160 |
           IEEE80211_VHT_CAP_TXSTBC |
           IEEE80211_VHT_CAP_RXSTBC_1 |
           IEEE80211_VHT_CAP_RXSTBC_2 |
           IEEE80211_VHT_CAP_RXSTBC_3 |
           IEEE80211_VHT_CAP_RXSTBC_4 |
           IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK,
    .vht_mcs = {
        .rx_mcs_map = cpu_to_le16(0xfffa),
        .rx_highest = cpu_to_le16(2400),
        .tx_mcs_map = cpu_to_le16(0xfffa),
        .tx_highest = cpu_to_le16(2400),
    },
};

struct ieee80211_sta_ht_cap wifi67_6ghz_ht_cap = {
    .ht_supported = true,
    .cap = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
           IEEE80211_HT_CAP_SGI_40 |
           IEEE80211_HT_CAP_DSSSCCK40 |
           IEEE80211_HT_CAP_MAX_AMSDU,
    .ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K,
    .ampdu_density = IEEE80211_HT_MPDU_DENSITY_8,
    .mcs = {
        .rx_mask = {0xff, 0xff},
        .rx_highest = cpu_to_le16(300),
        .tx_params = IEEE80211_HT_MCS_TX_DEFINED,
    },
};

struct ieee80211_sta_vht_cap wifi67_6ghz_vht_cap = {
    .vht_supported = true,
    .cap = IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454 |
           IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ |
           IEEE80211_VHT_CAP_RXLDPC |
           IEEE80211_VHT_CAP_SHORT_GI_80 |
           IEEE80211_VHT_CAP_SHORT_GI_160 |
           IEEE80211_VHT_CAP_TXSTBC |
           IEEE80211_VHT_CAP_RXSTBC_1 |
           IEEE80211_VHT_CAP_RXSTBC_2 |
           IEEE80211_VHT_CAP_RXSTBC_3 |
           IEEE80211_VHT_CAP_RXSTBC_4 |
           IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK,
    .vht_mcs = {
        .rx_mcs_map = cpu_to_le16(0xfffa),
        .rx_highest = cpu_to_le16(2400),
        .tx_mcs_map = cpu_to_le16(0xfffa),
        .tx_highest = cpu_to_le16(2400),
    },
}; 