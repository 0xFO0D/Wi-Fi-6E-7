#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include "fw_eventlog.h"

/* Test PCR values */
static const u8 test_pcr_values[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
};

/* Test event log update */
static int test_eventlog_update(void)
{
    int ret;

    pr_info("Testing event log update...\n");

    ret = fw_eventlog_update();
    if (ret < 0) {
        pr_err("Event log update failed: %d\n", ret);
        return ret;
    }

    pr_info("Event log update test passed!\n");
    return 0;
}

/* Test PCR validation */
static int test_pcr_validation(void)
{
    int ret;

    pr_info("Testing PCR validation...\n");

    ret = fw_eventlog_validate_pcr(8, test_pcr_values);
    if (ret < 0) {
        pr_err("PCR validation failed: %d\n", ret);
        return ret;
    }

    pr_info("PCR validation test passed!\n");
    return 0;
}

/* Test event log export */
static int test_eventlog_export(void)
{
    struct event_export export;
    struct event_entry_export *events;
    int ret;

    pr_info("Testing event log export...\n");

    events = kzalloc(sizeof(*events) * 10, GFP_KERNEL);
    if (!events)
        return -ENOMEM;

    export.events = events;
    export.count = 0;

    ret = fw_eventlog_export(&export, 0, 10);
    if (ret < 0) {
        pr_err("Event log export failed: %d\n", ret);
        goto out;
    }

    if (export.count > 0)
        pr_info("Successfully exported %u events\n", export.count);

    ret = 0;

out:
    kfree(events);
    return ret;
}

/* Test event log statistics */
static int test_eventlog_stats(void)
{
    struct eventlog_stats stats;
    int ret;

    pr_info("Testing event log statistics...\n");

    ret = fw_eventlog_get_stats(&stats);
    if (ret < 0) {
        pr_err("Failed to get event log stats: %d\n", ret);
        return ret;
    }

    pr_info("Event count: %u\n", stats.event_count);
    pr_info("Last update: %llu\n", stats.last_update);

    return 0;
}

static int __init test_fw_eventlog_init(void)
{
    int ret;

    pr_info("Starting firmware event log tests!\n");

    ret = test_eventlog_update();
    if (ret)
        return ret;

    ret = test_pcr_validation();
    if (ret)
        return ret;

    ret = test_eventlog_export();
    if (ret)
        return ret;

    ret = test_eventlog_stats();
    if (ret)
        return ret;

    pr_info("All firmware event log tests passed!\n");
    return 0;
}

static void __exit test_fw_eventlog_exit(void)
{
    pr_info("Firmware event log tests cleanup complete!\n");
}

module_init(test_fw_eventlog_init);
module_exit(test_fw_eventlog_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("WiFi 6E/7 Driver Team");
MODULE_DESCRIPTION("Firmware Event Log Tests");
MODULE_VERSION("1.0"); 