#ifndef _WIFI67_CRYPTO_TYPES_H_
#define _WIFI67_CRYPTO_TYPES_H_

#include <linux/types.h>
#include <linux/crypto.h>
#include <crypto/aes.h>
#include <crypto/skcipher.h>
#include <crypto/hash.h>
#include <crypto/aead.h>

#define WIFI67_MAX_KEY_SIZE    32
#define WIFI67_MAX_IV_SIZE     16
#define WIFI67_MAX_ICE_SIZE    8
#define WIFI67_NUM_KEYS        4

struct wifi67_crypto_key {
    u8 key[WIFI67_MAX_KEY_SIZE];
    u8 key_len;
    u8 key_idx;
    u8 cipher;
    u8 flags;
    u8 hw_key_idx;
    atomic_t refcount;
};

struct wifi67_crypto_queue {
    struct crypto_wait wait;
    struct sk_buff_head queue;
    spinlock_t lock;
    atomic_t pending;
};

struct wifi67_crypto_tfm {
    struct crypto_skcipher *tfm;
    struct crypto_aead *aead;
    struct crypto_shash *shash;
    u8 key[WIFI67_MAX_KEY_SIZE];
    u8 key_len;
};

struct wifi67_crypto_ctx {
    struct wifi67_crypto_tfm enc;
    struct wifi67_crypto_tfm dec;
    u8 iv[WIFI67_MAX_IV_SIZE];
    u8 iv_len;
    atomic_t refcount;
};

struct wifi67_crypto {
    struct wifi67_crypto_key keys[WIFI67_NUM_KEYS];
    struct wifi67_crypto_ctx ctx[WIFI67_NUM_KEYS];
    struct wifi67_crypto_queue tx_queue;
    struct wifi67_crypto_queue rx_queue;
    
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

#endif /* _WIFI67_CRYPTO_TYPES_H_ */ 