#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include "../../include/crypto/crypto_core.h"

static int wifi67_crypto_alloc_tfm(struct wifi67_crypto_key *key)
{
    switch (key->alg) {
    case CRYPTO_ALG_AES:
        key->tfm = crypto_alloc_skcipher("cbc(aes)", 0, 0);
        if (IS_ERR(key->tfm))
            return PTR_ERR(key->tfm);
        break;
    case CRYPTO_ALG_GCMP:
        key->aead = crypto_alloc_aead("gcm(aes)", 0, 0);
        if (IS_ERR(key->aead))
            return PTR_ERR(key->aead);
        break;
    case CRYPTO_ALG_CCMP:
        key->aead = crypto_alloc_aead("ccm(aes)", 0, 0);
        if (IS_ERR(key->aead))
            return PTR_ERR(key->aead);
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

int wifi67_crypto_init(struct wifi67_priv *priv)
{
    struct wifi67_crypto *crypto;
    int i;

    crypto = kzalloc(sizeof(*crypto), GFP_KERNEL);
    if (!crypto)
        return -ENOMEM;

    priv->crypto = crypto;
    spin_lock_init(&crypto->lock);

    for (i = 0; i < WIFI67_MAX_KEY_ENTRIES; i++) {
        crypto->keys[i].alg = CRYPTO_ALG_NONE;
    }

    return 0;
}

void wifi67_crypto_deinit(struct wifi67_priv *priv)
{
    struct wifi67_crypto *crypto = priv->crypto;
    int i;

    if (!crypto)
        return;

    for (i = 0; i < WIFI67_MAX_KEY_ENTRIES; i++) {
        if (crypto->keys[i].tfm)
            crypto_free_skcipher(crypto->keys[i].tfm);
        if (crypto->keys[i].aead)
            crypto_free_aead(crypto->keys[i].aead);
    }

    kfree(crypto);
    priv->crypto = NULL;
}

int wifi67_crypto_set_key(struct wifi67_priv *priv, u8 idx, const u8 *key,
                          u32 key_len, enum crypto_alg alg)
{
    struct wifi67_crypto *crypto = priv->crypto;
    struct wifi67_crypto_key *crypto_key;
    int ret;

    if (idx >= WIFI67_MAX_KEY_ENTRIES)
        return -EINVAL;

    spin_lock(&crypto->lock);

    crypto_key = &crypto->keys[idx];
    memcpy(crypto_key->key, key, key_len);
    crypto_key->key_len = key_len;
    crypto_key->alg = alg;

    ret = wifi67_crypto_alloc_tfm(crypto_key);
    if (ret) {
        spin_unlock(&crypto->lock);
        return ret;
    }

    if (crypto_key->tfm)
        crypto_skcipher_setkey(crypto_key->tfm, key, key_len);
    if (crypto_key->aead)
        crypto_aead_setkey(crypto_key->aead, key, key_len);

    spin_unlock(&crypto->lock);
    return 0;
}

int wifi67_crypto_encrypt(struct wifi67_priv *priv, u8 idx, struct sk_buff *skb)
{
    struct wifi67_crypto *crypto = priv->crypto;
    struct wifi67_crypto_key *crypto_key;
    struct scatterlist sg;
    struct skcipher_request *req;
    int ret;

    if (idx >= WIFI67_MAX_KEY_ENTRIES)
        return -EINVAL;

    spin_lock(&crypto->lock);
    crypto_key = &crypto->keys[idx];
    if (crypto_key->alg == CRYPTO_ALG_NONE) {
        spin_unlock(&crypto->lock);
        return -EINVAL;
    }

    req = skcipher_request_alloc(crypto_key->tfm, GFP_ATOMIC);
    if (!req) {
        spin_unlock(&crypto->lock);
        return -ENOMEM;
    }

    sg_init_one(&sg, skb->data, skb->len);
    skcipher_request_set_crypt(req, &sg, &sg, skb->len, crypto_key->iv);
    ret = crypto_skcipher_encrypt(req);
    skcipher_request_free(req);

    spin_unlock(&crypto->lock);
    return ret;
}

int wifi67_crypto_decrypt(struct wifi67_priv *priv, u8 idx, struct sk_buff *skb)
{
    struct wifi67_crypto *crypto = priv->crypto;
    struct wifi67_crypto_key *crypto_key;
    struct scatterlist sg;
    struct skcipher_request *req;
    int ret;

    if (idx >= WIFI67_MAX_KEY_ENTRIES)
        return -EINVAL;

    spin_lock(&crypto->lock);
    crypto_key = &crypto->keys[idx];
    if (crypto_key->alg == CRYPTO_ALG_NONE) {
        spin_unlock(&crypto->lock);
        return -EINVAL;
    }

    req = skcipher_request_alloc(crypto_key->tfm, GFP_ATOMIC);
    if (!req) {
        spin_unlock(&crypto->lock);
        return -ENOMEM;
    }

    sg_init_one(&sg, skb->data, skb->len);
    skcipher_request_set_crypt(req, &sg, &sg, skb->len, crypto_key->iv);
    ret = crypto_skcipher_decrypt(req);
    skcipher_request_free(req);

    spin_unlock(&crypto->lock);
    return ret;
} 