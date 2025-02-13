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

/* TPM session flags */
#define TPM_SESSION_POLICY     BIT(0)
#define TPM_SESSION_TRIAL      BIT(1)
#define TPM_SESSION_QUOTE      BIT(2)

/* Cached policy data */
struct tpm_policy_cache {
    u8 policy_digest[TPM2_SHA256_DIGEST_SIZE];
    u32 pcr_mask;
    u64 cache_time;
    bool valid;
    spinlock_t lock;
};

/* TPM policy structure */
struct tpm_policy {
    u32 pcr_mask;
    u8 pcr_digest[TPM2_SHA256_DIGEST_SIZE];
    u8 policy_digest[TPM2_SHA256_DIGEST_SIZE];
    struct tpm_policy_cache cache;
};

/* Static policy for firmware keys */
static struct tpm_policy fw_policy = {
    .pcr_mask = (1 << TPM_PCR_FIRMWARE_CODE) |
                (1 << TPM_PCR_FIRMWARE_CONFIG) |
                (1 << TPM_PCR_KEYS),
};

/* uwu notice me senpai! This is where the magic happens~ */
static int calculate_policy_digest(struct tpm_chip *chip,
                                 struct tpm_policy *policy,
                                 bool use_cache)
{
    unsigned long flags;
    int ret = 0;

    /* Check if we can use cached policy digest */
    if (use_cache) {
        spin_lock_irqsave(&policy->cache.lock, flags);
        if (policy->cache.valid &&
            time_before64(get_jiffies_64(),
                         policy->cache.cache_time + (HZ * 300))) {
            memcpy(policy->policy_digest,
                   policy->cache.policy_digest,
                   TPM2_SHA256_DIGEST_SIZE);
            spin_unlock_irqrestore(&policy->cache.lock, flags);
            return 0;
        }
        spin_unlock_irqrestore(&policy->cache.lock, flags);
    }

    /* OwO what's this? A fresh policy calculation! */
    struct tpm2_policy_cmd cmd;
    u32 pcr_select_size = 3;
    u8 pcr_select[3] = {0};
    u32 pcr_mask = policy->pcr_mask;
    int i;

    /* Convert PCR mask to selection */
    for (i = 0; i < TPM2_MAX_PCRS && pcr_mask; i++) {
        if (pcr_mask & (1 << i))
            pcr_select[i >> 3] |= 1 << (i & 7);
        pcr_mask &= ~(1 << i);
    }

    /* Start a new policy session */
    ret = tpm2_policy_start(chip, &cmd, TPM_SESSION_TRIAL);
    if (ret)
        return ret;

    /* Add PCR policy */
    ret = tpm2_policy_pcr(chip, &cmd, pcr_select,
                         pcr_select_size, policy->pcr_digest);
    if (ret)
        goto out;

    /* Get final policy digest */
    ret = tpm2_policy_get_digest(chip, &cmd,
                                policy->policy_digest,
                                TPM2_SHA256_DIGEST_SIZE);
    if (ret)
        goto out;

    /* Cache the new policy digest */
    if (use_cache) {
        spin_lock_irqsave(&policy->cache.lock, flags);
        memcpy(policy->cache.policy_digest,
               policy->policy_digest,
               TPM2_SHA256_DIGEST_SIZE);
        policy->cache.pcr_mask = policy->pcr_mask;
        policy->cache.cache_time = get_jiffies_64();
        policy->cache.valid = true;
        spin_unlock_irqrestore(&policy->cache.lock, flags);
    }

out:
    tpm2_policy_end(chip, &cmd);
    return ret;
}

/* UwU Time to verify those quotes! */
static int verify_pcr_quote(struct tpm_chip *chip,
                          const struct tpm_policy *policy,
                          const u8 *quote_data,
                          size_t quote_size,
                          const u8 *signature,
                          size_t sig_size)
{
    struct tpm2_quote_cmd cmd;
    u8 nonce[TPM2_SHA256_DIGEST_SIZE];
    u32 pcr_update_counter;
    u8 quoted[TPM2_QUOTE_SIZE];
    size_t quoted_size = sizeof(quoted);
    int ret;

    /* Generate random nonce */
    get_random_bytes(nonce, sizeof(nonce));

    /* Get PCR quote */
    ret = tpm2_quote(chip, &cmd, policy->pcr_mask,
                    nonce, sizeof(nonce),
                    quoted, &quoted_size,
                    &pcr_update_counter);
    if (ret)
        return ret;

    /* Verify quote signature */
    ret = tpm2_verify_signature(chip, quoted, quoted_size,
                               signature, sig_size);
    if (ret)
        return ret;

    /* Compare with provided quote */
    if (quote_size != quoted_size ||
        memcmp(quote_data, quoted, quoted_size) != 0)
        return -EINVAL;

    return 0;
}

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