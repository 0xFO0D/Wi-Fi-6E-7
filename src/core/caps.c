#include <linux/module.h>
#include <linux/ieee80211.h>
#include "../../include/core/caps.h"

void wifi67_setup_hw_caps(struct wifi67_priv *priv)
{
    struct ieee80211_hw *hw = priv->hw;

    /* Set hardware capabilities */
    hw->wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) |
                                BIT(NL80211_IFTYPE_AP);

    hw->wiphy->flags |= WIPHY_FLAG_HAS_CHANNEL_SWITCH |
                       WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL |
                       WIPHY_FLAG_SUPPORTS_5_10_MHZ |
                       WIPHY_FLAG_HAS_AP_SME |
                       WIPHY_FLAG_SUPPORTS_WMM_ADMISSION;

    hw->queues = IEEE80211_NUM_ACS;
    hw->offchannel_tx_hw_queue = IEEE80211_NUM_ACS;

    /* Check supported features */
    if (priv->features.has_6ghz) {
        hw->wiphy->flags |= WIPHY_FLAG_6GHZ_CAPABLE;
    }

    if (priv->features.has_5ghz) {
        hw->wiphy->bands[NL80211_BAND_5GHZ] = &priv->bands[NL80211_BAND_5GHZ];
    }

    /* Set TX/RX mask based on spatial streams */
    if (priv->features.has_16ss) {
        hw->max_tx_aggregation_subframes = 64;
        hw->max_rx_aggregation_subframes = 128;
    } else if (priv->features.has_8ss) {
        hw->max_tx_aggregation_subframes = 32;
        hw->max_rx_aggregation_subframes = 64;
    } else {
        hw->max_tx_aggregation_subframes = 16;
        hw->max_rx_aggregation_subframes = 32;
    }

    /* Set advanced capabilities */
    if (priv->features.has_mu_mimo) {
        hw->wiphy->flags |= WIPHY_FLAG_SUPPORTS_MU_MIMO;
    }

    if (priv->features.has_ofdma) {
        hw->wiphy->flags |= WIPHY_FLAG_SUPPORTS_OFDMA;
    }
}

void wifi67_setup_band_rates(struct ieee80211_supported_band *sband,
                           struct ieee80211_rate *rates,
                           int n_rates)
{
    sband->bitrates = rates;
    sband->n_bitrates = n_rates;
}

void wifi67_setup_band_channels(struct ieee80211_supported_band *sband,
                              struct ieee80211_channel *channels,
                              int n_channels)
{
    sband->channels = channels;
    sband->n_channels = n_channels;
}

void wifi67_setup_band_capabilities(struct ieee80211_supported_band *sband)
{
    struct ieee80211_sta_ht_cap *ht_cap = &sband->ht_cap;
    struct ieee80211_sta_vht_cap *vht_cap = &sband->vht_cap;

    /* Setup HT capabilities */
    ht_cap->ht_supported = true;
    ht_cap->cap = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
                  IEEE80211_HT_CAP_SGI_40 |
                  IEEE80211_HT_CAP_DSSSCCK40 |
                  IEEE80211_HT_CAP_MAX_AMSDU;
    ht_cap->ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K;
    ht_cap->ampdu_density = IEEE80211_HT_MPDU_DENSITY_8;

    /* Setup VHT capabilities */
    vht_cap->vht_supported = true;
    vht_cap->cap = IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454 |
                   IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ |
                   IEEE80211_VHT_CAP_RXLDPC |
                   IEEE80211_VHT_CAP_SHORT_GI_160 |
                   IEEE80211_VHT_CAP_TXSTBC |
                   IEEE80211_VHT_CAP_RXSTBC_4 |
                   IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK;

    /* Setup HE capabilities */
    wifi67_setup_he_cap((struct ieee80211_sta_he_cap *)&sband->iftype_data->he_cap);
    
    /* Setup EHT capabilities */
    wifi67_setup_eht_cap((struct ieee80211_sta_eht_cap *)&sband->iftype_data->eht_cap);
}

void wifi67_setup_he_cap(struct ieee80211_sta_he_cap *he_cap)
{
    /* HE capability setup */
    he_cap->has_he = true;
    he_cap->he_cap_elem.phy_cap_info[0] = IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ |
                                         IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G |
                                         IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G;
    
    /* Add more HE capabilities setup */
}

void wifi67_setup_eht_cap(struct ieee80211_sta_eht_cap *eht_cap)
{
    /* EHT capability setup */
    eht_cap->has_eht = true;
    eht_cap->eht_cap_elem.phy_cap_info[0] = IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ |
                                           IEEE80211_EHT_PHY_CAP0_242_TONE_RU_GT20MHZ;
    
    /* Add more EHT capabilities setup */
}

EXPORT_SYMBOL_GPL(wifi67_setup_hw_caps);
EXPORT_SYMBOL_GPL(wifi67_setup_band_rates);
EXPORT_SYMBOL_GPL(wifi67_setup_band_channels);
EXPORT_SYMBOL_GPL(wifi67_setup_band_capabilities);
EXPORT_SYMBOL_GPL(wifi67_setup_he_cap);
EXPORT_SYMBOL_GPL(wifi67_setup_eht_cap);