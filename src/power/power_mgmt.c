#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pm_runtime.h>
#include "../../include/core/wifi67.h"
#include "../../include/power/power_mgmt.h"

void wifi67_power_save_setup(struct wifi67_priv *priv)
{
    struct ieee80211_hw *hw = priv->hw;
    
    /* Enable hardware power management features */
    hw->flags |= IEEE80211_HW_SUPPORTS_PS |
                IEEE80211_HW_SUPPORTS_DYNAMIC_PS |
                IEEE80211_HW_SUPPORTS_WAKEUP;

    /* Configure default power save timeouts */
    hw->max_idle_period = 1000; /* ms */
    hw->dynamic_ps_timeout = 100; /* ms */
    
    /* Enable WoWLAN if supported */
    hw->wiphy->wowlan = &wifi67_wowlan_support;
}

int wifi67_power_save_enter(struct wifi67_priv *priv)
{
    int ret;

    /* Save current hardware state */
    ret = wifi67_save_context(priv);
    if (ret)
        return ret;

    /* Configure low power state */
    ret = wifi67_configure_power_state(priv, WIFI67_PS_DEEP);
    if (ret)
        goto restore_context;

    return 0;

restore_context:
    wifi67_restore_context(priv);
    return ret;
}

int wifi67_power_save_exit(struct wifi67_priv *priv)
{
    int ret;

    /* Return to full power state */
    ret = wifi67_configure_power_state(priv, WIFI67_PS_ACTIVE);
    if (ret)
        return ret;

    /* Restore hardware context */
    ret = wifi67_restore_context(priv);
    if (ret)
        return ret;

    return 0;
}

EXPORT_SYMBOL(wifi67_power_save_setup);
EXPORT_SYMBOL(wifi67_power_save_enter);
EXPORT_SYMBOL(wifi67_power_save_exit); 