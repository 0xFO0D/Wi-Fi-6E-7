#ifndef __WIFI7_REG_H
#define __WIFI7_REG_H

#include <linux/types.h>
#include <linux/nl80211.h>
#include "../core/wifi7_core.h"

#define WIFI7_REG_MAX_RULES 64
#define WIFI7_REG_MAX_AFC_RULES 32
#define WIFI7_REG_AFC_TIMEOUT_MS 3600000 /* 1 hour */

enum wifi7_reg_domain {
    WIFI7_REG_UNSET = 0,
    WIFI7_REG_FCC = 1,
    WIFI7_REG_ETSI = 2,
    WIFI7_REG_MKK = 3,
    WIFI7_REG_CN = 4,
    WIFI7_REG_MAX
};

struct wifi7_reg_rule {
    u32 freq_start;
    u32 freq_end;
    u32 max_bw;
    u8 max_ant_gain;
    u8 max_power;
    u32 flags;
    bool afc_required;
};

struct wifi7_afc_rule {
    u32 freq_start;
    u32 freq_end;
    u8 max_power;
    u64 timestamp;
    u8 geo_area[32];
    bool valid;
};

struct wifi7_regulatory {
    enum wifi7_reg_domain domain;
    struct wifi7_reg_rule rules[WIFI7_REG_MAX_RULES];
    struct wifi7_afc_rule afc_rules[WIFI7_REG_MAX_AFC_RULES];
    u32 n_rules;
    u32 n_afc_rules;
    spinlock_t lock;
    struct mutex afc_mutex;
    struct delayed_work afc_work;
    bool afc_enabled;
};

int wifi7_regulatory_init(struct wifi7_phy_dev *dev);
void wifi7_regulatory_deinit(struct wifi7_phy_dev *dev);

int wifi7_regulatory_set_region(struct wifi7_phy_dev *dev,
                              enum wifi7_reg_domain domain);
                              
int wifi7_regulatory_check_freq_range(struct wifi7_regulatory *reg,
                                    u32 freq_range[2]);
                                    
int wifi7_regulatory_get_max_power(struct wifi7_regulatory *reg,
                                 u32 freq_range[2],
                                 const u8 *geo_area,
                                 u8 *max_power);

int wifi7_regulatory_update_afc(struct wifi7_regulatory *reg,
                              const struct wifi7_afc_rule *rules,
                              u32 n_rules);

#endif /* __WIFI7_REG_H */ 