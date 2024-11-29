#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ieee80211.h>
#include <linux/nl80211.h>
#include <net/cfg80211.h>
#include "../../include/core/caps.h"

void wifi67_setup_ht_cap(struct ieee80211_supported_band *sband)
{
    struct ieee80211_sta_ht_cap *ht_cap = &sband->ht_cap;
    
    ht_cap->ht_supported = true;
    ht_cap->cap = IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
                  IEEE80211_HT_CAP_SGI_40 |
                  IEEE80211_HT_CAP_DSSSCCK40 |
                  IEEE80211_HT_CAP_SM_PS |
                  IEEE80211_HT_CAP_GRN_FLD |
                  IEEE80211_HT_CAP_SGI_20 |
                  IEEE80211_HT_CAP_RX_STBC |
                  IEEE80211_HT_CAP_TX_STBC |
                  IEEE80211_HT_CAP_LDPC_CODING;
                  
    ht_cap->ampdu_factor = IEEE80211_HT_MAX_AMPDU_64K;
    ht_cap->ampdu_density = IEEE80211_HT_MPDU_DENSITY_8;
    
    memset(&ht_cap->mcs, 0, sizeof(ht_cap->mcs));
    ht_cap->mcs.rx_mask[0] = 0xff;
    ht_cap->mcs.rx_mask[1] = 0xff;
    ht_cap->mcs.rx_mask[2] = 0xff;
    ht_cap->mcs.rx_mask[3] = 0xff;
    ht_cap->mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED |
                           IEEE80211_HT_MCS_TX_RX_DIFF |
                           ((1 << IEEE80211_HT_MCS_TX_STREAMS) - 1);
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
                   IEEE80211_VHT_CAP_SU_BEAMFORMER_CAPABLE |
                   IEEE80211_VHT_CAP_SU_BEAMFORMEE_CAPABLE |
                   IEEE80211_VHT_CAP_MU_BEAMFORMER_CAPABLE |
                   IEEE80211_VHT_CAP_MU_BEAMFORMEE_CAPABLE |
                   IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK;
                   
    vht_cap->vht_mcs.rx_mcs_map = cpu_to_le16(0xfffa);
    vht_cap->vht_mcs.tx_mcs_map = cpu_to_le16(0xfffa);
    vht_cap->vht_mcs.rx_highest = cpu_to_le16(2400);
    vht_cap->vht_mcs.tx_highest = cpu_to_le16(2400);
}

void wifi67_setup_he_cap(struct ieee80211_supported_band *sband)
{
    struct ieee80211_sta_he_cap *he_cap = &sband->he_cap;
    __le16 mcs_map;
    
    he_cap->has_he = true;

    /* MAC capabilities */
    he_cap->he_cap_elem.mac_cap_info[0] =
        IEEE80211_HE_MAC_CAP0_HTC_HE |
        IEEE80211_HE_MAC_CAP0_TWT_REQ |
        IEEE80211_HE_MAC_CAP0_TWT_RES |
        IEEE80211_HE_MAC_CAP0_DYNAMIC_FRAG_NOT_SUPP;
    
    he_cap->he_cap_elem.mac_cap_info[1] =
        IEEE80211_HE_MAC_CAP1_TF_MAC_PAD_DUR_16US |
        IEEE80211_HE_MAC_CAP1_MULTI_TID_AGG_RX_QOS_8;

    he_cap->he_cap_elem.mac_cap_info[2] =
        IEEE80211_HE_MAC_CAP2_BSR |
        IEEE80211_HE_MAC_CAP2_MU_CASCADING |
        IEEE80211_HE_MAC_CAP2_ACK_EN;

    /* PHY capabilities */
    he_cap->he_cap_elem.phy_cap_info[0] =
        IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_40MHZ_80MHZ_IN_5G |
        IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_160MHZ_IN_5G |
        IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G;
    
    he_cap->he_cap_elem.phy_cap_info[1] =
        IEEE80211_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD |
        IEEE80211_HE_PHY_CAP1_HE_LTF_AND_GI_FOR_HE_PPDUS_0_8US |
        IEEE80211_HE_PHY_CAP1_MIDAMBLE_RX_TX_MAX_NSTS;
    
    he_cap->he_cap_elem.phy_cap_info[2] =
        IEEE80211_HE_PHY_CAP2_NDP_4x_LTF_AND_3_2US |
        IEEE80211_HE_PHY_CAP2_STBC_TX_UNDER_80MHZ |
        IEEE80211_HE_PHY_CAP2_STBC_RX_UNDER_80MHZ |
        IEEE80211_HE_PHY_CAP2_UL_MU_FULL_MU_MIMO |
        IEEE80211_HE_PHY_CAP2_UL_MU_PARTIAL_MU_MIMO;

    /* PPE Thresholds */
    he_cap->ppe_thres[0] = 0x61;
    he_cap->ppe_thres[1] = 0x1c;
    he_cap->ppe_thres[2] = 0xc7;
    he_cap->ppe_thres[3] = 0x71;

    /* MCS map */
    mcs_map = cpu_to_le16(0xfffa);
    memcpy(&he_cap->he_mcs_nss_supp.rx_mcs_80, &mcs_map, sizeof(mcs_map));
    memcpy(&he_cap->he_mcs_nss_supp.tx_mcs_80, &mcs_map, sizeof(mcs_map));
    
    mcs_map = cpu_to_le16(0xfffa);
    memcpy(&he_cap->he_mcs_nss_supp.rx_mcs_160, &mcs_map, sizeof(mcs_map));
    memcpy(&he_cap->he_mcs_nss_supp.tx_mcs_160, &mcs_map, sizeof(mcs_map));
    
    mcs_map = cpu_to_le16(0xfffa);
    memcpy(&he_cap->he_mcs_nss_supp.rx_mcs_80p80, &mcs_map, sizeof(mcs_map));
    memcpy(&he_cap->he_mcs_nss_supp.tx_mcs_80p80, &mcs_map, sizeof(mcs_map));
}

void wifi67_setup_eht_cap(struct ieee80211_supported_band *sband)
{
    struct ieee80211_eht_cap_elem_fixed *eht_cap = &sband->eht_cap.eht_cap_elem;
    
    sband->eht_cap.has_eht = true;

    /* MAC capabilities */
    eht_cap->mac_cap_info[0] =
        IEEE80211_EHT_MAC_CAP0_EPCS_PRIO_ACCESS |
        IEEE80211_EHT_MAC_CAP0_OM_CONTROL |
        IEEE80211_EHT_MAC_CAP0_TRIG_TXOP_SHARING_MODE1 |
        IEEE80211_EHT_MAC_CAP0_RESTRICTED_TWT |
        IEEE80211_EHT_MAC_CAP0_SCS_TRAFFIC_DESC;

    /* PHY capabilities */
    eht_cap->phy_cap_info[0] =
        IEEE80211_EHT_PHY_CAP0_320MHZ_IN_6GHZ |
        IEEE80211_EHT_PHY_CAP0_242_TONE_RU_GT20MHZ |
        IEEE80211_EHT_PHY_CAP0_NDP_4_EHT_LFT_32_GI |
        IEEE80211_EHT_PHY_CAP0_PARTIAL_BW_UL_MU_MIMO |
        IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMER |
        IEEE80211_EHT_PHY_CAP0_SU_BEAMFORMEE;
        
    eht_cap->phy_cap_info[1] =
        IEEE80211_EHT_PHY_CAP1_PARTIAL_BW_UL_MU_MIMO |
        IEEE80211_EHT_PHY_CAP1_PARTIAL_BW_DL_MU_MIMO |
        IEEE80211_EHT_PHY_CAP1_PPE_THRESHOLD_PRESENT;

    /* MCS/NSS map */
    memset(&sband->eht_cap.eht_mcs_nss_supp, 0xff,
           sizeof(struct ieee80211_eht_mcs_nss_supp));
}

EXPORT_SYMBOL(wifi67_setup_ht_cap);
EXPORT_SYMBOL(wifi67_setup_vht_cap);
EXPORT_SYMBOL(wifi67_setup_he_cap);
EXPORT_SYMBOL(wifi67_setup_eht_cap);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WiFi 6E/7 Capability Setup");
MODULE_AUTHOR("0XFO0D");