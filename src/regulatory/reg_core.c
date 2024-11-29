#include <linux/module.h>
#include <linux/ieee80211.h>
#include "../../include/regulatory/reg_core.h"
#include "../../include/core/wifi67.h"
#include "../../include/core/wifi67_debug.h"

void wifi67_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request)
{
    struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
    struct wifi67_priv *priv = hw->priv;
    struct ieee80211_regdomain *regd;
    struct ieee80211_reg_rule *rule;
    int ret;

    /* Create a new regulatory domain */
    regd = kzalloc(sizeof(*regd) + sizeof(struct ieee80211_reg_rule) * 2,
                   GFP_KERNEL);
    if (!regd)
        return;

    /* Configure basic regulatory domain info */
    regd->n_reg_rules = 2;
    regd->alpha2[0] = request->alpha2[0];
    regd->alpha2[1] = request->alpha2[1];
    regd->dfs_region = request->dfs_region;

    /* Configure 5GHz rule */
    rule = &regd->reg_rules[0];
    rule->freq_range.start_freq_khz = MHZ_TO_KHZ(5150);
    rule->freq_range.end_freq_khz = MHZ_TO_KHZ(5850);
    rule->freq_range.max_bandwidth_khz = MHZ_TO_KHZ(80);
    rule->power_rule.max_antenna_gain = DBI_TO_MBI(6);
    rule->power_rule.max_eirp = DBM_TO_MBM(23);
    rule->flags = NL80211_RRF_NO_OUTDOOR;

    /* Configure 6GHz rule */
    rule = &regd->reg_rules[1];
    rule->freq_range.start_freq_khz = MHZ_TO_KHZ(5925);
    rule->freq_range.end_freq_khz = MHZ_TO_KHZ(7125);
    rule->freq_range.max_bandwidth_khz = MHZ_TO_KHZ(160);
    rule->power_rule.max_antenna_gain = DBI_TO_MBI(6);
    rule->power_rule.max_eirp = DBM_TO_MBM(23);
    rule->flags = NL80211_RRF_NO_OUTDOOR;

    /* Apply the new regulatory domain */
    ret = regulatory_set_wiphy_regd(wiphy, regd);
    if (ret)
        wifi67_err(priv, "Failed to set regulatory domain: %d\n", ret);

    kfree(regd);
}

int wifi67_reg_init(struct wifi67_priv *priv)
{
    struct ieee80211_hw *hw = priv->hw;
    struct wiphy *wiphy = hw->wiphy;

    /* Set regulatory hooks */
    wiphy->reg_notifier = wifi67_reg_notifier;

    /* Set supported bands */
    wiphy->bands[NL80211_BAND_5GHZ] = &wifi67_band_5ghz;
    wiphy->bands[NL80211_BAND_6GHZ] = &wifi67_band_6ghz;

    return 0;
}

void wifi67_reg_deinit(struct wifi67_priv *priv)
{
    /* Nothing to clean up for now */
}

