#include <linux/module.h>
#include <linux/slab.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <net/regulatory.h>
#include "../../include/regulatory/reg_core.h"
#include "../../include/regulatory/reg_radar.h"

/* Hardware register definitions */
#define REG_CTRL            0x0000
#define REG_POWER           0x0004
#define REG_CHANNEL         0x0008
#define REG_DFS_CTRL        0x000C
#define REG_DFS_STATUS      0x0010
#define REG_RADAR_CTRL      0x0014
#define REG_RADAR_DATA      0x0018

/* Control register bits */
#define REG_CTRL_ENABLE     BIT(0)
#define REG_CTRL_DFS_EN     BIT(1)
#define REG_CTRL_CAC_EN     BIT(2)
#define REG_CTRL_RADAR_EN   BIT(3)
#define REG_CTRL_PWR_CONST  BIT(4)

/* DFS Status bits */
#define DFS_STATUS_CAC      BIT(0)
#define DFS_STATUS_RADAR    BIT(1)
#define DFS_STATUS_BUSY     BIT(2)

static DEFINE_MUTEX(reg_mutex);

static struct wifi67_reg_rule *wifi67_find_rule(struct wifi67_regulatory *reg,
                                               u32 freq)
{
    struct rb_node *node = reg->rule_tree.rb_node;
    struct wifi67_reg_rule *rule;

    while (node) {
        rule = rb_entry(node, struct wifi67_reg_rule, node);

        if (freq < rule->start_freq)
            node = node->rb_left;
        else if (freq > rule->end_freq)
            node = node->rb_right;
        else
            return rule;
    }

    return NULL;
}

static int wifi67_insert_rule(struct wifi67_regulatory *reg,
                            struct wifi67_reg_rule *new_rule)
{
    struct rb_node **node = &reg->rule_tree.rb_node;
    struct rb_node *parent = NULL;
    struct wifi67_reg_rule *rule;

    while (*node) {
        rule = rb_entry(*node, struct wifi67_reg_rule, node);
        parent = *node;

        if (new_rule->start_freq < rule->start_freq)
            node = &(*node)->rb_left;
        else if (new_rule->start_freq > rule->start_freq)
            node = &(*node)->rb_right;
        else
            return -EEXIST;
    }

    rb_link_node(&new_rule->node, parent, node);
    rb_insert_color(&new_rule->node, &reg->rule_tree);

    return 0;
}

static void wifi67_reg_apply_radar_detector(struct wifi67_priv *priv,
                                          u32 freq)
{
    struct wifi67_regulatory *reg = priv->reg;
    u32 val;

    /* Configure radar detection parameters based on frequency */
    if (freq >= 5250 && freq <= 5350) {
        /* U-NII-2A settings */
        val = 0x1234ABCD; /* Vendor-specific radar parameters */
    } else if (freq >= 5470 && freq <= 5725) {
        /* U-NII-2C settings */
        val = 0x5678DCBA; /* Vendor-specific radar parameters */
    } else {
        /* Disable radar detection for non-DFS channels */
        val = 0;
    }

    writel(val, reg->reg_base + REG_RADAR_CTRL);
}

static void wifi67_reg_dfs_worker(struct work_struct *work)
{
    struct wifi67_regulatory *reg = container_of(work, struct wifi67_regulatory,
                                               dfs_work.work);
    struct wifi67_priv *priv = container_of(reg, struct wifi67_priv, reg);
    struct wifi67_dfs_state *dfs;
    unsigned long now = jiffies;
    u32 status, i;

    mutex_lock(&reg->reg_mutex);

    status = readl(reg->reg_base + REG_DFS_STATUS);

    for (i = 0; i < reg->num_dfs_states; i++) {
        dfs = &reg->dfs_states[i];
        spin_lock(&dfs->lock);

        if (dfs->state == 1) { /* CAC in progress */
            if (time_after(now, dfs->cac_end)) {
                /* CAC completed */
                dfs->state = 2;
                reg->cac_completed_count++;
                
                /* Update hardware */
                status |= DFS_STATUS_CAC;
                writel(status, reg->reg_base + REG_DFS_STATUS);
                
                /* Notify stack */
                cfg80211_cac_event(priv->hw->wiphy, priv->netdev,
                                 NL80211_RADAR_CAC_COMPLETED,
                                 GFP_KERNEL);
            }
        }

        spin_unlock(&dfs->lock);
    }

    mutex_unlock(&reg->reg_mutex);

    /* Reschedule worker */
    queue_delayed_work(reg->dfs_wq, &reg->dfs_work, HZ);
}

int wifi67_regulatory_init(struct wifi67_priv *priv)
{
    struct wifi67_regulatory *reg;
    int ret = 0, i;

    reg = kzalloc(sizeof(*reg), GFP_KERNEL);
    if (!reg)
        return -ENOMEM;

    priv->reg = reg;
    mutex_init(&reg->reg_mutex);
    reg->rule_tree = RB_ROOT;

    /* Initialize DFS states */
    for (i = 0; i < MAX_DFS_CHANNELS; i++) {
        spin_lock_init(&reg->dfs_states[i].lock);
    }

    /* Create DFS workqueue */
    reg->dfs_wq = create_singlethread_workqueue("wifi67-dfs");
    if (!reg->dfs_wq) {
        ret = -ENOMEM;
        goto err_free;
    }

    INIT_DELAYED_WORK(&reg->dfs_work, wifi67_reg_dfs_worker);

    /* Map registers */
    reg->reg_base = priv->mmio + 0x4000; /* Regulatory base offset */

    /* Enable regulatory subsystem */
    writel(REG_CTRL_ENABLE, reg->reg_base + REG_CTRL);

    /* Start DFS worker */
    queue_delayed_work(reg->dfs_wq, &reg->dfs_work, HZ);

    return 0;

err_free:
    kfree(reg);
    return ret;
}

void wifi67_regulatory_deinit(struct wifi67_priv *priv)
{
    struct wifi67_regulatory *reg = priv->reg;
    struct rb_node *node;
    struct wifi67_reg_rule *rule;

    if (!reg)
        return;

    /* Disable regulatory subsystem */
    writel(0, reg->reg_base + REG_CTRL);

    /* Cancel DFS work */
    cancel_delayed_work_sync(&reg->dfs_work);
    destroy_workqueue(reg->dfs_wq);

    /* Free all rules */
    while ((node = rb_first(&reg->rule_tree))) {
        rule = rb_entry(node, struct wifi67_reg_rule, node);
        rb_erase(node, &reg->rule_tree);
        kfree(rule);
    }

    kfree(reg);
    priv->reg = NULL;
}

int wifi67_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request)
{
    struct ieee80211_hw *hw = wiphy_to_ieee80211_hw(wiphy);
    struct wifi67_priv *priv = hw->priv;
    struct wifi67_regulatory *reg = priv->reg;
    u32 val;

    mutex_lock(&reg->reg_mutex);

    /* Store new country code */
    memcpy(reg->alpha2, request->alpha2, sizeof(reg->alpha2));

    /* Update DFS region */
    switch (request->dfs_region) {
    case NL80211_DFS_FCC:
        reg->dfs_region = RADAR_TYPE_FCC;
        break;
    case NL80211_DFS_ETSI:
        reg->dfs_region = RADAR_TYPE_ETSI;
        break;
    case NL80211_DFS_JP:
        reg->dfs_region = RADAR_TYPE_JP;
        break;
    default:
        reg->dfs_region = RADAR_TYPE_ETSI;
        break;
    }

    /* Update hardware */
    val = readl(reg->reg_base + REG_CTRL);
    val &= ~(0xFF << 8);
    val |= (reg->dfs_region << 8);
    writel(val, reg->reg_base + REG_CTRL);

    mutex_unlock(&reg->reg_mutex);

    return 0;
}

int wifi67_reg_set_power(struct wifi67_priv *priv, u32 freq, u32 power)
{
    struct wifi67_regulatory *reg = priv->reg;
    struct wifi67_reg_rule *rule;
    u32 val;

    mutex_lock(&reg->reg_mutex);

    rule = wifi67_find_rule(reg, freq);
    if (!rule) {
        mutex_unlock(&reg->reg_mutex);
        return -EINVAL;
    }

    if (power > rule->max_power)
        power = rule->max_power;

    reg->current_power = power;

    /* Update hardware */
    val = readl(reg->reg_base + REG_POWER);
    val &= ~0xFF;
    val |= power;
    writel(val, reg->reg_base + REG_POWER);

    mutex_unlock(&reg->reg_mutex);

    return 0;
}

int wifi67_reg_get_power(struct wifi67_priv *priv, u32 freq)
{
    struct wifi67_regulatory *reg = priv->reg;
    struct wifi67_reg_rule *rule;
    int power;

    mutex_lock(&reg->reg_mutex);

    rule = wifi67_find_rule(reg, freq);
    if (!rule) {
        mutex_unlock(&reg->reg_mutex);
        return -EINVAL;
    }

    power = reg->current_power;

    mutex_unlock(&reg->reg_mutex);

    return power;
}

bool wifi67_reg_is_channel_allowed(struct wifi67_priv *priv, u32 freq)
{
    struct wifi67_regulatory *reg = priv->reg;
    struct wifi67_reg_rule *rule;
    bool allowed = false;

    mutex_lock(&reg->reg_mutex);

    rule = wifi67_find_rule(reg, freq);
    if (rule)
        allowed = true;

    mutex_unlock(&reg->reg_mutex);

    return allowed;
}

int wifi67_reg_start_cac(struct wifi67_priv *priv, u32 freq)
{
    struct wifi67_regulatory *reg = priv->reg;
    struct wifi67_dfs_state *dfs;
    int i;

    mutex_lock(&reg->reg_mutex);

    for (i = 0; i < reg->num_dfs_states; i++) {
        dfs = &reg->dfs_states[i];
        spin_lock(&dfs->lock);

        if (dfs->state == 0) { /* Idle */
            dfs->channel = freq;
            dfs->state = 1; /* CAC in progress */
            dfs->cac_start = jiffies;
            dfs->cac_end = dfs->cac_start + msecs_to_jiffies(60000); /* 60s CAC */
            spin_unlock(&dfs->lock);
            mutex_unlock(&reg->reg_mutex);
            return 0;
        }

        spin_unlock(&dfs->lock);
    }

    mutex_unlock(&reg->reg_mutex);
    return -EBUSY;
}

void wifi67_reg_stop_cac(struct wifi67_priv *priv)
{
    struct wifi67_regulatory *reg = priv->reg;
    struct wifi67_dfs_state *dfs;
    int i;

    mutex_lock(&reg->reg_mutex);

    for (i = 0; i < reg->num_dfs_states; i++) {
        dfs = &reg->dfs_states[i];
        spin_lock(&dfs->lock);

        if (dfs->state == 1) { /* CAC in progress */
            dfs->state = 0; /* Reset to idle */
            reg->cac_failed_count++;
        }

        spin_unlock(&dfs->lock);
    }

    mutex_unlock(&reg->reg_mutex);
}

void wifi67_reg_radar_detected(struct wifi67_priv *priv, u32 freq)
{
    struct wifi67_regulatory *reg = priv->reg;
    struct wifi67_dfs_state *dfs;
    int i;

    mutex_lock(&reg->reg_mutex);

    for (i = 0; i < reg->num_dfs_states; i++) {
        dfs = &reg->dfs_states[i];
        spin_lock(&dfs->lock);

        if (dfs->channel == freq) {
            dfs->radar_detected = 1;
            reg->radar_detected_count++;
            /* Notify stack */
            cfg80211_radar_event(priv->hw->wiphy, priv->netdev,
                                 GFP_KERNEL);
        }

        spin_unlock(&dfs->lock);
    }

    mutex_unlock(&reg->reg_mutex);
}
