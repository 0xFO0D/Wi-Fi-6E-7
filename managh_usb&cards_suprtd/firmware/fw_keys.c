#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/tpm.h>
#include <linux/key-type.h>
#include <keys/trusted-type.h>
#include <keys/encrypted-type.h>
#include "fw_keys.h"

/* Global key list and lock */
static LIST_HEAD(key_list);
static DEFINE_MUTEX(key_lock);

/* TPM handle for key operations */
static struct tpm_chip *tpm_chip;

/* Key operations for different storage backends */
static const struct key_ops keyring_ops;
static const struct key_ops tpm_ops;

/* Helper function to calculate key fingerprint */
static void calculate_fingerprint(const void *key_data, size_t key_len,
                                u8 fingerprint[32])
{
    struct crypto_shash *tfm;
    struct shash_desc *desc;

    tfm = crypto_alloc_shash("sha256", 0, 0);
    if (IS_ERR(tfm))
        return;

    desc = kmalloc(sizeof(*desc) + crypto_shash_descsize(tfm),
                  GFP_KERNEL);
    if (!desc) {
        crypto_free_shash(tfm);
        return;
    }

    desc->tfm = tfm;
    crypto_shash_init(desc);
    crypto_shash_update(desc, key_data, key_len);
    crypto_shash_final(desc, fingerprint);

    kfree(desc);
    crypto_free_shash(tfm);
}

/* Initialize key management subsystem */
int fw_key_init(void)
{
    int ret;

    /* Initialize TPM if available */
    tpm_chip = tpm_default_chip();
    if (tpm_chip) {
        ret = fw_tpm_init();
        if (ret)
            pr_warn("TPM initialization failed: %d\n", ret);
    }

    return 0;
}

void fw_key_exit(void)
{
    struct key_entry *entry, *tmp;

    /* Free all keys in the list */
    mutex_lock(&key_lock);
    list_for_each_entry_safe(entry, tmp, &key_list, list) {
        list_del(&entry->list);
        kfree(entry->key_data);
        kfree(entry);
    }
    mutex_unlock(&key_lock);

    if (tpm_chip)
        fw_tpm_exit();
}

/* Add a new key to storage */
int fw_key_add(const struct key_entry *entry)
{
    struct key_entry *new_entry;
    int ret = KEY_ERR_NONE;

    if (!entry || !entry->key_data || !entry->key_len)
        return KEY_ERR_INVALID;

    /* Allocate and copy key entry */
    new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
    if (!new_entry)
        return KEY_ERR_STORAGE;

    new_entry->key_data = kmemdup(entry->key_data, entry->key_len,
                                 GFP_KERNEL);
    if (!new_entry->key_data) {
        kfree(new_entry);
        return KEY_ERR_STORAGE;
    }

    /* Copy metadata and calculate fingerprint */
    memcpy(&new_entry->meta, &entry->meta, sizeof(entry->meta));
    new_entry->key_len = entry->key_len;
    calculate_fingerprint(new_entry->key_data, new_entry->key_len,
                         new_entry->meta.fingerprint);

    /* Store in TPM if requested */
    if (entry->meta.flags & KEY_FLAG_TPM_STORED) {
        ret = fw_tpm_store_key(new_entry);
        if (ret) {
            kfree(new_entry->key_data);
            kfree(new_entry);
            return ret;
        }
    }

    /* Add to key list */
    mutex_lock(&key_lock);
    list_add_tail(&new_entry->list, &key_list);
    mutex_unlock(&key_lock);

    return KEY_ERR_NONE;
}

/* Remove a key from storage */
int fw_key_remove(u32 key_id)
{
    struct key_entry *entry;
    int found = 0;

    mutex_lock(&key_lock);
    list_for_each_entry(entry, &key_list, list) {
        if (entry->meta.id == key_id) {
            list_del(&entry->list);
            kfree(entry->key_data);
            kfree(entry);
            found = 1;
            break;
        }
    }
    mutex_unlock(&key_lock);

    return found ? KEY_ERR_NONE : KEY_ERR_INVALID;
}

/* Revoke a key */
int fw_key_revoke(u32 key_id)
{
    struct key_entry *entry;
    int found = 0;

    mutex_lock(&key_lock);
    list_for_each_entry(entry, &key_list, list) {
        if (entry->meta.id == key_id) {
            entry->meta.flags |= KEY_FLAG_REVOKED;
            found = 1;
            break;
        }
    }
    mutex_unlock(&key_lock);

    if (found && (entry->meta.flags & KEY_FLAG_TPM_STORED))
        fw_tpm_verify_key(entry);

    return found ? KEY_ERR_NONE : KEY_ERR_INVALID;
}

/* Get a key by ID */
int fw_key_get(u32 key_id, struct key_entry *entry)
{
    struct key_entry *found_entry;
    int ret = KEY_ERR_INVALID;

    if (!entry)
        return KEY_ERR_INVALID;

    mutex_lock(&key_lock);
    list_for_each_entry(found_entry, &key_list, list) {
        if (found_entry->meta.id == key_id) {
            memcpy(&entry->meta, &found_entry->meta,
                   sizeof(found_entry->meta));
            entry->key_data = kmemdup(found_entry->key_data,
                                    found_entry->key_len,
                                    GFP_KERNEL);
            entry->key_len = found_entry->key_len;
            if (!entry->key_data)
                ret = KEY_ERR_STORAGE;
            else
                ret = KEY_ERR_NONE;
            break;
        }
    }
    mutex_unlock(&key_lock);

    return ret;
}

/* List all keys */
int fw_key_list(struct key_meta *meta_list, size_t *count)
{
    struct key_entry *entry;
    size_t i = 0;

    if (!meta_list || !count || *count == 0)
        return KEY_ERR_INVALID;

    mutex_lock(&key_lock);
    list_for_each_entry(entry, &key_list, list) {
        if (i >= *count)
            break;
        memcpy(&meta_list[i], &entry->meta, sizeof(entry->meta));
        i++;
    }
    *count = i;
    mutex_unlock(&key_lock);

    return KEY_ERR_NONE;
}

/* Rotate a key */
int fw_key_rotate(u32 old_id, const struct key_entry *new_entry)
{
    int ret;

    /* Add new key first */
    ret = fw_key_add(new_entry);
    if (ret)
        return ret;

    /* Revoke old key */
    ret = fw_key_revoke(old_id);
    if (ret) {
        fw_key_remove(new_entry->meta.id);
        return ret;
    }

    return KEY_ERR_NONE;
}

EXPORT_SYMBOL_GPL(fw_key_init);
EXPORT_SYMBOL_GPL(fw_key_exit);
EXPORT_SYMBOL_GPL(fw_key_add);
EXPORT_SYMBOL_GPL(fw_key_remove);
EXPORT_SYMBOL_GPL(fw_key_revoke);
EXPORT_SYMBOL_GPL(fw_key_get);
EXPORT_SYMBOL_GPL(fw_key_list);
EXPORT_SYMBOL_GPL(fw_key_rotate); 