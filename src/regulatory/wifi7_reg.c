#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/time64.h>
#include "wifi7_reg.h"

/* FCC rules for 6 GHz band */
static const struct wifi7_reg_rule fcc_rules[] = {
    /* U-NII-5 */
    {
        .freq_start = 5925,
        .freq_end = 6425,
        .max_bw = 160,
        .max_ant_gain = 6,
        .max_power = 24,
        .flags = 0,
        .afc_required = false
    },
    /* U-NII-6 */
    {
        .freq_start = 6425,
        .freq_end = 6525,
        .max_bw = 320,
        .max_ant_gain = 6,
        .max_power = 24,
        .flags = 0,
        .afc_required = false
    },
    /* U-NII-7 */
    {
        .freq_start = 6525,
        .freq_end = 6875,
        .max_bw = 320,
        .max_ant_gain = 6,
        .max_power = 30,
        .flags = 0,
        .afc_required = true
    },
    /* U-NII-8 */
    {
        .freq_start = 6875,
        .freq_end = 7125,
        .max_bw = 320,
        .max_ant_gain = 6,
        .max_power = 30,
        .flags = 0,
        .afc_required = true
    }
};

/* ETSI rules for 6 GHz band */
static const struct wifi7_reg_rule etsi_rules[] = {
    /* Lower 6 GHz */
    {
        .freq_start = 5925,
        .freq_end = 6425,
        .max_bw = 320,
        .max_ant_gain = 6,
        .max_power = 23,
        .flags = 0,
        .afc_required = false
    },
    /* Upper 6 GHz */
    {
        .freq_start = 6425,
        .freq_end = 7125,
        .max_bw = 320,
        .max_ant_gain = 6,
        .max_power = 23,
        .flags = 0,
        .afc_required = true
    }
};

static void afc_timeout_work(struct work_struct *work)
{
    struct delayed_work *dwork = to_delayed_work(work);
    struct wifi7_regulatory *reg = container_of(dwork,
                                             struct wifi7_regulatory,
                                             afc_work);
    unsigned long flags;
    int i;

    mutex_lock(&reg->afc_mutex);
    spin_lock_irqsave(&reg->lock, flags);

    for (i = 0; i < reg->n_afc_rules; i++) {
        if (reg->afc_rules[i].valid &&
            time_after64(get_jiffies_64(),
                        reg->afc_rules[i].timestamp +
                        msecs_to_jiffies(WIFI7_REG_AFC_TIMEOUT_MS))) {
            reg->afc_rules[i].valid = false;
        }
    }

    spin_unlock_irqrestore(&reg->lock, flags);
    mutex_unlock(&reg->afc_mutex);

    schedule_delayed_work(&reg->afc_work,
                         msecs_to_jiffies(WIFI7_REG_AFC_TIMEOUT_MS));
}

int wifi7_regulatory_init(struct wifi7_phy_dev *dev)
{
    struct wifi7_regulatory *reg;

    reg = kzalloc(sizeof(*reg), GFP_KERNEL);
    if (!reg)
        return -ENOMEM;

    spin_lock_init(&reg->lock);
    mutex_init(&reg->afc_mutex);
    INIT_DELAYED_WORK(&reg->afc_work, afc_timeout_work);
    reg->domain = WIFI7_REG_UNSET;
    reg->afc_enabled = true;

    dev->regulatory = reg;

    schedule_delayed_work(&reg->afc_work,
                         msecs_to_jiffies(WIFI7_REG_AFC_TIMEOUT_MS));

    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_regulatory_init);

void wifi7_regulatory_deinit(struct wifi7_phy_dev *dev)
{
    struct wifi7_regulatory *reg = dev->regulatory;

    if (!reg)
        return;

    cancel_delayed_work_sync(&reg->afc_work);
    mutex_destroy(&reg->afc_mutex);
    kfree(reg);
    dev->regulatory = NULL;
}
EXPORT_SYMBOL_GPL(wifi7_regulatory_deinit);

int wifi7_regulatory_set_region(struct wifi7_phy_dev *dev,
                              enum wifi7_reg_domain domain)
{
    struct wifi7_regulatory *reg = dev->regulatory;
    const struct wifi7_reg_rule *rules;
    unsigned long flags;
    int n_rules;

    if (!reg || domain >= WIFI7_REG_MAX)
        return -EINVAL;

    switch (domain) {
    case WIFI7_REG_FCC:
        rules = fcc_rules;
        n_rules = ARRAY_SIZE(fcc_rules);
        break;
    case WIFI7_REG_ETSI:
        rules = etsi_rules;
        n_rules = ARRAY_SIZE(etsi_rules);
        break;
    default:
        return -EINVAL;
    }

    spin_lock_irqsave(&reg->lock, flags);
    memcpy(reg->rules, rules, sizeof(*rules) * n_rules);
    reg->n_rules = n_rules;
    reg->domain = domain;
    spin_unlock_irqrestore(&reg->lock, flags);

    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_regulatory_set_region);

int wifi7_regulatory_check_freq_range(struct wifi7_regulatory *reg,
                                    u32 freq_range[2])
{
    unsigned long flags;
    int i;

    if (!reg || !freq_range)
        return -EINVAL;

    spin_lock_irqsave(&reg->lock, flags);

    for (i = 0; i < reg->n_rules; i++) {
        if (freq_range[0] >= reg->rules[i].freq_start &&
            freq_range[1] <= reg->rules[i].freq_end) {
            if (reg->rules[i].afc_required && !reg->afc_enabled) {
                spin_unlock_irqrestore(&reg->lock, flags);
                return -EPERM;
            }
            spin_unlock_irqrestore(&reg->lock, flags);
            return 0;
        }
    }

    spin_unlock_irqrestore(&reg->lock, flags);
    return -ERANGE;
}
EXPORT_SYMBOL_GPL(wifi7_regulatory_check_freq_range);

int wifi7_regulatory_get_max_power(struct wifi7_regulatory *reg,
                                 u32 freq_range[2],
                                 const u8 *geo_area,
                                 u8 *max_power)
{
    unsigned long flags;
    u8 power = 0;
    int i;

    if (!reg || !freq_range || !max_power)
        return -EINVAL;

    spin_lock_irqsave(&reg->lock, flags);

    /* Check AFC rules first */
    if (reg->afc_enabled) {
        for (i = 0; i < reg->n_afc_rules; i++) {
            if (reg->afc_rules[i].valid &&
                freq_range[0] >= reg->afc_rules[i].freq_start &&
                freq_range[1] <= reg->afc_rules[i].freq_end &&
                (!geo_area || !memcmp(geo_area,
                                    reg->afc_rules[i].geo_area,
                                    sizeof(reg->afc_rules[i].geo_area)))) {
                power = reg->afc_rules[i].max_power;
                break;
            }
        }
    }

    /* Check static rules */
    for (i = 0; i < reg->n_rules; i++) {
        if (freq_range[0] >= reg->rules[i].freq_start &&
            freq_range[1] <= reg->rules[i].freq_end) {
            if (power == 0 || reg->rules[i].max_power < power)
                power = reg->rules[i].max_power;
            break;
        }
    }

    spin_unlock_irqrestore(&reg->lock, flags);

    if (power == 0)
        return -ERANGE;

    *max_power = power;
    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_regulatory_get_max_power);

int wifi7_regulatory_update_afc(struct wifi7_regulatory *reg,
                              const struct wifi7_afc_rule *rules,
                              u32 n_rules)
{
    unsigned long flags;
    int i;

    if (!reg || !rules || n_rules > WIFI7_REG_MAX_AFC_RULES)
        return -EINVAL;

    mutex_lock(&reg->afc_mutex);
    spin_lock_irqsave(&reg->lock, flags);

    /* Clear old AFC rules */
    for (i = 0; i < reg->n_afc_rules; i++)
        reg->afc_rules[i].valid = false;

    /* Add new AFC rules */
    for (i = 0; i < n_rules; i++) {
        memcpy(&reg->afc_rules[i], &rules[i],
               sizeof(reg->afc_rules[i]));
        reg->afc_rules[i].timestamp = get_jiffies_64();
        reg->afc_rules[i].valid = true;
    }
    reg->n_afc_rules = n_rules;

    spin_unlock_irqrestore(&reg->lock, flags);
    mutex_unlock(&reg->afc_mutex);

    return 0;
}
EXPORT_SYMBOL_GPL(wifi7_regulatory_update_afc);

/* Module initialization */
static int __init wifi7_regulatory_init_module(void)
{
    pr_info("WiFi 7 Regulatory Domain initialized\n");
    return 0;
}

static void __exit wifi7_regulatory_exit_module(void)
{
    pr_info("WiFi 7 Regulatory Domain unloaded\n");
}

module_init(wifi7_regulatory_init_module);
module_exit(wifi7_regulatory_exit_module);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Regulatory Domain Support");
MODULE_VERSION("1.0"); 