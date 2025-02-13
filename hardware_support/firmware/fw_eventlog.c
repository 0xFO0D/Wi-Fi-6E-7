#include <linux/kernel.h>
#include <linux/tpm.h>
#include <linux/tpm_eventlog.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include "fw_keys.h"
#include "fw_eventlog.h"

/* Event log cache structure */
struct eventlog_cache {
    struct rb_root events;
    struct mutex lock;
    u32 count;
    u64 last_update;
};

/* Event entry in cache */
struct event_entry {
    struct rb_node node;
    u32 pcr_index;
    u32 event_type;
    u8 digest[TPM2_SHA256_DIGEST_SIZE];
    void *data;
    size_t data_len;
    u64 timestamp;
};

static struct eventlog_cache log_cache;

/* Initialize event log handling */
int fw_eventlog_init(void)
{
    mutex_init(&log_cache.lock);
    log_cache.events = RB_ROOT;
    log_cache.count = 0;
    log_cache.last_update = 0;
    return 0;
}

/* Clean up event log resources */
void fw_eventlog_exit(void)
{
    struct rb_node *node;
    struct event_entry *entry;

    mutex_lock(&log_cache.lock);
    while ((node = rb_first(&log_cache.events))) {
        entry = rb_entry(node, struct event_entry, node);
        rb_erase(node, &log_cache.events);
        kfree(entry->data);
        kfree(entry);
    }
    mutex_unlock(&log_cache.lock);
}

/* Add event to cache */
static int cache_event(struct event_entry *event)
{
    struct rb_node **new = &log_cache.events.rb_node;
    struct rb_node *parent = NULL;
    struct event_entry *entry;

    while (*new) {
        parent = *new;
        entry = rb_entry(parent, struct event_entry, node);

        if (event->timestamp < entry->timestamp)
            new = &parent->rb_left;
        else if (event->timestamp > entry->timestamp)
            new = &parent->rb_right;
        else
            return -EEXIST;
    }

    rb_link_node(&event->node, parent, new);
    rb_insert_color(&event->node, &log_cache.events);
    log_cache.count++;
    return 0;
}

/* Parse and validate event data */
static int parse_event(const struct tpm_event_header *header,
                      const u8 *data, size_t len,
                      struct event_entry *entry)
{
    const struct tcg_pcr_event2 *event2;
    const struct tcg_digest *digest;
    size_t size;

    if (len < sizeof(*header))
        return -EINVAL;

    event2 = (const struct tcg_pcr_event2 *)data;
    size = sizeof(*event2) + event2->count * sizeof(*digest);
    if (len < size)
        return -EINVAL;

    entry->pcr_index = header->pcr_index;
    entry->event_type = header->event_type;
    entry->timestamp = ktime_get_real_seconds();

    /* Copy first SHA256 digest */
    digest = (const struct tcg_digest *)(data + sizeof(*event2));
    memcpy(entry->digest, digest->digest, TPM2_SHA256_DIGEST_SIZE);

    /* Copy event data */
    size = event2->event_size;
    entry->data = kmalloc(size, GFP_KERNEL);
    if (!entry->data)
        return -ENOMEM;
    
    memcpy(entry->data, data + size - event2->event_size, size);
    entry->data_len = size;

    return 0;
}

/* Process new events from TPM */
int fw_eventlog_update(void)
{
    struct tpm_chip *chip;
    struct tpm_event_header header;
    u8 *data = NULL;
    size_t len, pos = 0;
    int ret = 0;

    chip = tpm_default_chip();
    if (!chip)
        return -ENODEV;

    /* Read event log */
    ret = tpm_get_event_log(chip, NULL, 0, &len);
    if (ret < 0 || !len)
        return ret;

    data = kmalloc(len, GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    ret = tpm_get_event_log(chip, data, len, &len);
    if (ret < 0)
        goto out;

    mutex_lock(&log_cache.lock);

    /* Process each event */
    while (pos < len) {
        struct event_entry *entry;

        if (pos + sizeof(header) > len) {
            ret = -EINVAL;
            break;
        }

        memcpy(&header, data + pos, sizeof(header));
        
        entry = kzalloc(sizeof(*entry), GFP_KERNEL);
        if (!entry) {
            ret = -ENOMEM;
            break;
        }

        ret = parse_event(&header, data + pos, len - pos, entry);
        if (ret < 0) {
            kfree(entry);
            break;
        }

        ret = cache_event(entry);
        if (ret < 0) {
            kfree(entry->data);
            kfree(entry);
            break;
        }

        pos += sizeof(header) + header.event_size;
    }

    if (!ret)
        log_cache.last_update = ktime_get_real_seconds();

    mutex_unlock(&log_cache.lock);

out:
    kfree(data);
    return ret;
}

/* Validate PCR values against event log */
int fw_eventlog_validate_pcr(u32 pcr_index, const u8 *pcr_value)
{
    struct rb_node *node;
    struct event_entry *entry;
    u8 calculated[TPM2_SHA256_DIGEST_SIZE];
    struct shash_desc *desc;
    struct crypto_shash *tfm;
    int ret;

    if (!pcr_value)
        return -EINVAL;

    /* Initialize hash context */
    tfm = crypto_alloc_shash("sha256", 0, 0);
    if (IS_ERR(tfm))
        return PTR_ERR(tfm);

    desc = kzalloc(sizeof(*desc) + crypto_shash_descsize(tfm),
                   GFP_KERNEL);
    if (!desc) {
        crypto_free_shash(tfm);
        return -ENOMEM;
    }

    desc->tfm = tfm;

    /* Start with zero */
    memset(calculated, 0, sizeof(calculated));

    mutex_lock(&log_cache.lock);

    /* Replay events for PCR */
    for (node = rb_first(&log_cache.events); node; node = rb_next(node)) {
        entry = rb_entry(node, struct event_entry, node);
        
        if (entry->pcr_index != pcr_index)
            continue;

        /* Hash previous value */
        ret = crypto_shash_init(desc);
        if (ret < 0)
            break;

        ret = crypto_shash_update(desc, calculated, sizeof(calculated));
        if (ret < 0)
            break;

        /* Hash event digest */
        ret = crypto_shash_update(desc, entry->digest, sizeof(entry->digest));
        if (ret < 0)
            break;

        ret = crypto_shash_final(desc, calculated);
        if (ret < 0)
            break;
    }

    mutex_unlock(&log_cache.lock);

    crypto_free_shash(tfm);
    kfree(desc);

    if (ret < 0)
        return ret;

    /* Compare calculated value with provided PCR value */
    return memcmp(calculated, pcr_value, TPM2_SHA256_DIGEST_SIZE) ? -EINVAL : 0;
}

/* Get event log statistics */
int fw_eventlog_get_stats(struct eventlog_stats *stats)
{
    if (!stats)
        return -EINVAL;

    mutex_lock(&log_cache.lock);
    stats->event_count = log_cache.count;
    stats->last_update = log_cache.last_update;
    mutex_unlock(&log_cache.lock);

    return 0;
}

/* Export event log entries */
int fw_eventlog_export(struct event_export *export,
                      u32 start_idx, u32 count)
{
    struct rb_node *node;
    struct event_entry *entry;
    u32 idx = 0;
    int ret = 0;

    if (!export || !export->events || !count)
        return -EINVAL;

    mutex_lock(&log_cache.lock);

    if (start_idx >= log_cache.count) {
        ret = -EINVAL;
        goto out;
    }

    /* Find starting entry */
    for (node = rb_first(&log_cache.events);
         node && idx < start_idx;
         node = rb_next(node), idx++)
        ;

    /* Export requested entries */
    for (idx = 0; node && idx < count; node = rb_next(node), idx++) {
        entry = rb_entry(node, struct event_entry, node);
        
        export->events[idx].pcr_index = entry->pcr_index;
        export->events[idx].event_type = entry->event_type;
        memcpy(export->events[idx].digest, entry->digest,
               sizeof(entry->digest));
        export->events[idx].timestamp = entry->timestamp;

        if (entry->data && entry->data_len <= MAX_EVENT_DATA_SIZE) {
            export->events[idx].data_len = entry->data_len;
            memcpy(export->events[idx].data, entry->data,
                   entry->data_len);
        } else {
            export->events[idx].data_len = 0;
        }
    }

    export->count = idx;

out:
    mutex_unlock(&log_cache.lock);
    return ret;
}

EXPORT_SYMBOL_GPL(fw_eventlog_init);
EXPORT_SYMBOL_GPL(fw_eventlog_exit);
EXPORT_SYMBOL_GPL(fw_eventlog_update);
EXPORT_SYMBOL_GPL(fw_eventlog_validate_pcr);
EXPORT_SYMBOL_GPL(fw_eventlog_get_stats);
EXPORT_SYMBOL_GPL(fw_eventlog_export); 