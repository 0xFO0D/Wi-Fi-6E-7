#ifndef __WIFI67_MLO_H
#define __WIFI67_MLO_H

enum wifi67_mlo_link_state {
    WIFI67_MLO_LINK_IDLE,
    WIFI67_MLO_LINK_SETUP,
    WIFI67_MLO_LINK_ACTIVE,
    WIFI67_MLO_LINK_ERROR
};

#define WIFI67_MLO_LINK_FLAG_ACTIVE  BIT(0)
#define WIFI67_MLO_LINK_FLAG_ERROR   BIT(1)

#define WIFI67_MLO_MAX_TIDS 8
#define WIFI67_MLO_TID_ALL_LINKS 0xFF

struct wifi67_mlo_tid_map {
    u8 primary_link;
    u8 secondary_links;
    u32 flags;
};

struct wifi67_mlo_link {
    struct wifi67_priv *priv;
    struct list_head list;
    u8 link_id;
    u8 band;
    enum wifi67_mlo_link_state state;
    u32 flags;
    struct wifi67_mlo_tid_map tid_maps[WIFI67_MLO_MAX_TIDS];
};

struct wifi67_mlo_link *wifi67_mlo_alloc_link(struct wifi67_priv *priv);
int wifi67_mlo_setup_link(struct wifi67_priv *priv, struct wifi67_mlo_link *link,
                         u8 link_id, u8 band);
void wifi67_mlo_remove_link(struct wifi67_mlo_link *link);
int wifi67_mlo_init(struct wifi67_priv *priv);
void wifi67_mlo_deinit(struct wifi67_priv *priv);

int wifi67_mlo_activate_link(struct wifi67_mlo_link *link);
int wifi67_mlo_deactivate_link(struct wifi67_mlo_link *link);
int wifi67_mlo_handle_link_error(struct wifi67_mlo_link *link);
struct wifi67_mlo_link *wifi67_mlo_get_link_by_id(struct wifi67_priv *priv,
                                                 u8 link_id);

int wifi67_mlo_map_tid(struct wifi67_mlo_link *link, u8 tid,
                      u8 primary_link, u8 secondary_links);
int wifi67_mlo_unmap_tid(struct wifi67_mlo_link *link, u8 tid);
u8 wifi67_mlo_get_link_for_tid(struct wifi67_mlo_link *link, u8 tid);

#endif /* __WIFI67_MLO_H */ 