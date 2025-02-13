#ifndef __WIFI67_MLO_H
#define __WIFI67_MLO_H

enum wifi67_mlo_link_state {
    WIFI67_MLO_LINK_IDLE,
    WIFI67_MLO_LINK_SETUP,
    WIFI67_MLO_LINK_ACTIVE,
    WIFI67_MLO_LINK_ERROR
};

struct wifi67_mlo_link {
    struct wifi67_priv *priv;
    struct list_head list;
    u8 link_id;
    u8 band;
    enum wifi67_mlo_link_state state;
    u32 flags;
};

struct wifi67_mlo_link *wifi67_mlo_alloc_link(struct wifi67_priv *priv);
int wifi67_mlo_setup_link(struct wifi67_priv *priv, struct wifi67_mlo_link *link,
                         u8 link_id, u8 band);
void wifi67_mlo_remove_link(struct wifi67_mlo_link *link);
int wifi67_mlo_init(struct wifi67_priv *priv);
void wifi67_mlo_deinit(struct wifi67_priv *priv);

#endif /* __WIFI67_MLO_H */ 