#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/nl80211.h>
#include <net/regulatory.h>
#include "../../include/core/wifi67.h"
#include "../../include/regulatory/reg_core.h"

static const struct ieee80211_regdomain wifi67_reg_def = {
    .n_reg_rules = 4,
    .alpha2 = "00",
    .reg_rules = {
        /* 5GHz UNII-1 */
        REG_RULE(5150-10, 5250+10, 40, 0, 23, 0),
        /* 5GHz UNII-2 */
        REG_RULE(5250-10, 5350+10, 40, 0, 23,
                NL80211_RRF_DFS | 0),
        /* 5GHz UNII-2e */
        REG_RULE(5470-10, 5725+10, 40, 0, 23,
                NL80211_RRF_DFS | 0),
        /* 6GHz */
        REG_RULE(5925-10, 7125+10, 160, 0, 30, 0),
    }
};

static void wifi67_reg_notifier(struct wiphy *wiphy,
                              struct regulatory_request *request)
{
    struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
    struct wifi67_priv *priv = hw->priv;

    /* Update channel flags based on regulatory domain */
    wifi67_update_channel_flags(priv, request->alpha2);
}

int wifi67_regulatory_init(struct wifi67_priv *priv)
{
    struct wiphy *wiphy = priv->hw->wiphy;
    int ret;

    wiphy->regulatory_flags |= REGULATORY_STRICT_REG |
                              REGULATORY_DISABLE_BEACON_HINTS;
    
    wiphy->reg_notifier = wifi67_reg_notifier;
    
    ret = regulatory_set_wiphy_regd(wiphy, &wifi67_reg_def);
    if (ret)
        return ret;

    return 0;
}
EXPORT_SYMBOL(wifi67_regulatory_init);

