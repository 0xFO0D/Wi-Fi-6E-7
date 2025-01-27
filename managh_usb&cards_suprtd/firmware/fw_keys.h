#ifndef _FW_KEYS_H_
#define _FW_KEYS_H_

#include <linux/types.h>
#include <linux/crypto.h>

/* Key storage error codes */
#define KEY_ERR_NONE           0
#define KEY_ERR_INVALID       -1
#define KEY_ERR_REVOKED       -2
#define KEY_ERR_EXPIRED       -3
#define KEY_ERR_STORAGE       -4
#define KEY_ERR_TPM           -5
#define KEY_ERR_KEYRING       -6

/* Key types */
#define KEY_TYPE_RSA_2048     1
#define KEY_TYPE_RSA_4096     2
#define KEY_TYPE_ECDSA_256    3
#define KEY_TYPE_ECDSA_384    4

/* Key flags */
#define KEY_FLAG_NONE         0
#define KEY_FLAG_REVOKED      BIT(0)
#define KEY_FLAG_EXPIRED      BIT(1)
#define KEY_FLAG_PRIMARY      BIT(2)
#define KEY_FLAG_BACKUP       BIT(3)
#define KEY_FLAG_TPM_STORED   BIT(4)

/* Key version structure */
struct key_version {
    u8 major;
    u8 minor;
    u16 revision;
};

/* Key metadata structure */
struct key_meta {
    u32 id;
    u32 type;
    u32 flags;
    struct key_version version;
    u64 creation_time;
    u64 expiration_time;
    u8 fingerprint[32];
};

/* Key entry structure */
struct key_entry {
    struct key_meta meta;
    void *key_data;
    size_t key_len;
    struct list_head list;
};

/* Key operations */
struct key_ops {
    int (*store)(const struct key_entry *entry);
    int (*load)(u32 key_id, struct key_entry *entry);
    int (*revoke)(u32 key_id);
    int (*verify)(const struct key_entry *entry);
    int (*rotate)(u32 old_id, const struct key_entry *new_entry);
};

/* Key management functions */
int fw_key_init(void);
void fw_key_exit(void);
int fw_key_add(const struct key_entry *entry);
int fw_key_remove(u32 key_id);
int fw_key_revoke(u32 key_id);
int fw_key_get(u32 key_id, struct key_entry *entry);
int fw_key_list(struct key_meta *meta_list, size_t *count);
int fw_key_rotate(u32 old_id, const struct key_entry *new_entry);

/* TPM support functions */
int fw_tpm_init(void);
void fw_tpm_exit(void);
int fw_tpm_store_key(const struct key_entry *entry);
int fw_tpm_load_key(u32 key_id, struct key_entry *entry);
int fw_tpm_verify_key(const struct key_entry *entry);

#endif /* _FW_KEYS_H_ */ 