#include <linux/module.h>
#include <linux/crypto.h>
#include <crypto/aes.h>
#include <crypto/skcipher.h>
#include <crypto/hash.h>
#include <crypto/aead.h>
#include "../../include/crypto/crypto_core.h"
#include "../../include/core/wifi67.h"
#include "../../include/core/wifi67_debug.h"

static int wifi67_crypto_alloc_tfm(struct wifi67_crypto_key *key)
{
    key->aead = crypto_alloc_aead("gcm(aes)", 0, CRYPTO_ALG_ASYNC);
    if (IS_ERR(key->aead))
        return PTR_ERR(key->aead);

    return 0;
}

int wifi67_crypto_init(struct wifi67_priv *priv)
{
    struct wifi67_crypto *crypto = &priv->crypto;
    int i;

    spin_lock_init(&crypto->lock);

    for (i = 0; i < WIFI67_NUM_KEYS; i++) {
        crypto->keys[i].valid = false;
        crypto->keys[i].aead = NULL;
    }

    atomic_set(&crypto->tx_encrypted, 0);
    atomic_set(&crypto->tx_failed, 0);
    atomic_set(&crypto->rx_decrypted, 0);
    atomic_set(&crypto->rx_failed, 0);

    return 0;
}

void wifi67_crypto_deinit(struct wifi67_priv *priv)
{
    struct wifi67_crypto *crypto = &priv->crypto;
    int i;

    for (i = 0; i < WIFI67_NUM_KEYS; i++) {
        if (crypto->keys[i].valid) {
            crypto_free_aead(crypto->keys[i].aead);
            crypto->keys[i].valid = false;
        }
    }
}

int wifi67_crypto_set_key(struct wifi67_priv *priv, u8 key_idx,
                         const u8 *key, u32 key_len, u8 cipher)
{
    struct wifi67_crypto *crypto = &priv->crypto;
    struct wifi67_crypto_key *crypto_key;
    int ret;

    if (key_idx >= WIFI67_NUM_KEYS)
        return -EINVAL;

    crypto_key = &crypto->keys[key_idx];

    if (crypto_key->valid)
        crypto_free_aead(crypto_key->aead);

    ret = wifi67_crypto_alloc_tfm(crypto_key);
    if (ret)
        return ret;

    memcpy(crypto_key->key, key, key_len);
    crypto_key->key_len = key_len;
    crypto_key->key_idx = key_idx;
    crypto_key->cipher = cipher;
    crypto_key->valid = true;

    ret = crypto_aead_setkey(crypto_key->aead, key, key_len);
    if (ret) {
        crypto_free_aead(crypto_key->aead);
        crypto_key->valid = false;
        return ret;
    }

    return 0;
}

int wifi67_crypto_encrypt(struct wifi67_priv *priv, struct sk_buff *skb,
                         u8 key_idx)
{
    struct wifi67_crypto *crypto = &priv->crypto;
    struct wifi67_crypto_key *crypto_key;
    struct aead_request *req;
    struct scatterlist sg;
    int ret;

    if (key_idx >= WIFI67_NUM_KEYS)
        return -EINVAL;

    crypto_key = &crypto->keys[key_idx];
    if (!crypto_key->valid)
        return -EINVAL;

    req = aead_request_alloc(crypto_key->aead, GFP_ATOMIC);
    if (!req)
        return -ENOMEM;

    sg_init_one(&sg, skb->data, skb->len);
    aead_request_set_crypt(req, &sg, &sg, skb->len, crypto_key->iv);

    ret = crypto_aead_encrypt(req);
    aead_request_free(req);

    if (ret)
        atomic_inc(&crypto->tx_failed);
    else
        atomic_inc(&crypto->tx_encrypted);

    return ret;
}

int wifi67_crypto_decrypt(struct wifi67_priv *priv, struct sk_buff *skb,
                         u8 key_idx)
{
    struct wifi67_crypto *crypto = &priv->crypto;
    struct wifi67_crypto_key *crypto_key;
    struct aead_request *req;
    struct scatterlist sg;
    int ret;

    if (key_idx >= WIFI67_NUM_KEYS)
        return -EINVAL;

    crypto_key = &crypto->keys[key_idx];
    if (!crypto_key->valid)
        return -EINVAL;

    req = aead_request_alloc(crypto_key->aead, GFP_ATOMIC);
    if (!req)
        return -ENOMEM;

    sg_init_one(&sg, skb->data, skb->len);
    aead_request_set_crypt(req, &sg, &sg, skb->len, crypto_key->iv);

    ret = crypto_aead_decrypt(req);
    aead_request_free(req);

    if (ret)
        atomic_inc(&crypto->rx_failed);
    else
        atomic_inc(&crypto->rx_decrypted);

    return ret;
} 