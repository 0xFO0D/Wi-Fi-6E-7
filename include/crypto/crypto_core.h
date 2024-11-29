#ifndef _WIFI67_CRYPTO_CORE_H_
#define _WIFI67_CRYPTO_CORE_H_

#include <linux/types.h>
#include <linux/crypto.h>
#include <crypto/aes.h>
#include <crypto/skcipher.h>
#include <crypto/hash.h>
#include <crypto/aead.h>
#include "../core/wifi67_forward.h"

#define WIFI67_MAX_KEY_SIZE     32
#define WIFI67_MAX_IV_SIZE      16
#define WIFI67_MAX_ICE_SIZE     8
#define WIFI67_NUM_KEYS         4
#define WIFI67_MAX_KEY_ENTRIES  WIFI67_NUM_KEYS

struct wifi67_crypto_key {
    u8 key[WIFI67_MAX_KEY_SIZE];
    u8 key_len;
    u8 key_idx;
    u8 cipher;
    u8 flags;
    u8 hw_key_idx;
    bool valid;
    struct crypto_aead *aead;
    u8 iv[WIFI67_MAX_IV_SIZE];
    atomic_t refcount;
};

struct wifi67_crypto {
    struct wifi67_crypto_key keys[WIFI67_NUM_KEYS];
    
    /* Hardware acceleration */
    bool hw_crypto;
    void __iomem *crypto_regs;
    
    /* Statistics */
    atomic_t tx_encrypted;
    atomic_t tx_failed;
    atomic_t rx_decrypted;
    atomic_t rx_failed;
    
    /* Locks */
    spinlock_t lock;
    
    /* Work items */
    struct work_struct crypto_work;
};

/* Function prototypes */
int wifi67_crypto_init(struct wifi67_priv *priv);
void wifi67_crypto_deinit(struct wifi67_priv *priv);
int wifi67_crypto_set_key(struct wifi67_priv *priv, u8 key_idx,
                         const u8 *key, u32 key_len, u8 cipher);
int wifi67_crypto_encrypt(struct wifi67_priv *priv, struct sk_buff *skb,
                         u8 key_idx);
int wifi67_crypto_decrypt(struct wifi67_priv *priv, struct sk_buff *skb,
                         u8 key_idx);

#endif /* _WIFI67_CRYPTO_CORE_H_ */ 