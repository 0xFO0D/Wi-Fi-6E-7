#include <linux/module.h>
#include <linux/delay.h>
#include <linux/pci.h>
#include <net/cfg80211.h>

#include "../../include/regulatory/reg_core.h"
#include "../../include/core/wifi67.h"

static void wifi67_reg_dfs_worker(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct wifi67_regulatory *reg = container_of(dwork, struct wifi67_regulatory, dfs_work);
    struct wifi67_priv *priv = container_of(reg, struct wifi67_priv, reg);

    if (!reg->cac_started)
        return;

    /* CAC completed successfully */
    reg->cac_started = false;

    cfg80211_cac_event(priv->netdev,
                      &reg->dfs_chan_def,
                      NL80211_RADAR_CAC_FINISHED,
                      GFP_KERNEL);
}

int wifi67_regulatory_init(struct wifi67_priv *priv)
{
    struct wifi67_regulatory *reg = &priv->reg;

    reg->dfs_enabled = false;
    reg->cac_started = false;
    reg->n_rules = 0;
    INIT_DELAYED_WORK(&reg->dfs_work, wifi67_reg_dfs_worker);

    return 0;
}

void wifi67_regulatory_deinit(struct wifi67_priv *priv)
{
    struct wifi67_regulatory *reg = &priv->reg;

    if (reg->cac_started) {
        cancel_delayed_work_sync(&reg->dfs_work);
        reg->cac_started = false;
    }
}

void wifi67_reg_notifier(struct wiphy *wiphy,
                        struct regulatory_request *request)
{
    struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
    struct wifi67_priv *priv = hw->priv;
    struct ieee80211_regdomain *regd;

    /* Create a new regulatory domain based on the request */
    regd = kzalloc(sizeof(*regd) + sizeof(struct ieee80211_reg_rule) * 2,
                   GFP_KERNEL);
    if (!regd)
        return;

    /* Set up basic regulatory domain info */
    memcpy(regd->alpha2, request->alpha2, sizeof(regd->alpha2));
    regd->n_reg_rules = 1;

    /* Set up default rule for 2.4 GHz */
    regd->reg_rules[0].freq_range.start_freq_khz = MHZ_TO_KHZ(2412);
    regd->reg_rules[0].freq_range.end_freq_khz = MHZ_TO_KHZ(2484);
    regd->reg_rules[0].freq_range.max_bandwidth_khz = MHZ_TO_KHZ(40);
    regd->reg_rules[0].power_rule.max_eirp = DBM_TO_MBM(20);
    regd->reg_rules[0].flags = 0;

    /* Apply the new regulatory domain */
    wiphy_apply_custom_regulatory(wiphy, regd);

    /* Clean up */
    kfree(regd);
}

int wifi67_reg_set_power(struct wifi67_priv *priv,
                        struct cfg80211_chan_def *chandef,
                        u32 power)
{
    struct wifi67_regulatory *reg = &priv->reg;
    int i;

    /* Find matching rule and update power */
    for (i = 0; i < reg->n_rules; i++) {
        if (chandef->center_freq1 >= reg->rules[i].start_freq &&
            chandef->center_freq1 <= reg->rules[i].end_freq) {
            reg->rules[i].max_power = power;
            return 0;
        }
    }

    return -EINVAL;
}

