#ifndef _WIFI67_CRYPTO_CORE_H_
#define _WIFI67_CRYPTO_CORE_H_

#include <linux/types.h>
#include <linux/crypto.h>
#include "../core/wifi67.h"

#define WIFI67_CRYPTO_MAX_KEY_SIZE    32
#define WIFI67_CRYPTO_MAX_IV_SIZE     16
#define WIFI67_CRYPTO_BLOCK_SIZE      16
#define WIFI67_CRYPTO_MAX_KEYS        8

struct wifi67_crypto_key {
    u8 key[WIFI67_CRYPTO_MAX_KEY_SIZE];
    u8 key_len;
    u8 key_idx;
    bool valid;
    u32 cipher;
    u8 tx_pn[6];
    u8 rx_pn[6];
};

struct wifi67_crypto_ctx {
    void __iomem *regs;
    struct crypto_aead *tfm_aead;
    struct crypto_skcipher *tfm_cipher;
    struct wifi67_crypto_key keys[WIFI67_CRYPTO_MAX_KEYS];
    spinlock_t lock;
    bool initialized;
};

int wifi67_crypto_init(struct wifi67_priv *priv);
void wifi67_crypto_deinit(struct wifi67_priv *priv);
int wifi67_crypto_set_key(struct wifi67_priv *priv, int key_idx, 
                         const u8 *key, int key_len, u32 cipher);
int wifi67_crypto_encrypt(struct wifi67_priv *priv, struct sk_buff *skb,
                         u8 key_idx, u8 *iv);
int wifi67_crypto_decrypt(struct wifi67_priv *priv, struct sk_buff *skb,
                         u8 key_idx, u8 *iv);

#endif /* _WIFI67_CRYPTO_CORE_H_ */ 