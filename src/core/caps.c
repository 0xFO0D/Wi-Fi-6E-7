#include <linux/ieee80211.h>
#include "../../include/core/caps.h"

void wifi67_setup_ht_cap(struct ieee80211_supported_band *sband)
{
    struct ieee80211_sta_ht_cap *ht_cap = &sband->ht_cap;
    
    ht_cap->ht_supported = true;
    ht_cap->cap = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
                  IEEE80211_HT_CAP_SGI_40 |
                  IEEE80211_HT_CAP_DSSSCCK40;
    ht_cap->ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K;
    ht_cap->ampdu_density = IEEE80211_HT_MPDU_DENSITY_8;
    
    memset(&ht_cap->mcs, 0, sizeof(ht_cap->mcs));
    ht_cap->mcs.rx_mask[0] = 0xff;
    ht_cap->mcs.rx_mask[1] = 0xff;
    ht_cap->mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;
}

void wifi67_setup_vht_cap(struct ieee80211_supported_band *sband)
{
    struct ieee80211_sta_vht_cap *vht_cap = &sband->vht_cap;
    
    vht_cap->vht_supported = true;
    vht_cap->cap = IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454 |
                   IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ |
                   IEEE80211_VHT_CAP_RXLDPC |
                   IEEE80211_VHT_CAP_SHORT_GI_160 |
                   IEEE80211_VHT_CAP_TXSTBC |
                   IEEE80211_VHT_CAP_RXSTBC_4 |
                   IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK;
                   
    vht_cap->vht_mcs.rx_mcs_map = cpu_to_le16(0xfffa);
    vht_cap->vht_mcs.tx_mcs_map = cpu_to_le16(0xfffa);
}

void wifi67_setup_he_cap(struct ieee80211_supported_band *sband)
{
    struct ieee80211_sta_he_cap *he_cap = &sband->he_cap;
    
    he_cap->has_he = true;
    /* Setup HE capabilities */
    // TODO: HE capability setup ...
}

void wifi67_setup_eht_cap(struct ieee80211_supported_band *sband)
{
    struct ieee80211_sta_eht_cap *eht_cap = &sband->eht_cap;
    
    eht_cap->has_eht = true;
    /* Setup EHT capabilities */
    //  TODO: EHT capability setup ...
} 