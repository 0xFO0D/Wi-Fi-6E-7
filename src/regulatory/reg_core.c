#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/nl80211.h>
#include <net/cfg80211.h>
#include "../../include/core/wifi67.h"
#include "../../include/regulatory/reg_core.h"

static const struct ieee80211_regdomain wifi67_reg_def = {
    .n_reg_rules = 3,
    .alpha2 = "00",
    .reg_rules = {
        /* 2.4 GHz */
        REG_RULE(2412-10, 2484+10, 40, 0, 20, 0),
        /* 5 GHz */
        REG_RULE(5180-10, 5320+10, 80, 0, 23, 
                 NL80211_RRF_AUTO_BW | NL80211_RRF_DFS),
        /* 6 GHz */
        REG_RULE(5945-10, 7125+10, 320, 0, 23,
                 NL80211_RRF_AUTO_BW | NL80211_RRF_NO_OUTDOOR),
    }
};

void wifi67_reg_notifier(struct wiphy *wiphy,
                        struct regulatory_request *request)
{
    struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
    struct wifi67_priv *priv = hw->priv;

    /* Update channel flags based on new regulatory domain */
    wifi67_update_channel_flags(priv, request->alpha2);
}

void wifi67_update_channel_flags(struct wifi67_priv *priv, const char *alpha2)
{
    struct wiphy *wiphy = priv->hw->wiphy;
    struct ieee80211_supported_band *sband;
    struct ieee80211_channel *chan;
    int i;

    /* Update 2.4 GHz band */
    sband = wiphy->bands[NL80211_BAND_2GHZ];
    if (sband) {
        for (i = 0; i < sband->n_channels; i++) {
            chan = &sband->channels[i];
            chan->flags = 0;
            
            /* Set DFS and NO_IR flags if needed */
            if (chan->center_freq >= 2412 && chan->center_freq <= 2484) {
                if (strncmp(alpha2, "JP", 2) == 0)
                    chan->flags |= IEEE80211_CHAN_NO_IR;
            }
        }
    }

    /* Update 5 GHz band */
    sband = wiphy->bands[NL80211_BAND_5GHZ];
    if (sband) {
        for (i = 0; i < sband->n_channels; i++) {
            chan = &sband->channels[i];
            chan->flags = 0;
            
            /* Set DFS flags for 5.25-5.35 GHz and 5.47-5.725 GHz */
            if ((chan->center_freq >= 5250 && chan->center_freq <= 5350) ||
                (chan->center_freq >= 5470 && chan->center_freq <= 5725)) {
                chan->flags |= IEEE80211_CHAN_RADAR;
                chan->flags |= IEEE80211_CHAN_NO_IR;
            }
        }
    }

    /* Update 6 GHz band */
    sband = wiphy->bands[NL80211_BAND_6GHZ];
    if (sband) {
        for (i = 0; i < sband->n_channels; i++) {
            chan = &sband->channels[i];
            chan->flags = 0;
            
            /* Set NO_IR flag for all 6 GHz channels */
            chan->flags |= IEEE80211_CHAN_NO_IR;
        }
    }
}

int wifi67_regulatory_init(struct wifi67_priv *priv)
{
    struct wiphy *wiphy = priv->hw->wiphy;
    int ret;

    /* Set regulatory callbacks */
    wiphy->reg_notifier = wifi67_reg_notifier;

    /* Set initial regulatory domain */
    ret = regulatory_set_wiphy_regd(wiphy, (struct ieee80211_regdomain *)&wifi67_reg_def);
    if (ret)
        return ret;

    /* Update channel flags */
    wifi67_update_channel_flags(priv, wifi67_reg_def.alpha2);

    return 0;
}
EXPORT_SYMBOL(wifi67_regulatory_init);

