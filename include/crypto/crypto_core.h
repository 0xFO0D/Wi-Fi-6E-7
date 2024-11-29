#ifndef _WIFI67_CRYPTO_CORE_H_
#define _WIFI67_CRYPTO_CORE_H_

#include <linux/types.h>
#include <crypto/aead.h>
#include "../core/wifi67_forward.h"

#define WIFI67_MAX_KEY_ENTRIES 32
#define WIFI67_MAX_KEY_LEN    32
#define WIFI67_IV_LEN         12
#define WIFI67_MIC_LEN        16

/* Supported crypto algorithms */
enum wifi67_crypto_alg {
    WIFI67_CRYPTO_AES_GCM,
    WIFI67_CRYPTO_AES_CCM,
};

struct wifi67_crypto_key {
    bool valid;
    u8 key[WIFI67_MAX_KEY_LEN];
    u32 key_len;
    u8 iv[WIFI67_IV_LEN];
    struct crypto_aead *aead;
};

struct wifi67_crypto {
    struct wifi67_crypto_key keys[WIFI67_MAX_KEY_ENTRIES];
    spinlock_t lock;
};

/* Function declarations */
int wifi67_crypto_init(struct wifi67_priv *priv);
void wifi67_crypto_deinit(struct wifi67_priv *priv);
int wifi67_crypto_set_key(struct wifi67_priv *priv, u8 key_idx,
                         const u8 *key, u32 key_len);
int wifi67_crypto_encrypt(struct wifi67_priv *priv, u8 key_idx,
                         struct sk_buff *skb);
int wifi67_crypto_decrypt(struct wifi67_priv *priv, u8 key_idx,
                         struct sk_buff *skb);

#endif /* _WIFI67_CRYPTO_CORE_H_ */ 