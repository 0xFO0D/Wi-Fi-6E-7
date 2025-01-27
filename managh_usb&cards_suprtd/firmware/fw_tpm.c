#include <linux/kernel.h>
#include <linux/tpm.h>
#include <linux/tpm_command.h>
#include "fw_keys.h"

/* TPM PCR indices for firmware measurements */
#define TPM_PCR_FIRMWARE_CODE     8
#define TPM_PCR_FIRMWARE_CONFIG   9
#define TPM_PCR_KEYS            10

/* TPM NV index base for key storage */
#define TPM_NV_INDEX_BASE     0x01400000
#define TPM_NV_INDEX_MASK     0x00000FFF

/* TPM policy structure */
struct tpm_policy {
    u32 pcr_mask;
    u8 pcr_digest[TPM2_SHA256_DIGEST_SIZE];
    u8 policy_digest[TPM2_SHA256_DIGEST_SIZE];
};

/* Static policy for firmware keys */
static struct tpm_policy fw_policy = {
    .pcr_mask = (1 << TPM_PCR_FIRMWARE_CODE) |
                (1 << TPM_PCR_FIRMWARE_CONFIG) |
                (1 << TPM_PCR_KEYS),
};

/* Initialize TPM support */
int fw_tpm_init(void)
{
    struct tpm_chip *chip;
    int ret;

    chip = tpm_default_chip();
    if (!chip)
        return KEY_ERR_TPM;

    /* Check TPM version and capabilities */
    if (!(chip->flags & TPM_CHIP_FLAG_TPM2))
        return KEY_ERR_TPM;

    /* Initialize PCR policy */
    ret = tpm2_pcr_read(chip, TPM_PCR_FIRMWARE_CODE,
                        fw_policy.pcr_digest);
    if (ret)
        return KEY_ERR_TPM;

    /* TODO: Calculate and extend policy digest */

    return KEY_ERR_NONE;
}

void fw_tpm_exit(void)
{
    /* Nothing to clean up currently */
}

/* Store a key in TPM NV storage */
int fw_tpm_store_key(const struct key_entry *entry)
{
    struct tpm_chip *chip;
    u32 nv_index;
    int ret;

    if (!entry || !entry->key_data || !entry->key_len)
        return KEY_ERR_INVALID;

    chip = tpm_default_chip();
    if (!chip)
        return KEY_ERR_TPM;

    /* Calculate NV index for key */
    nv_index = TPM_NV_INDEX_BASE | (entry->meta.id & TPM_NV_INDEX_MASK);

    /* Define NV space with policy */
    ret = tpm2_nv_define_space(chip, nv_index, entry->key_len,
                              TPM2_NT_ORDINARY,
                              fw_policy.policy_digest,
                              sizeof(fw_policy.policy_digest));
    if (ret)
        return KEY_ERR_TPM;

    /* Write key data */
    ret = tpm2_nv_write(chip, nv_index, 0,
                        entry->key_data, entry->key_len);
    if (ret) {
        tpm2_nv_undefine_space(chip, nv_index);
        return KEY_ERR_TPM;
    }

    return KEY_ERR_NONE;
}

/* Load a key from TPM NV storage */
int fw_tpm_load_key(u32 key_id, struct key_entry *entry)
{
    struct tpm_chip *chip;
    u32 nv_index;
    size_t key_len;
    void *key_data;
    int ret;

    if (!entry)
        return KEY_ERR_INVALID;

    chip = tpm_default_chip();
    if (!chip)
        return KEY_ERR_TPM;

    /* Calculate NV index for key */
    nv_index = TPM_NV_INDEX_BASE | (key_id & TPM_NV_INDEX_MASK);

    /* Get NV space size */
    ret = tpm2_nv_read_public(chip, nv_index, &key_len);
    if (ret)
        return KEY_ERR_TPM;

    /* Allocate buffer for key data */
    key_data = kmalloc(key_len, GFP_KERNEL);
    if (!key_data)
        return KEY_ERR_STORAGE;

    /* Read key data */
    ret = tpm2_nv_read(chip, nv_index, 0, key_data, key_len);
    if (ret) {
        kfree(key_data);
        return KEY_ERR_TPM;
    }

    entry->key_data = key_data;
    entry->key_len = key_len;

    return KEY_ERR_NONE;
}

/* Verify a key against TPM PCR values */
int fw_tpm_verify_key(const struct key_entry *entry)
{
    struct tpm_chip *chip;
    u8 pcr_digest[TPM2_SHA256_DIGEST_SIZE];
    int ret;

    if (!entry)
        return KEY_ERR_INVALID;

    chip = tpm_default_chip();
    if (!chip)
        return KEY_ERR_TPM;

    /* Read current PCR values */
    ret = tpm2_pcr_read(chip, TPM_PCR_FIRMWARE_CODE, pcr_digest);
    if (ret)
        return KEY_ERR_TPM;

    /* TODO: Verify PCR values against policy */
    /* TODO: Implement PCR quote verification */

    return KEY_ERR_NONE;
}

EXPORT_SYMBOL_GPL(fw_tpm_init);
EXPORT_SYMBOL_GPL(fw_tpm_exit);
EXPORT_SYMBOL_GPL(fw_tpm_store_key);
EXPORT_SYMBOL_GPL(fw_tpm_load_key);
EXPORT_SYMBOL_GPL(fw_tpm_verify_key); 