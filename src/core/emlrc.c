#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <linux/math64.h>
#include "../../include/core/wifi67.h"
#include "../../include/core/emlrc.h"

struct wifi67_emlrc_stats {
    u32 attempts;
    u32 successes;
    u32 retries;
    u32 failures;
    u32 throughput;
    u32 ewma_prob;
};

struct wifi67_emlrc_rate {
    u16 rate_code;
    u8 nss;
    u8 bw;
    u8 gi;
    u8 mcs;
    bool eht;
    struct wifi67_emlrc_stats stats;
};

struct wifi67_emlrc {
    spinlock_t lock;
    u8 state;
    struct {
        struct wifi67_emlrc_rate rates[WIFI67_EMLRC_MAX_RATES];
        u8 current_idx;
        u8 probe_idx;
        u32 update_interval;
        u32 probe_interval;
        struct delayed_work update_work;
    } rate_ctrl[WIFI67_MAX_LINKS];
    u32 sampling_period;
    bool probing;
};

static struct wifi67_emlrc *emlrc_alloc(void)
{
    struct wifi67_emlrc *emlrc;
    int i, j;

    emlrc = kzalloc(sizeof(*emlrc), GFP_KERNEL);
    if (!emlrc)
        return NULL;

    spin_lock_init(&emlrc->lock);

    for (i = 0; i < WIFI67_MAX_LINKS; i++) {
        INIT_DELAYED_WORK(&emlrc->rate_ctrl[i].update_work,
                         wifi67_emlrc_update_handler);
        emlrc->rate_ctrl[i].current_idx = 0;
        emlrc->rate_ctrl[i].probe_idx = 0;
        emlrc->rate_ctrl[i].update_interval = WIFI67_EMLRC_UPDATE_INTERVAL;
        emlrc->rate_ctrl[i].probe_interval = WIFI67_EMLRC_PROBE_INTERVAL;

        for (j = 0; j < WIFI67_EMLRC_MAX_RATES; j++) {
            struct wifi67_emlrc_rate *rate = &emlrc->rate_ctrl[i].rates[j];
            rate->ewma_prob = 0;
            rate->attempts = 0;
            rate->successes = 0;
            rate->retries = 0;
            rate->failures = 0;
            rate->throughput = 0;
        }
    }

    emlrc->sampling_period = WIFI67_EMLRC_SAMPLING_PERIOD;
    emlrc->probing = false;

    return emlrc;
}

int wifi67_emlrc_init(struct wifi67_priv *priv)
{
    struct wifi67_emlrc *emlrc;

    emlrc = emlrc_alloc();
    if (!emlrc)
        return -ENOMEM;

    emlrc->state = WIFI67_EMLRC_DISABLED;
    priv->emlrc = emlrc;

    return 0;
}

void wifi67_emlrc_deinit(struct wifi67_priv *priv)
{
    struct wifi67_emlrc *emlrc = priv->emlrc;
    int i;

    if (!emlrc)
        return;

    for (i = 0; i < WIFI67_MAX_LINKS; i++)
        cancel_delayed_work_sync(&emlrc->rate_ctrl[i].update_work);

    kfree(emlrc);
    priv->emlrc = NULL;
}

static void wifi67_emlrc_update_stats(struct wifi67_emlrc_rate *rate,
                                    u32 attempts, u32 successes,
                                    u32 retries)
{
    u32 prob;

    rate->attempts += attempts;
    rate->successes += successes;
    rate->retries += retries;
    rate->failures += attempts - successes;

    if (rate->attempts > 0) {
        prob = (rate->successes * 100) / rate->attempts;
        rate->ewma_prob = (rate->ewma_prob * 7 + prob) / 8;
        rate->throughput = (rate->ewma_prob * rate->rate_code) / 100;
    }
}

static u8 wifi67_emlrc_find_best_rate(struct wifi67_emlrc *emlrc, u8 link_id)
{
    struct wifi67_emlrc_rate *rates = emlrc->rate_ctrl[link_id].rates;
    u8 best_idx = 0;
    u32 best_tput = 0;
    int i;

    for (i = 0; i < WIFI67_EMLRC_MAX_RATES; i++) {
        if (rates[i].throughput > best_tput) {
            best_tput = rates[i].throughput;
            best_idx = i;
        }
    }

    return best_idx;
}

static void wifi67_emlrc_update_handler(struct work_struct *work)
{
    struct wifi67_rate_ctrl *rc = container_of(to_delayed_work(work),
                                             struct wifi67_rate_ctrl,
                                             update_work);
    struct wifi67_emlrc *emlrc = container_of(rc, struct wifi67_emlrc,
                                            rate_ctrl[rc->link_id]);
    unsigned long flags;
    u8 new_idx;

    spin_lock_irqsave(&emlrc->lock, flags);

    if (emlrc->state != WIFI67_EMLRC_ENABLED)
        goto out;

    if (emlrc->probing) {
        wifi67_emlrc_update_stats(&rc->rates[rc->probe_idx],
                                rc->probe_attempts,
                                rc->probe_successes,
                                rc->probe_retries);
        emlrc->probing = false;
    }

    new_idx = wifi67_emlrc_find_best_rate(emlrc, rc->link_id);
    if (new_idx != rc->current_idx) {
        rc->current_idx = new_idx;
        wifi67_hw_set_rate(priv, rc->link_id,
                          &rc->rates[new_idx]);
    }

    if (rc->probe_interval-- == 0) {
        rc->probe_idx = (rc->current_idx + 1) % WIFI67_EMLRC_MAX_RATES;
        emlrc->probing = true;
        rc->probe_interval = WIFI67_EMLRC_PROBE_INTERVAL;
    }

out:
    spin_unlock_irqrestore(&emlrc->lock, flags);
    schedule_delayed_work(&rc->update_work,
                         msecs_to_jiffies(rc->update_interval));
}

int wifi67_emlrc_enable(struct wifi67_priv *priv)
{
    struct wifi67_emlrc *emlrc = priv->emlrc;
    unsigned long flags;
    int ret = 0, i;

    if (!emlrc)
        return -EINVAL;

    spin_lock_irqsave(&emlrc->lock, flags);

    if (emlrc->state != WIFI67_EMLRC_DISABLED) {
        ret = -EBUSY;
        goto out;
    }

    emlrc->state = WIFI67_EMLRC_ENABLED;
    for (i = 0; i < WIFI67_MAX_LINKS; i++) {
        schedule_delayed_work(&emlrc->rate_ctrl[i].update_work,
                            msecs_to_jiffies(emlrc->rate_ctrl[i].update_interval));
    }

out:
    spin_unlock_irqrestore(&emlrc->lock, flags);
    return ret;
}

void wifi67_emlrc_disable(struct wifi67_priv *priv)
{
    struct wifi67_emlrc *emlrc = priv->emlrc;
    unsigned long flags;
    int i;

    if (!emlrc)
        return;

    spin_lock_irqsave(&emlrc->lock, flags);

    if (emlrc->state == WIFI67_EMLRC_ENABLED) {
        emlrc->state = WIFI67_EMLRC_DISABLED;
        for (i = 0; i < WIFI67_MAX_LINKS; i++)
            cancel_delayed_work(&emlrc->rate_ctrl[i].update_work);
    }

    spin_unlock_irqrestore(&emlrc->lock, flags);
}

void wifi67_emlrc_tx_status(struct wifi67_priv *priv, u8 link_id,
                          struct sk_buff *skb, bool success,
                          u8 retries)
{
    struct wifi67_emlrc *emlrc = priv->emlrc;
    struct wifi67_rate_ctrl *rc;
    unsigned long flags;

    if (!emlrc || emlrc->state != WIFI67_EMLRC_ENABLED ||
        link_id >= WIFI67_MAX_LINKS)
        return;

    spin_lock_irqsave(&emlrc->lock, flags);
    rc = &emlrc->rate_ctrl[link_id];

    if (emlrc->probing && skb->priority & WIFI67_EMLRC_PROBE_FLAG) {
        rc->probe_attempts++;
        if (success)
            rc->probe_successes++;
        rc->probe_retries += retries;
    } else {
        struct wifi67_emlrc_rate *rate = &rc->rates[rc->current_idx];
        wifi67_emlrc_update_stats(rate, 1, success ? 1 : 0, retries);
    }

    spin_unlock_irqrestore(&emlrc->lock, flags);
}

EXPORT_SYMBOL(wifi67_emlrc_init);
EXPORT_SYMBOL(wifi67_emlrc_deinit);
EXPORT_SYMBOL(wifi67_emlrc_enable);
EXPORT_SYMBOL(wifi67_emlrc_disable);
EXPORT_SYMBOL(wifi67_emlrc_tx_status); 