#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/completion.h>
#include <linux/etherdevice.h>
#include <linux/ieee80211.h>
#include <linux/bitops.h>
#include "../include/mac/mac_core.h"
#include "../include/phy/phy_core.h"
#include "test_framework.h"

#define PP_TEST_BUFFER_SIZE 8192
#define PP_TEST_ITERATIONS 1000
#define PP_TEST_TIMEOUT_MS 5000
#define PP_MAX_PATTERNS 16
#define PP_MAX_SUBCARRIERS 996
#define PP_MAX_PUNCTURE_SIZE 160

/* Preamble puncturing pattern */
struct puncture_pattern {
    u32 num_punctures;
    u32 start_subcarrier[PP_MAX_PUNCTURE_SIZE];
    u32 end_subcarrier[PP_MAX_PUNCTURE_SIZE];
    u32 bandwidth;
    bool dynamic;
};

/* Preamble Puncturing Test Context */
struct pp_test_context {
    struct wifi67_mac_dev *mac_dev;
    struct wifi67_phy_dev *phy_dev;
    struct completion setup_done;
    struct completion scan_done;
    struct completion tx_done;
    struct completion rx_done;
    atomic_t tx_count;
    atomic_t rx_count;
    void *test_buffer;
    struct sk_buff_head tx_queue;
    struct sk_buff_head rx_queue;
    spinlock_t lock;
    struct puncture_pattern current_pattern;
    bool pp_enabled;
    bool dynamic_enabled;
    struct {
        u64 bytes;
        ktime_t start;
        ktime_t end;
    } throughput;
    /* Interference metrics */
    struct {
        u32 rssi[PP_MAX_SUBCARRIERS];
        u32 snr[PP_MAX_SUBCARRIERS];
        u32 interference[PP_MAX_SUBCARRIERS];
        u32 error_count[PP_MAX_SUBCARRIERS];
    } metrics;
    /* Historical data */
    struct {
        u32 pattern_stats[PP_MAX_PATTERNS];
        u32 subcarrier_stats[PP_MAX_SUBCARRIERS];
        u32 dynamic_changes;
    } history;
};

/* Predefined puncturing patterns */
static const struct puncture_pattern puncture_patterns[] = {
    /* Single 20MHz puncture */
    {1,
     {100},
     {119},
     80,
     false},
    /* Two 20MHz punctures */
    {2,
     {100, 300},
     {119, 319},
     160,
     false},
    /* Single 40MHz puncture */
    {1,
     {200},
     {239},
     160,
     false},
    /* Mixed puncture sizes */
    {3,
     {100, 300, 500},
     {119, 339, 519},
     320,
     false},
    /* Dynamic pattern */
    {2,
     {0, 0}, /* Placeholder, will be set dynamically */
     {0, 0},
     160,
     true}
};

/* Test callback functions */
static void pp_test_setup_callback(void *data)
{
    struct pp_test_context *ctx = data;
    complete(&ctx->setup_done);
}

static void pp_test_scan_callback(void *data)
{
    struct pp_test_context *ctx = data;
    complete(&ctx->scan_done);
}

static void pp_test_tx_callback(void *data)
{
    struct pp_test_context *ctx = data;
    atomic_inc(&ctx->tx_count);
    complete(&ctx->tx_done);
}

static void pp_test_rx_callback(void *data)
{
    struct pp_test_context *ctx = data;
    atomic_inc(&ctx->rx_count);
    complete(&ctx->rx_done);
}

/* Helper functions */
static int pp_test_set_pattern(struct pp_test_context *ctx,
                             const struct puncture_pattern *pattern)
{
    int ret, i;

    memcpy(&ctx->current_pattern, pattern, sizeof(*pattern));

    ret = wifi67_mac_set_puncture_pattern(ctx->mac_dev,
                                       pattern->num_punctures,
                                       pattern->start_subcarrier,
                                       pattern->end_subcarrier,
                                       pattern->bandwidth,
                                       pattern->dynamic);
    if (ret)
        return ret;

    /* Update statistics */
    for (i = 0; i < pattern->num_punctures; i++) {
        u32 j;
        for (j = pattern->start_subcarrier[i];
             j <= pattern->end_subcarrier[i]; j++) {
            ctx->history.subcarrier_stats[j]++;
        }
    }

    return 0;
}

static void pp_test_start_throughput(struct pp_test_context *ctx)
{
    ctx->throughput.bytes = 0;
    ctx->throughput.start = ktime_get();
}

static u64 pp_test_end_throughput(struct pp_test_context *ctx)
{
    u64 duration_us;
    u64 throughput_mbps;

    ctx->throughput.end = ktime_get();
    duration_us = ktime_to_us(ktime_sub(ctx->throughput.end,
                                       ctx->throughput.start));
    if (duration_us == 0)
        return 0;

    /* Calculate throughput in Mbps */
    throughput_mbps = (ctx->throughput.bytes * 8ULL) / duration_us;
    return throughput_mbps;
}

static void pp_test_scan_interference(struct pp_test_context *ctx)
{
    /* Scan for interference across all subcarriers */
    wifi67_mac_scan_interference(ctx->mac_dev,
                              ctx->metrics.interference,
                              PP_MAX_SUBCARRIERS);
}

static void pp_test_update_metrics(struct pp_test_context *ctx)
{
    /* Get current metrics */
    wifi67_mac_get_subcarrier_metrics(ctx->mac_dev,
                                   ctx->metrics.rssi,
                                   ctx->metrics.snr,
                                   ctx->metrics.error_count,
                                   PP_MAX_SUBCARRIERS);
}

/* Test initialization */
static struct pp_test_context *pp_test_init(void)
{
    struct pp_test_context *ctx;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize completions */
    init_completion(&ctx->setup_done);
    init_completion(&ctx->scan_done);
    init_completion(&ctx->tx_done);
    init_completion(&ctx->rx_done);

    /* Initialize queues */
    skb_queue_head_init(&ctx->tx_queue);
    skb_queue_head_init(&ctx->rx_queue);

    /* Initialize spinlock */
    spin_lock_init(&ctx->lock);

    /* Allocate test buffer */
    ctx->test_buffer = kmalloc(PP_TEST_BUFFER_SIZE, GFP_KERNEL);
    if (!ctx->test_buffer)
        goto err_free_ctx;

    return ctx;

err_free_ctx:
    kfree(ctx);
    return NULL;
}

/* Test cleanup */
static void pp_test_cleanup(struct pp_test_context *ctx)
{
    struct sk_buff *skb;

    if (!ctx)
        return;

    /* Free queued packets */
    while ((skb = skb_dequeue(&ctx->tx_queue)))
        dev_kfree_skb(skb);
    while ((skb = skb_dequeue(&ctx->rx_queue)))
        dev_kfree_skb(skb);

    kfree(ctx->test_buffer);
    kfree(ctx);
}

/* Test cases */
static int test_pp_setup(void *data)
{
    struct pp_test_context *ctx = data;
    int ret;

    pr_info("Starting Preamble Puncturing setup test\n");

    /* Enable Preamble Puncturing */
    ret = wifi67_mac_enable_puncturing(ctx->mac_dev, true);
    if (ret) {
        pr_err("Failed to enable Preamble Puncturing: %d\n", ret);
        return TEST_FAIL;
    }

    /* Wait for setup completion */
    if (!wait_for_completion_timeout(&ctx->setup_done,
                                   msecs_to_jiffies(PP_TEST_TIMEOUT_MS))) {
        pr_err("Timeout waiting for Preamble Puncturing setup\n");
        return TEST_FAIL;
    }

    ctx->pp_enabled = true;
    pr_info("Preamble Puncturing setup test passed\n");
    return TEST_PASS;
}

static int test_pp_static(void *data)
{
    struct pp_test_context *ctx = data;
    int ret, i, j;
    u64 throughput;

    pr_info("Starting static Preamble Puncturing test\n");

    if (!ctx->pp_enabled) {
        pr_err("Preamble Puncturing not enabled\n");
        return TEST_FAIL;
    }

    /* Test each static pattern */
    for (i = 0; i < ARRAY_SIZE(puncture_patterns); i++) {
        if (puncture_patterns[i].dynamic)
            continue;

        ret = pp_test_set_pattern(ctx, &puncture_patterns[i]);
        if (ret) {
            pr_err("Failed to set puncture pattern %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Start throughput measurement */
        pp_test_start_throughput(ctx);

        /* Run test iterations */
        for (j = 0; j < PP_TEST_ITERATIONS; j++) {
            /* Generate and transmit test data */
            get_random_bytes(ctx->test_buffer, PP_TEST_BUFFER_SIZE);
            ctx->throughput.bytes += PP_TEST_BUFFER_SIZE;

            /* Wait for TX completion */
            if (!wait_for_completion_timeout(&ctx->tx_done,
                                           msecs_to_jiffies(PP_TEST_TIMEOUT_MS))) {
                pr_err("Timeout waiting for TX completion\n");
                return TEST_FAIL;
            }

            /* Wait for RX completion */
            if (!wait_for_completion_timeout(&ctx->rx_done,
                                           msecs_to_jiffies(PP_TEST_TIMEOUT_MS))) {
                pr_err("Timeout waiting for RX completion\n");
                return TEST_FAIL;
            }

            /* Update metrics */
            pp_test_update_metrics(ctx);
        }

        throughput = pp_test_end_throughput(ctx);
        pr_info("Static pattern %d throughput: %llu Mbps\n", i, throughput);

        /* Verify no errors in punctured subcarriers */
        for (j = 0; j < puncture_patterns[i].num_punctures; j++) {
            u32 k;
            for (k = puncture_patterns[i].start_subcarrier[j];
                 k <= puncture_patterns[i].end_subcarrier[j]; k++) {
                if (ctx->metrics.error_count[k] > 0) {
                    pr_err("Errors detected in punctured subcarrier %u\n", k);
                    return TEST_FAIL;
                }
            }
        }

        ctx->history.pattern_stats[i]++;
    }

    pr_info("Static Preamble Puncturing test passed\n");
    return TEST_PASS;
}

static int test_pp_dynamic(void *data)
{
    struct pp_test_context *ctx = data;
    int ret, i;
    u32 interference_threshold = 20; /* dB */
    struct puncture_pattern dynamic_pattern;

    pr_info("Starting dynamic Preamble Puncturing test\n");

    if (!ctx->pp_enabled) {
        pr_err("Preamble Puncturing not enabled\n");
        return TEST_FAIL;
    }

    /* Enable dynamic puncturing */
    ret = wifi67_mac_enable_dynamic_puncturing(ctx->mac_dev, true);
    if (ret) {
        pr_err("Failed to enable dynamic puncturing: %d\n", ret);
        return TEST_FAIL;
    }
    ctx->dynamic_enabled = true;

    /* Run test iterations */
    for (i = 0; i < PP_TEST_ITERATIONS; i++) {
        /* Scan for interference */
        pp_test_scan_interference(ctx);

        /* Wait for scan completion */
        if (!wait_for_completion_timeout(&ctx->scan_done,
                                       msecs_to_jiffies(PP_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for interference scan\n");
            return TEST_FAIL;
        }

        /* Create dynamic pattern based on interference */
        dynamic_pattern.num_punctures = 0;
        dynamic_pattern.bandwidth = 160;
        dynamic_pattern.dynamic = true;

        for (i = 0; i < PP_MAX_SUBCARRIERS; i++) {
            if (ctx->metrics.interference[i] > interference_threshold) {
                u32 idx = dynamic_pattern.num_punctures;
                if (idx >= PP_MAX_PUNCTURE_SIZE)
                    break;

                dynamic_pattern.start_subcarrier[idx] = i;
                while (i < PP_MAX_SUBCARRIERS &&
                       ctx->metrics.interference[i] > interference_threshold)
                    i++;
                dynamic_pattern.end_subcarrier[idx] = i - 1;
                dynamic_pattern.num_punctures++;
            }
        }

        /* Apply dynamic pattern */
        ret = pp_test_set_pattern(ctx, &dynamic_pattern);
        if (ret) {
            pr_err("Failed to set dynamic puncture pattern: %d\n", ret);
            return TEST_FAIL;
        }

        /* Generate and transmit test data */
        get_random_bytes(ctx->test_buffer, PP_TEST_BUFFER_SIZE);

        /* Wait for TX completion */
        if (!wait_for_completion_timeout(&ctx->tx_done,
                                       msecs_to_jiffies(PP_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for TX completion\n");
            return TEST_FAIL;
        }

        /* Wait for RX completion */
        if (!wait_for_completion_timeout(&ctx->rx_done,
                                       msecs_to_jiffies(PP_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for RX completion\n");
            return TEST_FAIL;
        }

        /* Update metrics */
        pp_test_update_metrics(ctx);
        ctx->history.dynamic_changes++;
    }

    pr_info("Dynamic pattern changes: %u\n", ctx->history.dynamic_changes);
    pr_info("Dynamic Preamble Puncturing test passed\n");
    return TEST_PASS;
}

static int test_pp_stress(void *data)
{
    struct pp_test_context *ctx = data;
    int ret, i;
    u32 pattern_idx;

    pr_info("Starting Preamble Puncturing stress test\n");

    if (!ctx->pp_enabled) {
        pr_err("Preamble Puncturing not enabled\n");
        return TEST_FAIL;
    }

    /* Enable dynamic puncturing */
    if (!ctx->dynamic_enabled) {
        ret = wifi67_mac_enable_dynamic_puncturing(ctx->mac_dev, true);
        if (ret) {
            pr_err("Failed to enable dynamic puncturing: %d\n", ret);
            return TEST_FAIL;
        }
        ctx->dynamic_enabled = true;
    }

    /* Perform rapid pattern changes */
    for (i = 0; i < PP_TEST_ITERATIONS; i++) {
        /* Randomly select between static and dynamic patterns */
        if (get_random_u32() % 2) {
            /* Static pattern */
            pattern_idx = get_random_u32() % (ARRAY_SIZE(puncture_patterns) - 1);
            ret = pp_test_set_pattern(ctx, &puncture_patterns[pattern_idx]);
        } else {
            /* Dynamic pattern based on interference scan */
            pp_test_scan_interference(ctx);
            if (!wait_for_completion_timeout(&ctx->scan_done,
                                           msecs_to_jiffies(PP_TEST_TIMEOUT_MS))) {
                pr_err("Timeout waiting for interference scan\n");
                return TEST_FAIL;
            }
        }

        if (ret) {
            pr_err("Failed to set puncture pattern: %d\n", ret);
            return TEST_FAIL;
        }

        /* Generate and transmit test data */
        get_random_bytes(ctx->test_buffer, PP_TEST_BUFFER_SIZE);

        /* Wait for TX completion */
        if (!wait_for_completion_timeout(&ctx->tx_done,
                                       msecs_to_jiffies(PP_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for TX completion\n");
            return TEST_FAIL;
        }

        /* Wait for RX completion */
        if (!wait_for_completion_timeout(&ctx->rx_done,
                                       msecs_to_jiffies(PP_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for RX completion\n");
            return TEST_FAIL;
        }

        /* Update metrics */
        pp_test_update_metrics(ctx);
    }

    pr_info("Preamble Puncturing stress test passed\n");
    return TEST_PASS;
}

/* Module initialization */
static int __init pp_test_module_init(void)
{
    struct pp_test_context *ctx;

    pr_info("Initializing Preamble Puncturing test module\n");

    /* Initialize test context */
    ctx = pp_test_init();
    if (!ctx) {
        pr_err("Failed to initialize test context\n");
        return -ENOMEM;
    }

    /* Register test cases */
    REGISTER_TEST("pp_setup", "Test Preamble Puncturing setup",
                 test_pp_setup, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("pp_static", "Test static Preamble Puncturing patterns",
                 test_pp_static, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("pp_dynamic", "Test dynamic Preamble Puncturing",
                 test_pp_dynamic, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("pp_stress", "Stress test Preamble Puncturing",
                 test_pp_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

/* Module cleanup */
static void __exit pp_test_module_exit(void)
{
    struct test_results results;

    /* Get test results */
    get_test_results(&results);
    pr_info("Preamble Puncturing tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);

    /* Cleanup is handled by test framework */
    pp_test_cleanup(NULL);
}

module_init(pp_test_module_init);
module_exit(pp_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 Preamble Puncturing Test Module");
MODULE_VERSION("1.0"); 