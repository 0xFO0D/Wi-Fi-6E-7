#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ieee80211.h>
#include "../include/core/wifi67.h"
#include "../include/core/bands.h"

static int test_band_registration(struct wifi67_priv *priv)
{
    struct ieee80211_supported_band *sband;
    int ret = 0;

    /* Test 5GHz band */
    sband = priv->hw->wiphy->bands[NL80211_BAND_5GHZ];
    if (!sband) {
        pr_err("5GHz band not registered\n");
        ret = -EINVAL;
        goto out;
    }

    if (sband->n_channels != wifi67_band_5ghz.n_channels) {
        pr_err("Invalid number of 5GHz channels\n");
        ret = -EINVAL;
        goto out;
    }

    /* Test 6GHz band */
    sband = priv->hw->wiphy->bands[NL80211_BAND_6GHZ];
    if (!sband) {
        pr_err("6GHz band not registered\n");
        ret = -EINVAL;
        goto out;
    }

    if (sband->n_channels != wifi67_band_6ghz.n_channels) {
        pr_err("Invalid number of 6GHz channels\n");
        ret = -EINVAL;
        goto out;
    }

out:
    return ret;
}

static int test_regulatory_compliance(struct wifi67_priv *priv)
{
    struct wiphy *wiphy = priv->hw->wiphy;
    int ret = 0;

    /* Verify regulatory flags */
    if (!(wiphy->regulatory_flags & REGULATORY_STRICT_REG)) {
        pr_err("Strict regulatory compliance not enabled\n");
        ret = -EINVAL;
        goto out;
    }

    /* Test power limits */
    ret = wifi67_verify_tx_power(priv);
    if (ret) {
        pr_err("TX power verification failed\n");
        goto out;
    }

out:
    return ret;
}

int wifi67_run_tests(struct wifi67_priv *priv)
{
    int ret;

    pr_info("Starting WiFi 6E/7 driver tests\n");

    ret = test_band_registration(priv);
    if (ret) {
        pr_err("Band registration test failed\n");
        goto out;
    }

    ret = test_regulatory_compliance(priv);
    if (ret) {
        pr_err("Regulatory compliance test failed\n");
        goto out;
    }

    pr_info("All tests passed successfully\n");

out:
    return ret;
}
EXPORT_SYMBOL(wifi67_run_tests);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("WiFi 6E/7 Driver Tests"); 