#ifndef _WIFI67_FW_CORE_H_
#define _WIFI67_FW_CORE_H_

#include <linux/types.h>
#include <linux/firmware.h>
#include "../core/wifi67_forward.h"

#define WIFI67_FW_MAX_SIZE      (512 * 1024)  /* 512KB */
#define WIFI67_FW_CHUNK_SIZE    4096
#define WIFI67_FW_NAME          "wifi67/firmware.bin"
#define WIFI67_FW_API_VERSION   1
#define WIFI67_FW_MAGIC         0x57494649    /* "WIFI" */

struct wifi67_fw_header {
    u32 magic;          /* Magic number for verification */
    u32 version;        /* Firmware version */
    u32 api_version;    /* API version */
    u32 size;          /* Total firmware size */
    u32 checksum;      /* Firmware checksum */
    u32 entry_point;   /* Firmware entry point */
    u32 flags;         /* Firmware flags */
    u8 reserved[16];   /* Reserved for future use */
} __packed;

/* Function prototypes */
int wifi67_fw_load(struct wifi67_priv *priv);
void wifi67_fw_unload(struct wifi67_priv *priv);
int wifi67_fw_verify(struct wifi67_priv *priv);
int wifi67_fw_start(struct wifi67_priv *priv);
void wifi67_fw_stop(struct wifi67_priv *priv);

#endif /* _WIFI67_FW_CORE_H_ */ 