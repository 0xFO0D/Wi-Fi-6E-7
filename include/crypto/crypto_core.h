#ifndef _WIFI67_CRYPTO_CORE_H_
#define _WIFI67_CRYPTO_CORE_H_

#include <linux/types.h>
#include <linux/crypto.h>
#include <crypto/aes.h>
#include <crypto/hash.h>
#include <crypto/skcipher.h>

#define MAX_KEY_SIZE 32
#define MAX_IV_SIZE 16

enum crypto_alg {
    CRYPTO_ALG_NONE,
    CRYPTO_ALG_AES,
    CRYPTO_ALG_GCMP,
    CRYPTO_ALG_CCMP,
    CRYPTO_ALG_TKIP,
    CRYPTO_ALG_WEP,
};

struct wifi67_crypto_key {
    u8 key[MAX_KEY_SIZE];
    u8 iv[MAX_IV_SIZE];
    u32 key_len;
    u32 iv_len;
    enum crypto_alg alg;
    struct crypto_skcipher *tfm;
    struct crypto_aead *aead;
    struct crypto_shash *hash;
};

struct wifi67_crypto {
    struct wifi67_crypto_key keys[WIFI67_MAX_KEY_ENTRIES];
    spinlock_t lock;
};

/* Function prototypes */
int wifi67_crypto_init(struct wifi67_priv *priv);
void wifi67_crypto_deinit(struct wifi67_priv *priv);
int wifi67_crypto_set_key(struct wifi67_priv *priv, u8 idx, const u8 *key,
                          u32 key_len, enum crypto_alg alg);
int wifi67_crypto_encrypt(struct wifi67_priv *priv, u8 idx, struct sk_buff *skb);
int wifi67_crypto_decrypt(struct wifi67_priv *priv, u8 idx, struct sk_buff *skb);

#endif /* _WIFI67_CRYPTO_CORE_H_ */ 