/*
 * WiFi 7 Security Implementation
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#ifndef __WIFI7_SECURITY_H
#define __WIFI7_SECURITY_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <crypto/aes.h>
#include <crypto/gcm.h>
#include <crypto/sha2.h>
#include "../core/wifi7_core.h"

/* Security capabilities */
#define WIFI7_SEC_CAP_WPA3          BIT(0)  /* WPA3 support */
#define WIFI7_SEC_CAP_OWE           BIT(1)  /* Enhanced Open */
#define WIFI7_SEC_CAP_SAE           BIT(2)  /* SAE support */
#define WIFI7_SEC_CAP_PMF           BIT(3)  /* Protected Management Frames */
#define WIFI7_SEC_CAP_MLO           BIT(4)  /* MLO security */
#define WIFI7_SEC_CAP_CCMP_256      BIT(5)  /* CCMP-256 */
#define WIFI7_SEC_CAP_GCMP_256      BIT(6)  /* GCMP-256 */
#define WIFI7_SEC_CAP_BIP_CMAC_256  BIT(7)  /* BIP-CMAC-256 */
#define WIFI7_SEC_CAP_HW_CRYPTO     BIT(8)  /* Hardware crypto */
#define WIFI7_SEC_CAP_KEY_CACHE     BIT(9)  /* Key caching */

/* Security modes */
#define WIFI7_SEC_MODE_NONE         0  /* No security */
#define WIFI7_SEC_MODE_WPA3_PSK     1  /* WPA3-Personal */
#define WIFI7_SEC_MODE_WPA3_ENT     2  /* WPA3-Enterprise */
#define WIFI7_SEC_MODE_OWE          3  /* Enhanced Open */
#define WIFI7_SEC_MODE_SAE          4  /* SAE */

/* Key types */
#define WIFI7_KEY_TYPE_PAIRWISE     0  /* Pairwise key */
#define WIFI7_KEY_TYPE_GROUP        1  /* Group key */
#define WIFI7_KEY_TYPE_IGTK         2  /* IGTK */
#define WIFI7_KEY_TYPE_BIGTK        3  /* BIGTK */

/* Key lengths */
#define WIFI7_KEY_LEN_CCMP_128      16  /* CCMP-128 key length */
#define WIFI7_KEY_LEN_CCMP_256      32  /* CCMP-256 key length */
#define WIFI7_KEY_LEN_GCMP_128      16  /* GCMP-128 key length */
#define WIFI7_KEY_LEN_GCMP_256      32  /* GCMP-256 key length */
#define WIFI7_KEY_LEN_BIP_128       16  /* BIP-128 key length */
#define WIFI7_KEY_LEN_BIP_256       32  /* BIP-256 key length */
#define WIFI7_KEY_LEN_PMK           32  /* PMK length */
#define WIFI7_KEY_LEN_PSK           32  /* PSK length */

/* Maximum values */
#define WIFI7_SEC_MAX_KEYS          32  /* Maximum keys */
#define WIFI7_SEC_MAX_PEERS         64  /* Maximum peers */
#define WIFI7_SEC_MAX_LINKS         16  /* Maximum links */
#define WIFI7_SEC_MAX_REPLAY        64  /* Replay window size */
#define WIFI7_SEC_MAX_KEY_RSC       16  /* Maximum RSC size */

/* Security flags */
#define WIFI7_SEC_FLAG_PMF_REQ      BIT(0)  /* PMF required */
#define WIFI7_SEC_FLAG_PMF_CAP      BIT(1)  /* PMF capable */
#define WIFI7_SEC_FLAG_MLO_REQ      BIT(2)  /* MLO required */
#define WIFI7_SEC_FLAG_MLO_CAP      BIT(3)  /* MLO capable */
#define WIFI7_SEC_FLAG_HW_CRYPTO    BIT(4)  /* Use HW crypto */
#define WIFI7_SEC_FLAG_KEY_CACHE    BIT(5)  /* Use key cache */
#define WIFI7_SEC_FLAG_VALID        BIT(6)  /* Key is valid */
#define WIFI7_SEC_FLAG_ACTIVE       BIT(7)  /* Key is active */

/* Security key */
struct wifi7_sec_key {
    u8 type;                   /* Key type */
    u8 id;                     /* Key ID */
    u8 addr[ETH_ALEN];        /* Peer address */
    u8 key[WIFI7_KEY_LEN_GCMP_256]; /* Key material */
    u8 key_len;               /* Key length */
    u8 cipher;                /* Cipher suite */
    u32 flags;                /* Key flags */
    u64 rsc[WIFI7_SEC_MAX_KEY_RSC]; /* Receive sequence counter */
    u64 tsc;                  /* Transmit sequence counter */
    atomic_t refcount;        /* Reference count */
    spinlock_t lock;          /* Key lock */
};

/* Security peer */
struct wifi7_sec_peer {
    u8 addr[ETH_ALEN];        /* Peer address */
    u8 state;                 /* Security state */
    u32 flags;                /* Peer flags */
    struct wifi7_sec_key *ptk; /* Pairwise key */
    struct wifi7_sec_key *gtk; /* Group key */
    struct wifi7_sec_key *igtk; /* IGTK */
    struct wifi7_sec_key *bigtk; /* BIGTK */
    u64 replay_mask;          /* Replay detection mask */
    spinlock_t lock;          /* Peer lock */
};

/* Security link */
struct wifi7_sec_link {
    u8 link_id;               /* Link identifier */
    u32 flags;                /* Link flags */
    struct wifi7_sec_key *keys[WIFI7_SEC_MAX_KEYS];
    u8 num_keys;              /* Number of keys */
    spinlock_t lock;          /* Link lock */
};

/* Security statistics */
struct wifi7_sec_stats {
    /* Frame counts */
    u32 encrypted_frames;     /* Encrypted frames */
    u32 decrypted_frames;     /* Decrypted frames */
    u32 protected_frames;     /* Protected frames */
    u32 replayed_frames;      /* Replayed frames */
    
    /* Key operations */
    u32 key_installations;    /* Key installations */
    u32 key_removals;         /* Key removals */
    u32 key_updates;          /* Key updates */
    u32 key_failures;         /* Key failures */
    
    /* Crypto operations */
    u32 encrypt_failures;     /* Encryption failures */
    u32 decrypt_failures;     /* Decryption failures */
    u32 mic_failures;         /* MIC failures */
    u32 replay_failures;      /* Replay check failures */
    
    /* MLO operations */
    u32 mlo_key_syncs;       /* MLO key syncs */
    u32 mlo_key_failures;    /* MLO key failures */
    
    /* Hardware crypto */
    u32 hw_encryptions;      /* HW encryptions */
    u32 hw_decryptions;      /* HW decryptions */
    u32 hw_failures;         /* HW failures */
};

/* Security device info */
struct wifi7_security {
    /* Capabilities */
    u32 capabilities;         /* Supported features */
    u32 flags;                /* Enabled features */
    
    /* Configuration */
    u8 mode;                  /* Security mode */
    u8 cipher_group;          /* Group cipher */
    u8 cipher_pairwise;       /* Pairwise cipher */
    u8 cipher_mgmt;           /* Management cipher */
    
    /* Keys */
    struct wifi7_sec_key keys[WIFI7_SEC_MAX_KEYS];
    u8 num_keys;              /* Number of keys */
    spinlock_t key_lock;      /* Key lock */
    
    /* Peers */
    struct wifi7_sec_peer peers[WIFI7_SEC_MAX_PEERS];
    u8 num_peers;             /* Number of peers */
    spinlock_t peer_lock;     /* Peer lock */
    
    /* Links */
    struct wifi7_sec_link links[WIFI7_SEC_MAX_LINKS];
    u8 num_links;             /* Number of links */
    spinlock_t link_lock;     /* Link lock */
    
    /* Crypto */
    struct crypto_aead *tfm_aead;  /* AEAD transform */
    struct crypto_shash *tfm_cmac; /* CMAC transform */
    struct crypto_shash *tfm_sha256; /* SHA-256 transform */
    
    /* Statistics */
    struct wifi7_sec_stats stats;
    spinlock_t stats_lock;    /* Stats lock */
    
    /* Work items */
    struct workqueue_struct *wq;
    struct delayed_work key_work;
    struct delayed_work rekey_work;
    
    /* Debugging */
    struct dentry *debugfs_dir;
    bool debug_enabled;
};

/* Function prototypes */
int wifi7_security_init(struct wifi7_dev *dev);
void wifi7_security_deinit(struct wifi7_dev *dev);

int wifi7_security_start(struct wifi7_dev *dev);
void wifi7_security_stop(struct wifi7_dev *dev);

int wifi7_security_set_key(struct wifi7_dev *dev,
                          struct wifi7_sec_key *key);
int wifi7_security_get_key(struct wifi7_dev *dev,
                          struct wifi7_sec_key *key);
int wifi7_security_del_key(struct wifi7_dev *dev,
                          u8 key_id);

int wifi7_security_add_peer(struct wifi7_dev *dev,
                           const u8 *addr);
int wifi7_security_del_peer(struct wifi7_dev *dev,
                           const u8 *addr);

int wifi7_security_encrypt(struct wifi7_dev *dev,
                          struct sk_buff *skb);
int wifi7_security_decrypt(struct wifi7_dev *dev,
                          struct sk_buff *skb);

int wifi7_security_protect_mgmt(struct wifi7_dev *dev,
                               struct sk_buff *skb);
int wifi7_security_verify_mgmt(struct wifi7_dev *dev,
                              struct sk_buff *skb);

int wifi7_security_get_stats(struct wifi7_dev *dev,
                            struct wifi7_sec_stats *stats);
int wifi7_security_clear_stats(struct wifi7_dev *dev);

#endif /* __WIFI7_SECURITY_H */ 