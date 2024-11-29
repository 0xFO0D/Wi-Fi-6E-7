#ifndef _WIFI67_FW_CORE_H_
#define _WIFI67_FW_CORE_H_

#include <linux/types.h>
#include <linux/firmware.h>
#include "../core/wifi67_forward.h"

#define WIFI67_FW_NAME "wifi67/firmware.bin"
#define WIFI67_FW_MAGIC 0x57494649 /* "WIFI" */

struct wifi67_firmware {
    const struct firmware *fw_data;
    void __iomem *fw_mem;
    bool loaded;
    bool running;
};

/* Function declarations */
int wifi67_fw_load(struct wifi67_priv *priv);
void wifi67_fw_unload(struct wifi67_priv *priv);
int wifi67_fw_verify(struct wifi67_priv *priv);
int wifi67_fw_start(struct wifi67_priv *priv);
int wifi67_fw_stop(struct wifi67_priv *priv);

#endif /* _WIFI67_FW_CORE_H_ */ 