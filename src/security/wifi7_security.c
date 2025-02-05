/*
 * WiFi 7 Security Implementation
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/random.h>
#include <crypto/aes.h>
#include <crypto/gcm.h>
#include <crypto/sha2.h>
#include <net/mac80211.h>
#include "wifi7_security.h"
#include "../core/wifi7_core.h"

/* Helper Functions */

static bool wifi7_security_is_valid_key(struct wifi7_sec_key *key)
{
    if (!key || !key->key_len)
        return false;
        
    switch (key->type) {
    case WIFI7_KEY_TYPE_PAIRWISE:
    case WIFI7_KEY_TYPE_GROUP:
        if (key->key_len != WIFI7_KEY_LEN_CCMP_128 &&
            key->key_len != WIFI7_KEY_LEN_CCMP_256 &&
            key->key_len != WIFI7_KEY_LEN_GCMP_128 &&
            key->key_len != WIFI7_KEY_LEN_GCMP_256)
            return false;
        break;
        
    case WIFI7_KEY_TYPE_IGTK:
    case WIFI7_KEY_TYPE_BIGTK:
        if (key->key_len != WIFI7_KEY_LEN_BIP_128 &&
            key->key_len != WIFI7_KEY_LEN_BIP_256)
            return false;
        break;
        
    default:
        return false;
    }
    
    return true;
}

static bool wifi7_security_is_valid_peer(struct wifi7_sec_peer *peer)
{
    if (!peer)
        return false;
        
    if (is_zero_ether_addr(peer->addr))
        return false;
        
    if (is_multicast_ether_addr(peer->addr))
        return false;
        
    return true;
}

static void wifi7_security_update_stats(struct wifi7_security *sec,
                                      enum wifi7_sec_stat_type type)
{
    unsigned long flags;
    
    spin_lock_irqsave(&sec->stats_lock, flags);
    
    switch (type) {
    case WIFI7_STAT_ENCRYPTED:
        sec->stats.encrypted_frames++;
        break;
    case WIFI7_STAT_DECRYPTED:
        sec->stats.decrypted_frames++;
        break;
    case WIFI7_STAT_PROTECTED:
        sec->stats.protected_frames++;
        break;
    case WIFI7_STAT_REPLAYED:
        sec->stats.replayed_frames++;
        break;
    case WIFI7_STAT_KEY_INSTALL:
        sec->stats.key_installations++;
        break;
    case WIFI7_STAT_KEY_REMOVE:
        sec->stats.key_removals++;
        break;
    case WIFI7_STAT_KEY_UPDATE:
        sec->stats.key_updates++;
        break;
    case WIFI7_STAT_KEY_FAIL:
        sec->stats.key_failures++;
        break;
    case WIFI7_STAT_ENCRYPT_FAIL:
        sec->stats.encrypt_failures++;
        break;
    case WIFI7_STAT_DECRYPT_FAIL:
        sec->stats.decrypt_failures++;
        break;
    case WIFI7_STAT_MIC_FAIL:
        sec->stats.mic_failures++;
        break;
    case WIFI7_STAT_REPLAY_FAIL:
        sec->stats.replay_failures++;
        break;
    case WIFI7_STAT_MLO_SYNC:
        sec->stats.mlo_key_syncs++;
        break;
    case WIFI7_STAT_MLO_FAIL:
        sec->stats.mlo_key_failures++;
        break;
    case WIFI7_STAT_HW_ENCRYPT:
        sec->stats.hw_encryptions++;
        break;
    case WIFI7_STAT_HW_DECRYPT:
        sec->stats.hw_decryptions++;
        break;
    case WIFI7_STAT_HW_FAIL:
        sec->stats.hw_failures++;
        break;
    }
    
    spin_unlock_irqrestore(&sec->stats_lock, flags);
}

/* Crypto Operations */

static int wifi7_security_init_crypto(struct wifi7_security *sec)
{
    sec->tfm_aead = crypto_alloc_aead("gcm(aes)", 0, CRYPTO_ALG_ASYNC);
    if (IS_ERR(sec->tfm_aead)) {
        pr_err("Failed to allocate AEAD transform\n");
        return PTR_ERR(sec->tfm_aead);
    }
    
    sec->tfm_cmac = crypto_alloc_shash("cmac(aes)", 0, 0);
    if (IS_ERR(sec->tfm_cmac)) {
        pr_err("Failed to allocate CMAC transform\n");
        crypto_free_aead(sec->tfm_aead);
        return PTR_ERR(sec->tfm_cmac);
    }
    
    sec->tfm_sha256 = crypto_alloc_shash("sha256", 0, 0);
    if (IS_ERR(sec->tfm_sha256)) {
        pr_err("Failed to allocate SHA-256 transform\n");
        crypto_free_aead(sec->tfm_aead);
        crypto_free_shash(sec->tfm_cmac);
        return PTR_ERR(sec->tfm_sha256);
    }
    
    return 0;
}

static void wifi7_security_free_crypto(struct wifi7_security *sec)
{
    crypto_free_aead(sec->tfm_aead);
    crypto_free_shash(sec->tfm_cmac);
    crypto_free_shash(sec->tfm_sha256);
}

static int wifi7_security_encrypt_frame(struct wifi7_security *sec,
                                      struct sk_buff *skb,
                                      struct wifi7_sec_key *key)
{
    struct scatterlist sg[2];
    u8 *iv, *mic;
    int ret;
    
    if (sec->flags & WIFI7_SEC_FLAG_HW_CRYPTO) {
        ret = wifi7_hw_encrypt_frame(sec->dev, skb, key);
        if (!ret) {
            wifi7_security_update_stats(sec, WIFI7_STAT_HW_ENCRYPT);
            return 0;
        }
        wifi7_security_update_stats(sec, WIFI7_STAT_HW_FAIL);
    }
    
    iv = skb_push(skb, IEEE80211_GCMP_IV_LEN);
    get_random_bytes(iv, IEEE80211_GCMP_IV_LEN);
    
    mic = skb_put(skb, IEEE80211_GCMP_MIC_LEN);
    
    sg_init_table(sg, 2);
    sg_set_buf(&sg[0], iv, IEEE80211_GCMP_IV_LEN);
    sg_set_buf(&sg[1], skb->data + IEEE80211_GCMP_IV_LEN,
               skb->len - IEEE80211_GCMP_IV_LEN - IEEE80211_GCMP_MIC_LEN);
    
    crypto_aead_setkey(sec->tfm_aead, key->key, key->key_len);
    crypto_aead_setauthsize(sec->tfm_aead, IEEE80211_GCMP_MIC_LEN);
    
    aead_request_set_ad(sec->tfm_aead, skb->data, IEEE80211_GCMP_HDR_LEN);
    
    ret = crypto_aead_encrypt(sec->tfm_aead);
    if (ret) {
        wifi7_security_update_stats(sec, WIFI7_STAT_ENCRYPT_FAIL);
        return ret;
    }
    
    wifi7_security_update_stats(sec, WIFI7_STAT_ENCRYPTED);
    return 0;
}

static int wifi7_security_decrypt_frame(struct wifi7_security *sec,
                                      struct sk_buff *skb,
                                      struct wifi7_sec_key *key)
{
    struct scatterlist sg[2];
    u8 *iv, *mic;
    int ret;
    
    if (sec->flags & WIFI7_SEC_FLAG_HW_CRYPTO) {
        ret = wifi7_hw_decrypt_frame(sec->dev, skb, key);
        if (!ret) {
            wifi7_security_update_stats(sec, WIFI7_STAT_HW_DECRYPT);
            return 0;
        }
        wifi7_security_update_stats(sec, WIFI7_STAT_HW_FAIL);
    }
    
    if (skb->len < (IEEE80211_GCMP_IV_LEN + IEEE80211_GCMP_MIC_LEN)) {
        wifi7_security_update_stats(sec, WIFI7_STAT_DECRYPT_FAIL);
        return -EINVAL;
    }
    
    iv = skb->data;
    mic = skb->data + skb->len - IEEE80211_GCMP_MIC_LEN;
    
    sg_init_table(sg, 2);
    sg_set_buf(&sg[0], iv, IEEE80211_GCMP_IV_LEN);
    sg_set_buf(&sg[1], skb->data + IEEE80211_GCMP_IV_LEN,
               skb->len - IEEE80211_GCMP_IV_LEN - IEEE80211_GCMP_MIC_LEN);
    
    crypto_aead_setkey(sec->tfm_aead, key->key, key->key_len);
    crypto_aead_setauthsize(sec->tfm_aead, IEEE80211_GCMP_MIC_LEN);
    
    aead_request_set_ad(sec->tfm_aead, skb->data, IEEE80211_GCMP_HDR_LEN);
    
    ret = crypto_aead_decrypt(sec->tfm_aead);
    if (ret) {
        wifi7_security_update_stats(sec, WIFI7_STAT_DECRYPT_FAIL);
        return ret;
    }
    
    skb_pull(skb, IEEE80211_GCMP_IV_LEN);
    skb_trim(skb, skb->len - IEEE80211_GCMP_MIC_LEN);
    
    wifi7_security_update_stats(sec, WIFI7_STAT_DECRYPTED);
    return 0;
}

/* Key Management */

static int wifi7_security_install_key(struct wifi7_security *sec,
                                    struct wifi7_sec_key *key)
{
    unsigned long flags;
    int ret = 0;
    
    if (!wifi7_security_is_valid_key(key)) {
        wifi7_security_update_stats(sec, WIFI7_STAT_KEY_FAIL);
        return -EINVAL;
    }
    
    spin_lock_irqsave(&sec->key_lock, flags);
    
    if (sec->num_keys >= WIFI7_SEC_MAX_KEYS) {
        ret = -ENOSPC;
        goto out;
    }
    
    memcpy(&sec->keys[sec->num_keys], key, sizeof(*key));
    atomic_set(&sec->keys[sec->num_keys].refcount, 1);
    spin_lock_init(&sec->keys[sec->num_keys].lock);
    
    sec->num_keys++;
    
    wifi7_security_update_stats(sec, WIFI7_STAT_KEY_INSTALL);
    
out:
    spin_unlock_irqrestore(&sec->key_lock, flags);
    return ret;
}

static int wifi7_security_remove_key(struct wifi7_security *sec,
                                   u8 key_id)
{
    unsigned long flags;
    int i, ret = -ENOENT;
    
    spin_lock_irqsave(&sec->key_lock, flags);
    
    for (i = 0; i < sec->num_keys; i++) {
        if (sec->keys[i].id == key_id) {
            if (atomic_read(&sec->keys[i].refcount) > 1) {
                ret = -EBUSY;
                goto out;
            }
            
            if (i < sec->num_keys - 1)
                memmove(&sec->keys[i], &sec->keys[i + 1],
                       sizeof(struct wifi7_sec_key) * (sec->num_keys - i - 1));
            
            sec->num_keys--;
            
            wifi7_security_update_stats(sec, WIFI7_STAT_KEY_REMOVE);
            ret = 0;
            break;
        }
    }
    
out:
    spin_unlock_irqrestore(&sec->key_lock, flags);
    return ret;
}

/* Peer Management */

static int wifi7_security_add_peer_internal(struct wifi7_security *sec,
                                          const u8 *addr)
{
    struct wifi7_sec_peer *peer;
    unsigned long flags;
    int ret = 0;
    
    spin_lock_irqsave(&sec->peer_lock, flags);
    
    if (sec->num_peers >= WIFI7_SEC_MAX_PEERS) {
        ret = -ENOSPC;
        goto out;
    }
    
    peer = &sec->peers[sec->num_peers];
    memcpy(peer->addr, addr, ETH_ALEN);
    peer->state = 0;
    peer->flags = 0;
    peer->ptk = NULL;
    peer->gtk = NULL;
    peer->igtk = NULL;
    peer->bigtk = NULL;
    peer->replay_mask = 0;
    spin_lock_init(&peer->lock);
    
    sec->num_peers++;
    
out:
    spin_unlock_irqrestore(&sec->peer_lock, flags);
    return ret;
}

static int wifi7_security_remove_peer_internal(struct wifi7_security *sec,
                                             const u8 *addr)
{
    unsigned long flags;
    int i, ret = -ENOENT;
    
    spin_lock_irqsave(&sec->peer_lock, flags);
    
    for (i = 0; i < sec->num_peers; i++) {
        if (ether_addr_equal(sec->peers[i].addr, addr)) {
            if (i < sec->num_peers - 1)
                memmove(&sec->peers[i], &sec->peers[i + 1],
                       sizeof(struct wifi7_sec_peer) * (sec->num_peers - i - 1));
            
            sec->num_peers--;
            ret = 0;
            break;
        }
    }
    
    spin_unlock_irqrestore(&sec->peer_lock, flags);
    return ret;
}

/* Work Handlers */

static void wifi7_security_key_work_handler(struct work_struct *work)
{
    struct wifi7_security *sec = container_of(work, struct wifi7_security,
                                            key_work.work);
    unsigned long flags;
    int i;
    
    spin_lock_irqsave(&sec->key_lock, flags);
    
    for (i = 0; i < sec->num_keys; i++) {
        struct wifi7_sec_key *key = &sec->keys[i];
        
        if (!(key->flags & WIFI7_SEC_FLAG_VALID))
            continue;
            
        if (key->flags & WIFI7_SEC_FLAG_ACTIVE) {
            /* Check for key expiration */
            if (time_after(jiffies, key->expiry)) {
                key->flags &= ~WIFI7_SEC_FLAG_ACTIVE;
                wifi7_security_update_stats(sec, WIFI7_STAT_KEY_UPDATE);
            }
        }
    }
    
    spin_unlock_irqrestore(&sec->key_lock, flags);
    
    schedule_delayed_work(&sec->key_work, HZ);
}

static void wifi7_security_rekey_work_handler(struct work_struct *work)
{
    struct wifi7_security *sec = container_of(work, struct wifi7_security,
                                            rekey_work.work);
    /* Implement rekey logic here */
    schedule_delayed_work(&sec->rekey_work, sec->rekey_interval);
}

/* Public API Implementation */

int wifi7_security_init(struct wifi7_dev *dev)
{
    struct wifi7_security *sec;
    int ret;
    
    sec = kzalloc(sizeof(*sec), GFP_KERNEL);
    if (!sec)
        return -ENOMEM;
        
    dev->security = sec;
    sec->dev = dev;
    
    /* Initialize locks */
    spin_lock_init(&sec->key_lock);
    spin_lock_init(&sec->peer_lock);
    spin_lock_init(&sec->link_lock);
    spin_lock_init(&sec->stats_lock);
    
    /* Initialize crypto */
    ret = wifi7_security_init_crypto(sec);
    if (ret)
        goto err_free;
        
    /* Initialize work queue */
    sec->wq = create_singlethread_workqueue("wifi7_security");
    if (!sec->wq) {
        ret = -ENOMEM;
        goto err_crypto;
    }
    
    INIT_DELAYED_WORK(&sec->key_work, wifi7_security_key_work_handler);
    INIT_DELAYED_WORK(&sec->rekey_work, wifi7_security_rekey_work_handler);
    
    /* Initialize debugfs */
    if (dev->debugfs_dir) {
        sec->debugfs_dir = debugfs_create_dir("security", dev->debugfs_dir);
        if (sec->debugfs_dir) {
            debugfs_create_bool("debug_enabled", 0600, sec->debugfs_dir,
                              &sec->debug_enabled);
        }
    }
    
    return 0;
    
err_crypto:
    wifi7_security_free_crypto(sec);
err_free:
    kfree(sec);
    return ret;
}

void wifi7_security_deinit(struct wifi7_dev *dev)
{
    struct wifi7_security *sec = dev->security;
    
    if (!sec)
        return;
        
    cancel_delayed_work_sync(&sec->key_work);
    cancel_delayed_work_sync(&sec->rekey_work);
    destroy_workqueue(sec->wq);
    
    debugfs_remove_recursive(sec->debugfs_dir);
    
    wifi7_security_free_crypto(sec);
    
    kfree(sec);
    dev->security = NULL;
}

int wifi7_security_start(struct wifi7_dev *dev)
{
    struct wifi7_security *sec = dev->security;
    
    if (!sec)
        return -EINVAL;
        
    schedule_delayed_work(&sec->key_work, HZ);
    schedule_delayed_work(&sec->rekey_work, sec->rekey_interval);
    
    return 0;
}

void wifi7_security_stop(struct wifi7_dev *dev)
{
    struct wifi7_security *sec = dev->security;
    
    if (!sec)
        return;
        
    cancel_delayed_work_sync(&sec->key_work);
    cancel_delayed_work_sync(&sec->rekey_work);
}

int wifi7_security_set_key(struct wifi7_dev *dev,
                          struct wifi7_sec_key *key)
{
    struct wifi7_security *sec = dev->security;
    
    if (!sec || !key)
        return -EINVAL;
        
    return wifi7_security_install_key(sec, key);
}

int wifi7_security_get_key(struct wifi7_dev *dev,
                          struct wifi7_sec_key *key)
{
    struct wifi7_security *sec = dev->security;
    unsigned long flags;
    int i;
    
    if (!sec || !key)
        return -EINVAL;
        
    spin_lock_irqsave(&sec->key_lock, flags);
    
    for (i = 0; i < sec->num_keys; i++) {
        if (sec->keys[i].id == key->id) {
            memcpy(key, &sec->keys[i], sizeof(*key));
            spin_unlock_irqrestore(&sec->key_lock, flags);
            return 0;
        }
    }
    
    spin_unlock_irqrestore(&sec->key_lock, flags);
    return -ENOENT;
}

int wifi7_security_del_key(struct wifi7_dev *dev,
                          u8 key_id)
{
    struct wifi7_security *sec = dev->security;
    
    if (!sec)
        return -EINVAL;
        
    return wifi7_security_remove_key(sec, key_id);
}

int wifi7_security_add_peer(struct wifi7_dev *dev,
                           const u8 *addr)
{
    struct wifi7_security *sec = dev->security;
    
    if (!sec || !addr)
        return -EINVAL;
        
    return wifi7_security_add_peer_internal(sec, addr);
}

int wifi7_security_del_peer(struct wifi7_dev *dev,
                           const u8 *addr)
{
    struct wifi7_security *sec = dev->security;
    
    if (!sec || !addr)
        return -EINVAL;
        
    return wifi7_security_remove_peer_internal(sec, addr);
}

int wifi7_security_encrypt(struct wifi7_dev *dev,
                          struct sk_buff *skb)
{
    struct wifi7_security *sec = dev->security;
    struct wifi7_sec_key *key;
    struct ethhdr *eth;
    unsigned long flags;
    int i, ret = -ENOENT;
    
    if (!sec || !skb)
        return -EINVAL;
        
    eth = (struct ethhdr *)skb->data;
    
    spin_lock_irqsave(&sec->key_lock, flags);
    
    for (i = 0; i < sec->num_keys; i++) {
        key = &sec->keys[i];
        
        if (!(key->flags & WIFI7_SEC_FLAG_VALID) ||
            !(key->flags & WIFI7_SEC_FLAG_ACTIVE))
            continue;
            
        if (is_multicast_ether_addr(eth->h_dest)) {
            if (key->type != WIFI7_KEY_TYPE_GROUP)
                continue;
        } else {
            if (key->type != WIFI7_KEY_TYPE_PAIRWISE)
                continue;
                
            if (!ether_addr_equal(key->addr, eth->h_dest))
                continue;
        }
        
        ret = wifi7_security_encrypt_frame(sec, skb, key);
        break;
    }
    
    spin_unlock_irqrestore(&sec->key_lock, flags);
    return ret;
}

int wifi7_security_decrypt(struct wifi7_dev *dev,
                          struct sk_buff *skb)
{
    struct wifi7_security *sec = dev->security;
    struct wifi7_sec_key *key;
    struct ethhdr *eth;
    unsigned long flags;
    int i, ret = -ENOENT;
    
    if (!sec || !skb)
        return -EINVAL;
        
    eth = (struct ethhdr *)skb->data;
    
    spin_lock_irqsave(&sec->key_lock, flags);
    
    for (i = 0; i < sec->num_keys; i++) {
        key = &sec->keys[i];
        
        if (!(key->flags & WIFI7_SEC_FLAG_VALID) ||
            !(key->flags & WIFI7_SEC_FLAG_ACTIVE))
            continue;
            
        if (is_multicast_ether_addr(eth->h_dest)) {
            if (key->type != WIFI7_KEY_TYPE_GROUP)
                continue;
        } else {
            if (key->type != WIFI7_KEY_TYPE_PAIRWISE)
                continue;
                
            if (!ether_addr_equal(key->addr, eth->h_source))
                continue;
        }
        
        ret = wifi7_security_decrypt_frame(sec, skb, key);
        break;
    }
    
    spin_unlock_irqrestore(&sec->key_lock, flags);
    return ret;
}

int wifi7_security_protect_mgmt(struct wifi7_dev *dev,
                               struct sk_buff *skb)
{
    struct wifi7_security *sec = dev->security;
    struct wifi7_sec_key *key;
    unsigned long flags;
    int i, ret = -ENOENT;
    
    if (!sec || !skb)
        return -EINVAL;
        
    if (!(sec->flags & WIFI7_SEC_FLAG_PMF_REQ))
        return 0;
        
    spin_lock_irqsave(&sec->key_lock, flags);
    
    for (i = 0; i < sec->num_keys; i++) {
        key = &sec->keys[i];
        
        if (!(key->flags & WIFI7_SEC_FLAG_VALID) ||
            !(key->flags & WIFI7_SEC_FLAG_ACTIVE))
            continue;
            
        if (key->type != WIFI7_KEY_TYPE_IGTK)
            continue;
            
        ret = wifi7_security_encrypt_frame(sec, skb, key);
        if (!ret)
            wifi7_security_update_stats(sec, WIFI7_STAT_PROTECTED);
        break;
    }
    
    spin_unlock_irqrestore(&sec->key_lock, flags);
    return ret;
}

int wifi7_security_verify_mgmt(struct wifi7_dev *dev,
                              struct sk_buff *skb)
{
    struct wifi7_security *sec = dev->security;
    struct wifi7_sec_key *key;
    unsigned long flags;
    int i, ret = -ENOENT;
    
    if (!sec || !skb)
        return -EINVAL;
        
    if (!(sec->flags & WIFI7_SEC_FLAG_PMF_REQ))
        return 0;
        
    spin_lock_irqsave(&sec->key_lock, flags);
    
    for (i = 0; i < sec->num_keys; i++) {
        key = &sec->keys[i];
        
        if (!(key->flags & WIFI7_SEC_FLAG_VALID) ||
            !(key->flags & WIFI7_SEC_FLAG_ACTIVE))
            continue;
            
        if (key->type != WIFI7_KEY_TYPE_IGTK)
            continue;
            
        ret = wifi7_security_decrypt_frame(sec, skb, key);
        break;
    }
    
    spin_unlock_irqrestore(&sec->key_lock, flags);
    return ret;
}

int wifi7_security_get_stats(struct wifi7_dev *dev,
                            struct wifi7_sec_stats *stats)
{
    struct wifi7_security *sec = dev->security;
    unsigned long flags;
    
    if (!sec || !stats)
        return -EINVAL;
        
    spin_lock_irqsave(&sec->stats_lock, flags);
    memcpy(stats, &sec->stats, sizeof(*stats));
    spin_unlock_irqrestore(&sec->stats_lock, flags);
    
    return 0;
}

int wifi7_security_clear_stats(struct wifi7_dev *dev)
{
    struct wifi7_security *sec = dev->security;
    unsigned long flags;
    
    if (!sec)
        return -EINVAL;
        
    spin_lock_irqsave(&sec->stats_lock, flags);
    memset(&sec->stats, 0, sizeof(sec->stats));
    spin_unlock_irqrestore(&sec->stats_lock, flags);
    
    return 0;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Security Implementation");
MODULE_VERSION("1.0"); 