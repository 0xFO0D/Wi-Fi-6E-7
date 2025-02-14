#ifndef __WIFI67_EMLRC_H
#define __WIFI67_EMLRC_H

#define WIFI67_EMLRC_MAX_RATES 32
#define WIFI67_EMLRC_UPDATE_INTERVAL 100
#define WIFI67_EMLRC_PROBE_INTERVAL 10
#define WIFI67_EMLRC_SAMPLING_PERIOD 1000
#define WIFI67_EMLRC_PROBE_FLAG BIT(7)

enum wifi67_emlrc_state {
    WIFI67_EMLRC_DISABLED,
    WIFI67_EMLRC_ENABLED
};

int wifi67_emlrc_init(struct wifi67_priv *priv);
void wifi67_emlrc_deinit(struct wifi67_priv *priv);
int wifi67_emlrc_enable(struct wifi67_priv *priv);
void wifi67_emlrc_disable(struct wifi67_priv *priv);
void wifi67_emlrc_tx_status(struct wifi67_priv *priv, u8 link_id,
                          struct sk_buff *skb, bool success,
                          u8 retries);

#endif /* __WIFI67_EMLRC_H */ 