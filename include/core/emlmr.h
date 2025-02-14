#ifndef __WIFI67_EMLMR_H
#define __WIFI67_EMLMR_H

#define WIFI67_MAX_RADIOS 4
#define WIFI67_INVALID_RADIO 0xFF
#define WIFI67_EMLMR_DEFAULT_DELAY 20
#define WIFI67_LINK_QUALITY_THRESHOLD 30

enum wifi67_emlmr_state {
    WIFI67_EMLMR_DISABLED,
    WIFI67_EMLMR_ENABLED
};

int wifi67_emlmr_init(struct wifi67_priv *priv);
void wifi67_emlmr_deinit(struct wifi67_priv *priv);
int wifi67_emlmr_enable(struct wifi67_priv *priv);
void wifi67_emlmr_disable(struct wifi67_priv *priv);
int wifi67_emlmr_add_link(struct wifi67_priv *priv, u8 link_id, u8 radio_id);
int wifi67_emlmr_remove_link(struct wifi67_priv *priv, u8 link_id);
int wifi67_emlmr_get_link_radio(struct wifi67_priv *priv, u8 link_id);

#endif /* __WIFI67_EMLMR_H */ 