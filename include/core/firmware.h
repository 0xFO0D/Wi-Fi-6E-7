#ifndef _WIFI67_FIRMWARE_H_
#define _WIFI67_FIRMWARE_H_

#include <linux/types.h>
#include <linux/firmware.h>

struct wifi67_fw_info {
    const struct firmware *fw;
    u32 version;
    bool loaded;
    void *data;
    size_t size;
};

#endif /* _WIFI67_FIRMWARE_H_ */ 