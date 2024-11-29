#ifndef _WIFI67_FIRMWARE_H_
#define _WIFI67_FIRMWARE_H_

#include <linux/types.h>
#include <linux/firmware.h>

struct wifi67_firmware {
    const struct firmware *fw;
    u32 version;
    u8 *data;
    size_t size;
    bool loaded;
    
    struct {
        u32 load_count;
        u32 load_errors;
        u32 verify_errors;
        u32 exec_errors;
    } stats;
};

#endif /* _WIFI67_FIRMWARE_H_ */ 