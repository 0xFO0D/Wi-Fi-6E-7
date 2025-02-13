#ifndef __WIFI67_EMLSR_H
#define __WIFI67_EMLSR_H

enum wifi67_emlsr_state {
    WIFI67_EMLSR_DISABLED,
    WIFI67_EMLSR_ENABLED
};

struct wifi67_emlsr_params {
    u32 transition_delay;
    bool pad_enabled;
};

int wifi67_emlsr_init(struct wifi67_priv *priv);
void wifi67_emlsr_deinit(struct wifi67_priv *priv);
int wifi67_emlsr_enable(struct wifi67_priv *priv);
void wifi67_emlsr_disable(struct wifi67_priv *priv);
int wifi67_emlsr_set_params(struct wifi67_priv *priv,
                          struct wifi67_emlsr_params *params);

#endif /* __WIFI67_EMLSR_H */ 