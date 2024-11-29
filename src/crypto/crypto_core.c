#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/crypto.h>
#include <crypto/aead.h>
#include <crypto/skcipher.h>
#include <crypto/aes.h>
#include <linux/scatterlist.h>
#include "../../include/crypto/crypto_core.h"

#define WIFI67_CRYPTO_REG_CTRL        0x0000
#define WIFI67_CRYPTO_REG_STATUS      0x0004
#define WIFI67_CRYPTO_REG_KEY         0x0008
#define WIFI67_CRYPTO_REG_IV          0x000C

#define WIFI67_CRYPTO_CTRL_ENABLE     BIT(0)
#define WIFI67_CRYPTO_CTRL_ENCRYPT    BIT(1)
#define WIFI67_CRYPTO_CTRL_DECRYPT    BIT(2)

static int wifi67_crypto_aead_encrypt(struct wifi67_crypto_ctx *ctx, 
                                    struct wifi67_crypto_key *key,
                                    struct sk_buff *skb, u8 *iv)
{
    struct aead_request *req;
    struct scatterlist sg[2];
    int ret;

    req = aead_request_alloc(ctx->tfm_aead, GFP_KERNEL);
    if (!req)
        return -ENOMEM;

    sg_init_table(sg, 2);
    sg_set_buf(&sg[0], skb->data, skb->len);
    sg_set_buf(&sg[1], iv, WIFI67_CRYPTO_MAX_IV_SIZE);

    aead_request_set_tfm(req, ctx->tfm_aead);
    aead_request_set_callback(req, CRYPTO_TFM_REQ_MAY_SLEEP, NULL, NULL);
    aead_request_set_crypt(req, sg, sg, skb->len, iv);
    aead_request_set_ad(req, 0);

    ret = crypto_aead_encrypt(req);
    aead_request_free(req);

    return ret;
}

static int wifi67_crypto_skcipher_encrypt(struct wifi67_crypto_ctx *ctx,
                                        struct wifi67_crypto_key *key,
                                        struct sk_buff *skb, u8 *iv)
{
    struct skcipher_request *req;
    struct scatterlist sg;
    int ret;

    req = skcipher_request_alloc(ctx->tfm_cipher, GFP_KERNEL);
    if (!req)
        return -ENOMEM;

    sg_init_one(&sg, skb->data, skb->len);
    skcipher_request_set_tfm(req, ctx->tfm_cipher);
    skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_SLEEP, NULL, NULL);
    skcipher_request_set_crypt(req, &sg, &sg, skb->len, iv);

    ret = crypto_skcipher_encrypt(req);
    skcipher_request_free(req);

    return ret;
}

int wifi67_crypto_encrypt(struct wifi67_priv *priv, struct sk_buff *skb,
                         u8 key_idx, u8 *iv)
{
    struct wifi67_crypto_ctx *ctx = priv->crypto_ctx;
    struct wifi67_crypto_key *key;
    unsigned long flags;
    u32 val;
    int ret = 0;

    if (!ctx || !ctx->initialized)
        return -EINVAL;

    if (key_idx >= WIFI67_CRYPTO_MAX_KEYS)
        return -EINVAL;

    key = &ctx->keys[key_idx];
    if (!key->valid)
        return -EINVAL;

    spin_lock_irqsave(&ctx->lock, flags);

    /* Set encryption mode and key index */
    val = WIFI67_CRYPTO_CTRL_ENABLE | WIFI67_CRYPTO_CTRL_ENCRYPT;
    writel(val, ctx->regs + WIFI67_CRYPTO_REG_CTRL);
    writel(key_idx, ctx->regs + WIFI67_CRYPTO_REG_KEY);

    /* Set IV */
    memcpy_toio(ctx->regs + WIFI67_CRYPTO_REG_IV, iv, WIFI67_CRYPTO_MAX_IV_SIZE);

    /* Perform encryption */
    if (key->cipher == WLAN_CIPHER_SUITE_CCMP) {
        ret = wifi67_crypto_aead_encrypt(ctx, key, skb, iv);
    } else if (key->cipher == WLAN_CIPHER_SUITE_TKIP) {
        ret = wifi67_crypto_skcipher_encrypt(ctx, key, skb, iv);
    }

    spin_unlock_irqrestore(&ctx->lock, flags);

    return ret;
}

static int wifi67_crypto_aead_decrypt(struct wifi67_crypto_ctx *ctx,
                                    struct wifi67_crypto_key *key,
                                    struct sk_buff *skb, u8 *iv)
{
    struct aead_request *req;
    struct scatterlist sg[2];
    int ret;

    req = aead_request_alloc(ctx->tfm_aead, GFP_KERNEL);
    if (!req)
        return -ENOMEM;

    sg_init_table(sg, 2);
    sg_set_buf(&sg[0], skb->data, skb->len);
    sg_set_buf(&sg[1], iv, WIFI67_CRYPTO_MAX_IV_SIZE);

    aead_request_set_tfm(req, ctx->tfm_aead);
    aead_request_set_callback(req, CRYPTO_TFM_REQ_MAY_SLEEP, NULL, NULL);
    aead_request_set_crypt(req, sg, sg, skb->len, iv);
    aead_request_set_ad(req, 0);

    ret = crypto_aead_decrypt(req);
    aead_request_free(req);

    return ret;
}

static int wifi67_crypto_skcipher_decrypt(struct wifi67_crypto_ctx *ctx,
                                        struct wifi67_crypto_key *key,
                                        struct sk_buff *skb, u8 *iv)
{
    struct skcipher_request *req;
    struct scatterlist sg;
    int ret;

    req = skcipher_request_alloc(ctx->tfm_cipher, GFP_KERNEL);
    if (!req)
        return -ENOMEM;

    sg_init_one(&sg, skb->data, skb->len);
    skcipher_request_set_tfm(req, ctx->tfm_cipher);
    skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_SLEEP, NULL, NULL);
    skcipher_request_set_crypt(req, &sg, &sg, skb->len, iv);

    ret = crypto_skcipher_decrypt(req);
    skcipher_request_free(req);

    return ret;
}

int wifi67_crypto_decrypt(struct wifi67_priv *priv, struct sk_buff *skb,
                         u8 key_idx, u8 *iv)
{
    struct wifi67_crypto_ctx *ctx = priv->crypto_ctx;
    struct wifi67_crypto_key *key;
    unsigned long flags;
    u32 val;
    int ret = 0;

    if (!ctx || !ctx->initialized)
        return -EINVAL;

    if (key_idx >= WIFI67_CRYPTO_MAX_KEYS)
        return -EINVAL;

    key = &ctx->keys[key_idx];
    if (!key->valid)
        return -EINVAL;

    spin_lock_irqsave(&ctx->lock, flags);

    /* Set decryption mode and key index */
    val = WIFI67_CRYPTO_CTRL_ENABLE | WIFI67_CRYPTO_CTRL_DECRYPT;
    writel(val, ctx->regs + WIFI67_CRYPTO_REG_CTRL);
    writel(key_idx, ctx->regs + WIFI67_CRYPTO_REG_KEY);

    /* Set IV */
    memcpy_toio(ctx->regs + WIFI67_CRYPTO_REG_IV, iv, WIFI67_CRYPTO_MAX_IV_SIZE);

    /* Perform decryption */
    if (key->cipher == WLAN_CIPHER_SUITE_CCMP) {
        ret = wifi67_crypto_aead_decrypt(ctx, key, skb, iv);
    } else if (key->cipher == WLAN_CIPHER_SUITE_TKIP) {
        ret = wifi67_crypto_skcipher_decrypt(ctx, key, skb, iv);
    }

    spin_unlock_irqrestore(&ctx->lock, flags);

    return ret;
}

EXPORT_SYMBOL_GPL(wifi67_crypto_init);
EXPORT_SYMBOL_GPL(wifi67_crypto_deinit);
EXPORT_SYMBOL_GPL(wifi67_crypto_set_key);
EXPORT_SYMBOL_GPL(wifi67_crypto_encrypt);
EXPORT_SYMBOL_GPL(wifi67_crypto_decrypt); 