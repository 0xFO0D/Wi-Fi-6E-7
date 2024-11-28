#include <linux/delay.h>
#include <linux/bitfield.h>
#include "../../include/mac/mac_regs.h"
#include "../../include/mac/mac_core.h"

static inline u32 mac_read32(struct wifi67_priv *priv, u32 reg)
{
    return readl(priv->mmio + reg);
}

static inline void mac_write32(struct wifi67_priv *priv, u32 reg, u32 val)
{
    writel(val, priv->mmio + reg);
}

static int wifi67_mac_configure_ampdu(struct wifi67_priv *priv)
{
    u32 val;

    val = FIELD_PREP(AMPDU_MAX_LEN_MASK, MAX_AMPDU_LEN) |
          FIELD_PREP(AMPDU_MIN_SPACING, 0x2);
    mac_write32(priv, MAC_AMPDU_CTRL, val);

    return 0;
}

static int wifi67_mac_configure_ba(struct wifi67_priv *priv)
{
    u32 val;

    val = BA_POLICY_IMMEDIATE |
          FIELD_PREP(BA_BUFFER_SIZE_MASK, 64);
    mac_write32(priv, MAC_BA_CTRL, val);

    return 0;
}

static void wifi67_mac_update_tsf(struct wifi67_priv *priv)
{
    u64 tsf;
    
    tsf = ktime_to_ns(ktime_get_real());
    mac_write32(priv, MAC_TSF_TIMER_LOW, lower_32_bits(tsf));
    mac_write32(priv, MAC_TSF_TIMER_HIGH, upper_32_bits(tsf));
}

int wifi67_mac_init(struct wifi67_priv *priv)
{
    u32 val;
    int ret;

    /* Configure basic MAC features */
    val = MAC_CFG_AMPDU_EN | MAC_CFG_AMSDU_EN | MAC_CFG_MLO_EN;
    mac_write32(priv, MAC_CTRL_CONFIG, val);

    /* Initialize AMPDU engine */
    ret = wifi67_mac_configure_ampdu(priv);
    if (ret)
        goto err;

    /* Initialize Block ACK */
    ret = wifi67_mac_configure_ba(priv);
    if (ret)
        goto err;

    /* Initialize TSF timer */
    wifi67_mac_update_tsf(priv);

    return 0;

err:
    mac_write32(priv, MAC_CTRL_CONFIG, 0);
    return ret;
}

void wifi67_mac_deinit(struct wifi67_priv *priv)
{
    mac_write32(priv, MAC_CTRL_CONFIG, 0);
}

int wifi67_mac_start(struct wifi67_priv *priv)
{
    u32 val;

    val = mac_read32(priv, MAC_CTRL_CONFIG);
    val |= MAC_CFG_ENABLE | MAC_CFG_RX_EN | MAC_CFG_TX_EN;
    mac_write32(priv, MAC_CTRL_CONFIG, val);

    return 0;
}

void wifi67_mac_stop(struct wifi67_priv *priv)
{
    u32 val;

    val = mac_read32(priv, MAC_CTRL_CONFIG);
    val &= ~(MAC_CFG_ENABLE | MAC_CFG_RX_EN | MAC_CFG_TX_EN);
    mac_write32(priv, MAC_CTRL_CONFIG, val);
}

int wifi67_mac_add_ba_session(struct wifi67_priv *priv, u8 tid, u16 ssn)
{
    struct mac_ba_session *ba;
    u32 val;

    if (tid >= MAX_TID_COUNT)
        return -EINVAL;

    ba = kzalloc(sizeof(*ba), GFP_KERNEL);
    if (!ba)
        return -ENOMEM;

    ba->tid = tid;
    ba->ssn = ssn;
    ba->buf_size = 64;
    ba->active = true;
    spin_lock_init(&ba->lock);

    ba->reorder_buf = kcalloc(ba->buf_size, sizeof(struct sk_buff *), 
                             GFP_KERNEL);
    if (!ba->reorder_buf) {
        kfree(ba);
        return -ENOMEM;
    }

    ba->bitmap = bitmap_zalloc(ba->buf_size, GFP_KERNEL);
    if (!ba->bitmap) {
        kfree(ba->reorder_buf);
        kfree(ba);
        return -ENOMEM;
    }

    val = mac_read32(priv, MAC_BA_CTRL);
    val |= FIELD_PREP(BA_TID_MASK, tid);
    mac_write32(priv, MAC_BA_CTRL, val);

    return 0;
}

void wifi67_mac_del_ba_session(struct wifi67_priv *priv, u8 tid)
{
    u32 val;

    val = mac_read32(priv, MAC_BA_CTRL);
    val &= ~FIELD_PREP(BA_TID_MASK, tid);
    mac_write32(priv, MAC_BA_CTRL, val);
}

int wifi67_mac_set_power_save(struct wifi67_priv *priv, bool enable)
{
    u32 val;

    val = mac_read32(priv, MAC_CTRL_CONFIG);
    if (enable)
        val |= MAC_CFG_PS_EN;
    else
        val &= ~MAC_CFG_PS_EN;
    mac_write32(priv, MAC_CTRL_CONFIG, val);

    return 0;
}

int wifi67_mac_add_mlo_link(struct wifi67_priv *priv, u8 link_id, u32 freq)
{
    struct mac_mlo_link *link;
    u32 val;

    if (link_id >= MAX_MLO_LINKS)
        return -EINVAL;

    link = kzalloc(sizeof(*link), GFP_KERNEL);
    if (!link)
        return -ENOMEM;

    link->link_id = link_id;
    link->freq = freq;
    link->state = 1; /* Active */

    val = mac_read32(priv, MAC_MLO_CTRL);
    val |= BIT(link_id);
    mac_write32(priv, MAC_MLO_CTRL, val);

    return 0;
} 