#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "fw_keys.h"
#include "fw_eventlog.h"
#include "fw_attest.h"

static struct dentry *wifi67_dir;
static struct dentry *eventlog_dir;
static struct dentry *attest_dir;

/* Event log status show function */
static int eventlog_status_show(struct seq_file *m, void *v)
{
    struct eventlog_stats stats;
    int ret;

    ret = fw_eventlog_get_stats(&stats);
    if (ret < 0)
        return ret;

    seq_printf(m, "Event Count: %u\n", stats.event_count);
    seq_printf(m, "Last Update: %llu\n", stats.last_update);
    return 0;
}

/* Event log entries show function */
static int eventlog_entries_show(struct seq_file *m, void *v)
{
    struct event_export export;
    struct event_entry_export *events;
    int ret, i;

    events = kzalloc(sizeof(*events) * 32, GFP_KERNEL);
    if (!events)
        return -ENOMEM;

    export.events = events;
    export.count = 0;

    ret = fw_eventlog_export(&export, 0, 32);
    if (ret < 0)
        goto out;

    for (i = 0; i < export.count; i++) {
        seq_printf(m, "Event %d:\n", i);
        seq_printf(m, "  PCR: %u\n", events[i].pcr_index);
        seq_printf(m, "  Type: 0x%x\n", events[i].event_type);
        seq_printf(m, "  Time: %llu\n", events[i].timestamp);
        seq_printf(m, "  Data: %.*s\n", events[i].data_len, events[i].data);
        seq_puts(m, "\n");
    }

    ret = 0;
out:
    kfree(events);
    return ret;
}

/* Attestation status show function */
static int attest_status_show(struct seq_file *m, void *v)
{
    struct attest_challenge challenge;
    const u8 test_id[16] = {0};
    int ret;

    ret = fw_attest_challenge(test_id, &challenge);
    if (ret < 0)
        return ret;

    seq_puts(m, "Attestation Service Status:\n");
    seq_printf(m, "Time: %llu\n", challenge.timestamp);
    seq_puts(m, "State: Active\n");
    return 0;
}

DEFINE_SHOW_ATTRIBUTE(eventlog_status);
DEFINE_SHOW_ATTRIBUTE(eventlog_entries);
DEFINE_SHOW_ATTRIBUTE(attest_status);

/* Initialize debugfs interface */
int fw_debugfs_init(void)
{
    wifi67_dir = debugfs_create_dir("wifi67", NULL);
    if (IS_ERR(wifi67_dir))
        return PTR_ERR(wifi67_dir);

    eventlog_dir = debugfs_create_dir("eventlog", wifi67_dir);
    if (IS_ERR(eventlog_dir))
        goto err_eventlog;

    attest_dir = debugfs_create_dir("attest", wifi67_dir);
    if (IS_ERR(attest_dir))
        goto err_attest;

    if (!debugfs_create_file("status", 0444, eventlog_dir,
                            NULL, &eventlog_status_fops))
        goto err_files;

    if (!debugfs_create_file("entries", 0444, eventlog_dir,
                            NULL, &eventlog_entries_fops))
        goto err_files;

    if (!debugfs_create_file("status", 0444, attest_dir,
                            NULL, &attest_status_fops))
        goto err_files;

    return 0;

err_files:
    debugfs_remove_recursive(attest_dir);
err_attest:
    debugfs_remove_recursive(eventlog_dir);
err_eventlog:
    debugfs_remove_recursive(wifi67_dir);
    return -ENODEV;
}

/* Cleanup debugfs interface */
void fw_debugfs_exit(void)
{
    debugfs_remove_recursive(wifi67_dir);
}

EXPORT_SYMBOL_GPL(fw_debugfs_init);
EXPORT_SYMBOL_GPL(fw_debugfs_exit); 