#ifndef _WIFI67_FW_CORE_H_
#define _WIFI67_FW_CORE_H_

#include <linux/types.h>
#include "../core/wifi67.h"

#define WIFI67_FW_NAME          "wifi67/firmware.bin"
#define WIFI67_FW_API_VER_MIN   1
#define WIFI67_FW_API_VER_MAX   1

struct wifi67_fw_header {
    u32 magic;
    u32 version;
    u32 api_version;
    u32 size;
    u32 entry_point;
    u32 data_offset;
    u32 data_size;
    u32 bss_offset;
    u32 bss_size;
    u32 checksum;
};

int wifi67_fw_load(struct wifi67_priv *priv);
void wifi67_fw_unload(struct wifi67_priv *priv);
int wifi67_fw_verify(struct wifi67_priv *priv);
int wifi67_fw_start(struct wifi67_priv *priv);
void wifi67_fw_stop(struct wifi67_priv *priv);

#endif /* _WIFI67_FW_CORE_H_ */ 