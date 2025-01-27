#ifndef _FW_COMMON_H_
#define _FW_COMMON_H_

#include <linux/types.h>
#include <linux/firmware.h>

/* Common firmware error codes */
#define FW_ERR_NONE            0
#define FW_ERR_REQUEST         -1
#define FW_ERR_VERIFY         -2
#define FW_ERR_LOAD           -3
#define FW_ERR_VERSION        -4
#define FW_ERR_COMPRESS       -5
#define FW_ERR_SECURE         -6
#define FW_ERR_ROLLBACK       -7

/* Common firmware flags */
#define FW_FLAG_NONE           0
#define FW_FLAG_COMPRESSED     BIT(0)
#define FW_FLAG_SECURE         BIT(1)
#define FW_FLAG_VERIFY         BIT(2)
#define FW_FLAG_FORCE          BIT(3)

/* Firmware version structure */
struct fw_version {
    u8 major;
    u8 minor;
    u8 patch;
    u8 build;
    char extra[16];
};

/* Firmware information structure */
struct fw_info {
    const char *name;
    const char *description;
    struct fw_version version;
    u32 features;
    u32 size;
    u32 checksum;
    u32 flags;
};

/* Common firmware operations */
struct fw_ops {
    int (*request)(const char *name, const struct firmware **fw);
    void (*release)(const struct firmware *fw);
    int (*verify)(const struct firmware *fw, u32 checksum);
    int (*decompress)(const void *src, size_t src_len,
                     void *dst, size_t *dst_len);
    int (*check_version)(const struct fw_version *current,
                        const struct fw_version *new);
    int (*secure_boot)(const struct firmware *fw,
                      const void *key, size_t key_len);
};

/* Helper functions */
int fw_verify_checksum(const void *data, size_t len, u32 expected);
int fw_decompress_zlib(const void *src, size_t src_len,
                      void *dst, size_t *dst_len);
int fw_decompress_xz(const void *src, size_t src_len,
                    void *dst, size_t *dst_len);
int fw_version_compare(const struct fw_version *v1,
                      const struct fw_version *v2);
void fw_version_to_string(const struct fw_version *ver,
                         char *str, size_t len);

#endif /* _FW_COMMON_H_ */ 