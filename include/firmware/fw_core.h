#ifndef _WIFI67_FW_CORE_H_
#define _WIFI67_FW_CORE_H_

#include <linux/types.h>
#include <linux/firmware.h>
#include <linux/completion.h>

#define FW_NAME "wifi67_fw.bin"
#define FW_MAX_SIZE (512 * 1024)

struct wifi67_firmware {
    const struct firmware *fw;
    struct completion fw_load;
    u8 *fw_data;
    u32 fw_size;
    u32 fw_version;
    u32 fw_crc;
};

/* Function prototypes */
int wifi67_fw_load(struct wifi67_priv *priv);
void wifi67_fw_unload(struct wifi67_priv *priv);
int wifi67_fw_verify(struct wifi67_priv *priv);
int wifi67_fw_start(struct wifi67_priv *priv);
void wifi67_fw_stop(struct wifi67_priv *priv);

#endif /* _WIFI67_FW_CORE_H_ */ 