#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/crypto.h>
#include <crypto/hash.h>
#include "fw_keys.h"
#include "fw_policy_sim.h"

/* Simulated PCR values */
static u8 sim_pcr_values[TPM2_MAX_PCRS][TPM2_SHA256_DIGEST_SIZE];
static DEFINE_MUTEX(sim_lock);

/* Initialize policy simulator */
int fw_policy_sim_init(void)
{
    mutex_init(&sim_lock);
    memset(sim_pcr_values, 0, sizeof(sim_pcr_values));
    return 0;
}

/* Set simulated PCR value */
int fw_policy_sim_set_pcr(u32 pcr_index, const u8 *value)
{
    if (pcr_index >= TPM2_MAX_PCRS || !value)
        return -EINVAL;

    mutex_lock(&sim_lock);
    memcpy(sim_pcr_values[pcr_index], value, TPM2_SHA256_DIGEST_SIZE);
    mutex_unlock(&sim_lock);
    return 0;
}

/* Get simulated PCR value */
int fw_policy_sim_get_pcr(u32 pcr_index, u8 *value)
{
    if (pcr_index >= TPM2_MAX_PCRS || !value)
        return -EINVAL;

    mutex_lock(&sim_lock);
    memcpy(value, sim_pcr_values[pcr_index], TPM2_SHA256_DIGEST_SIZE);
    mutex_unlock(&sim_lock);
    return 0;
}

/* Extend simulated PCR */
int fw_policy_sim_extend_pcr(u32 pcr_index, const u8 *data, size_t len)
{
    struct crypto_shash *tfm;
    struct shash_desc *desc;
    u8 digest[TPM2_SHA256_DIGEST_SIZE];
    int ret;

    if (pcr_index >= TPM2_MAX_PCRS || !data)
        return -EINVAL;

    /* Initialize hash context */
    tfm = crypto_alloc_shash("sha256", 0, 0);
    if (IS_ERR(tfm))
        return PTR_ERR(tfm);

    desc = kzalloc(sizeof(*desc) + crypto_shash_descsize(tfm), GFP_KERNEL);
    if (!desc) {
        crypto_free_shash(tfm);
        return -ENOMEM;
    }

    desc->tfm = tfm;

    mutex_lock(&sim_lock);

    /* Hash previous value */
    ret = crypto_shash_init(desc);
    if (ret < 0)
        goto out;

    ret = crypto_shash_update(desc, sim_pcr_values[pcr_index],
                             TPM2_SHA256_DIGEST_SIZE);
    if (ret < 0)
        goto out;

    /* Hash new data */
    ret = crypto_shash_update(desc, data, len);
    if (ret < 0)
        goto out;

    ret = crypto_shash_final(desc, digest);
    if (ret < 0)
        goto out;

    /* Update PCR value */
    memcpy(sim_pcr_values[pcr_index], digest, TPM2_SHA256_DIGEST_SIZE);

out:
    mutex_unlock(&sim_lock);
    crypto_free_shash(tfm);
    kfree(desc);
    return ret;
}

/* Simulate policy evaluation */
int fw_policy_sim_evaluate(const struct tpm_policy_info *policy,
                          u8 *policy_digest)
{
    struct crypto_shash *tfm;
    struct shash_desc *desc;
    u8 pcr_composite[TPM2_MAX_PCRS * TPM2_SHA256_DIGEST_SIZE];
    size_t composite_len = 0;
    int ret, i;

    if (!policy || !policy_digest)
        return -EINVAL;

    /* Initialize hash context */
    tfm = crypto_alloc_shash("sha256", 0, 0);
    if (IS_ERR(tfm))
        return PTR_ERR(tfm);

    desc = kzalloc(sizeof(*desc) + crypto_shash_descsize(tfm), GFP_KERNEL);
    if (!desc) {
        crypto_free_shash(tfm);
        return -ENOMEM;
    }

    desc->tfm = tfm;

    mutex_lock(&sim_lock);

    /* Build PCR composite */
    for (i = 0; i < TPM2_MAX_PCRS; i++) {
        if (policy->pcr_mask & (1 << i)) {
            memcpy(pcr_composite + composite_len,
                   sim_pcr_values[i], TPM2_SHA256_DIGEST_SIZE);
            composite_len += TPM2_SHA256_DIGEST_SIZE;
        }
    }

    /* Calculate policy digest */
    ret = crypto_shash_init(desc);
    if (ret < 0)
        goto out;

    ret = crypto_shash_update(desc, (u8 *)&policy->pcr_mask,
                             sizeof(policy->pcr_mask));
    if (ret < 0)
        goto out;

    ret = crypto_shash_update(desc, pcr_composite, composite_len);
    if (ret < 0)
        goto out;

    ret = crypto_shash_final(desc, policy_digest);

out:
    mutex_unlock(&sim_lock);
    crypto_free_shash(tfm);
    kfree(desc);
    return ret;
}

/* Reset simulator state */
void fw_policy_sim_reset(void)
{
    mutex_lock(&sim_lock);
    memset(sim_pcr_values, 0, sizeof(sim_pcr_values));
    mutex_unlock(&sim_lock);
}

EXPORT_SYMBOL_GPL(fw_policy_sim_init);
EXPORT_SYMBOL_GPL(fw_policy_sim_set_pcr);
EXPORT_SYMBOL_GPL(fw_policy_sim_get_pcr);
EXPORT_SYMBOL_GPL(fw_policy_sim_extend_pcr);
EXPORT_SYMBOL_GPL(fw_policy_sim_evaluate);
EXPORT_SYMBOL_GPL(fw_policy_sim_reset); 