#include <linux/kernel.h>
#include <linux/tpm.h>
#include <linux/tpm_command.h>
#include <linux/scatterlist.h>
#include <crypto/aead.h>
#include "fw_keys.h"
#include "fw_encrypt.h"

/* TPM key object for firmware encryption */
#define TPM_FW_KEY_HANDLE 0x81000100

/* Initialize encryption support */
int fw_encrypt_init(void)
{
    struct tpm_chip *chip;
    int ret;

    chip = tpm_default_chip();
    if (!chip)
        return -ENODEV;

    /* Create primary key if it doesn't exist */
    ret = tpm2_get_object_attributes(chip, TPM_FW_KEY_HANDLE, NULL);
    if (ret < 0) {
        ret = tpm2_create_primary_key(chip, TPM_FW_KEY_HANDLE);
        if (ret < 0)
            return ret;
    }

    return 0;
}

/* Encrypt firmware data */
int fw_encrypt_data(const void *data, size_t data_len,
                   void *out, size_t *out_len,
                   u8 *tag, size_t tag_len)
{
    struct tpm_chip *chip;
    struct crypto_aead *tfm;
    struct aead_request *req;
    struct scatterlist sg[3];
    u8 iv[16];
    u8 key[32];
    int ret;

    if (!data || !out || !out_len || !tag)
        return -EINVAL;

    if (*out_len < data_len + 16)
        return -ENOSPC;

    chip = tpm_default_chip();
    if (!chip)
        return -ENODEV;

    /* Get encryption key from TPM */
    ret = tpm2_get_random(chip, iv, sizeof(iv));
    if (ret < 0)
        return ret;

    ret = tpm2_derive_key(chip, TPM_FW_KEY_HANDLE, key, sizeof(key));
    if (ret < 0)
        return ret;

    /* Initialize AEAD cipher */
    tfm = crypto_alloc_aead("gcm(aes)", 0, 0);
    if (IS_ERR(tfm))
        return PTR_ERR(tfm);

    ret = crypto_aead_setkey(tfm, key, sizeof(key));
    if (ret < 0)
        goto out_free_tfm;

    ret = crypto_aead_setauthsize(tfm, 16);
    if (ret < 0)
        goto out_free_tfm;

    /* Prepare encryption request */
    req = aead_request_alloc(tfm, GFP_KERNEL);
    if (!req) {
        ret = -ENOMEM;
        goto out_free_tfm;
    }

    sg_init_table(sg, 3);
    sg_set_buf(&sg[0], iv, sizeof(iv));
    sg_set_buf(&sg[1], data, data_len);
    sg_set_buf(&sg[2], tag, 16);

    aead_request_set_crypt(req, sg, sg, data_len, iv);
    aead_request_set_ad(req, sizeof(iv));

    /* Perform encryption */
    ret = crypto_aead_encrypt(req);
    if (ret < 0)
        goto out_free_req;

    /* Copy encrypted data */
    memcpy(out, data, data_len);
    *out_len = data_len + 16;
    memcpy(tag, sg[2].data, 16);

    ret = 0;

out_free_req:
    aead_request_free(req);
out_free_tfm:
    crypto_free_aead(tfm);
    return ret;
}

/* Decrypt firmware data */
int fw_decrypt_data(const void *data, size_t data_len,
                   void *out, size_t *out_len,
                   const u8 *tag, size_t tag_len)
{
    struct tpm_chip *chip;
    struct crypto_aead *tfm;
    struct aead_request *req;
    struct scatterlist sg[3];
    u8 iv[16];
    u8 key[32];
    int ret;

    if (!data || !out || !out_len || !tag)
        return -EINVAL;

    if (data_len < 16)
        return -EINVAL;

    chip = tpm_default_chip();
    if (!chip)
        return -ENODEV;

    /* Get decryption key from TPM */
    ret = tpm2_derive_key(chip, TPM_FW_KEY_HANDLE, key, sizeof(key));
    if (ret < 0)
        return ret;

    /* Initialize AEAD cipher */
    tfm = crypto_alloc_aead("gcm(aes)", 0, 0);
    if (IS_ERR(tfm))
        return PTR_ERR(tfm);

    ret = crypto_aead_setkey(tfm, key, sizeof(key));
    if (ret < 0)
        goto out_free_tfm;

    ret = crypto_aead_setauthsize(tfm, 16);
    if (ret < 0)
        goto out_free_tfm;

    /* Prepare decryption request */
    req = aead_request_alloc(tfm, GFP_KERNEL);
    if (!req) {
        ret = -ENOMEM;
        goto out_free_tfm;
    }

    sg_init_table(sg, 3);
    sg_set_buf(&sg[0], iv, sizeof(iv));
    sg_set_buf(&sg[1], data, data_len - 16);
    sg_set_buf(&sg[2], tag, 16);

    aead_request_set_crypt(req, sg, sg, data_len - 16, iv);
    aead_request_set_ad(req, sizeof(iv));

    /* Perform decryption */
    ret = crypto_aead_decrypt(req);
    if (ret < 0)
        goto out_free_req;

    /* Copy decrypted data */
    memcpy(out, data, data_len - 16);
    *out_len = data_len - 16;

    ret = 0;

out_free_req:
    aead_request_free(req);
out_free_tfm:
    crypto_free_aead(tfm);
    return ret;
}

EXPORT_SYMBOL_GPL(fw_encrypt_init);
EXPORT_SYMBOL_GPL(fw_encrypt_data);
EXPORT_SYMBOL_GPL(fw_decrypt_data); 