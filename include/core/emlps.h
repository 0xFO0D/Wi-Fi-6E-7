#ifndef __WIFI67_EMLPS_H
#define __WIFI67_EMLPS_H

#define WIFI67_EMLPS_DEFAULT_TIMEOUT  100
#define WIFI67_EMLPS_DEFAULT_WINDOW   20

enum wifi67_emlps_state {
    WIFI67_EMLPS_DISABLED,
    WIFI67_EMLPS_ENABLED
};

struct wifi67_emlps_params {
    u32 timeout;
    u32 awake_window;
    bool force_active;
};

int wifi67_emlps_init(struct wifi67_priv *priv);
void wifi67_emlps_deinit(struct wifi67_priv *priv);
int wifi67_emlps_enable(struct wifi67_priv *priv);
void wifi67_emlps_disable(struct wifi67_priv *priv);
void wifi67_emlps_activity(struct wifi67_priv *priv, u8 link_id);
int wifi67_emlps_set_params(struct wifi67_priv *priv,
                          struct wifi67_emlps_params *params);

#endif /* __WIFI67_EMLPS_H */ 