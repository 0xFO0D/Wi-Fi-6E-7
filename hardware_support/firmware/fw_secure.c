#include <linux/kernel.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <crypto/hash.h>
#include <crypto/public_key.h>
#include "fw_common.h"

/* Secure boot error codes */
#define SECURE_ERR_NONE       0
#define SECURE_ERR_HASH      -1
#define SECURE_ERR_SIG       -2
#define SECURE_ERR_KEY       -3
#define SECURE_ERR_ALLOC     -4

/* Firmware header magic and version */
#define FW_HEADER_MAGIC      0x57494669  /* "WiFi" */
#define FW_HEADER_VERSION    0x01

/* Secure boot header structure */
struct secure_header {
    u32 magic;
    u32 version;
    u32 img_size;
    u32 sig_size;
    u8 hash[SHA256_DIGEST_SIZE];
    u8 signature[];
} __packed;

/* TODO: Add support for key revocation */
/* TODO: Implement secure key storage */
/* TODO: Add support for multiple signature algorithms */

static int verify_hash(const void *data, size_t len,
                      const u8 *expected_hash)
{
    struct crypto_shash *tfm;
    struct shash_desc *desc;
    u8 hash[SHA256_DIGEST_SIZE];
    int ret;

    tfm = crypto_alloc_shash("sha256", 0, 0);
    if (IS_ERR(tfm))
        return SECURE_ERR_HASH;

    desc = kmalloc(sizeof(*desc) + crypto_shash_descsize(tfm),
                  GFP_KERNEL);
    if (!desc) {
        crypto_free_shash(tfm);
        return SECURE_ERR_ALLOC;
    }

    desc->tfm = tfm;

    ret = crypto_shash_init(desc);
    if (ret < 0)
        goto out;

    ret = crypto_shash_update(desc, data, len);
    if (ret < 0)
        goto out;

    ret = crypto_shash_final(desc, hash);
    if (ret < 0)
        goto out;

    if (memcmp(hash, expected_hash, SHA256_DIGEST_SIZE) != 0)
        ret = SECURE_ERR_HASH;
    else
        ret = SECURE_ERR_NONE;

out:
    kfree(desc);
    crypto_free_shash(tfm);
    return ret;
}

static int verify_signature(const void *data, size_t data_len,
                          const void *sig, size_t sig_len,
                          const void *key, size_t key_len)
{
    struct public_key_signature sig_params = {
        .digest = "sha256",
        .hash_algo = "sha256",
        .encoding = "pkcs1",
        .key_size = key_len * 8,
        .nr_mpi = 1
    };
    int ret;

    /* TODO: Add support for other signature algorithms */
    sig_params.s = kmemdup(sig, sig_len, GFP_KERNEL);
    if (!sig_params.s)
        return SECURE_ERR_ALLOC;

    ret = public_key_verify_signature(key, key_len,
                                    &sig_params,
                                    data, data_len);
    kfree(sig_params.s);

    return ret == 0 ? SECURE_ERR_NONE : SECURE_ERR_SIG;
}

int fw_secure_verify(const void *fw_data, size_t fw_len,
                    const void *key, size_t key_len)
{
    const struct secure_header *hdr = fw_data;
    const void *img_data;
    int ret;

    /* Basic header validation */
    if (fw_len < sizeof(*hdr) ||
        hdr->magic != FW_HEADER_MAGIC ||
        hdr->version != FW_HEADER_VERSION ||
        fw_len < sizeof(*hdr) + hdr->sig_size + hdr->img_size)
        return FW_ERR_VERIFY;

    img_data = fw_data + sizeof(*hdr) + hdr->sig_size;

    /* Verify image hash */
    ret = verify_hash(img_data, hdr->img_size, hdr->hash);
    if (ret != SECURE_ERR_NONE)
        return FW_ERR_VERIFY;

    /* Verify signature if key provided */
    if (key && key_len) {
        ret = verify_signature(img_data, hdr->img_size,
                             hdr->signature, hdr->sig_size,
                             key, key_len);
        if (ret != SECURE_ERR_NONE)
            return FW_ERR_SECURE;
    }

    return FW_ERR_NONE;
}

EXPORT_SYMBOL_GPL(fw_secure_verify); 