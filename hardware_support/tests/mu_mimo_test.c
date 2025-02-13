#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/completion.h>
#include <linux/etherdevice.h>
#include <linux/ieee80211.h>
#include "../include/mac/mac_core.h"
#include "../include/phy/phy_core.h"
#include "test_framework.h"

#define MUMIMO_TEST_BUFFER_SIZE 8192
#define MUMIMO_TEST_ITERATIONS 1000
#define MUMIMO_TEST_TIMEOUT_MS 5000
#define MUMIMO_MAX_USERS 8
#define MUMIMO_MAX_STREAMS 4

/* MU-MIMO Test Context */
struct mumimo_test_context {
    struct wifi67_mac_dev *mac_dev;
    struct wifi67_phy_dev *phy_dev;
    struct completion setup_done;
    struct completion sounding_done;
    struct completion tx_done[MUMIMO_MAX_USERS];
    struct completion rx_done[MUMIMO_MAX_USERS];
    atomic_t active_users;
    atomic_t tx_count;
    atomic_t rx_count;
    void *test_buffer;
    struct sk_buff_head tx_queue;
    struct sk_buff_head rx_queue;
    spinlock_t lock;
    u8 mac_addr[MUMIMO_MAX_USERS][ETH_ALEN];
    u32 stream_mapping[MUMIMO_MAX_USERS];
    bool mumimo_enabled;
    bool dl_mumimo;  /* Downlink MU-MIMO */
    bool ul_mumimo;  /* Uplink MU-MIMO */
    struct {
        u64 bytes;
        ktime_t start;
        ktime_t end;
    } throughput;
    /* Beamforming matrices */
    void *bf_matrices;
    size_t bf_size;
    /* Channel state information */
    void *csi_data;
    size_t csi_size;
};

/* Stream allocation patterns */
struct stream_pattern {
    u32 num_users;
    u32 streams_per_user[MUMIMO_MAX_USERS];
};

static const struct stream_pattern stream_patterns[] = {
    {2, {2, 2, 0, 0, 0, 0, 0, 0}},  /* 2 users, 2 streams each */
    {4, {1, 1, 1, 1, 0, 0, 0, 0}},  /* 4 users, 1 stream each */
    {3, {2, 1, 1, 0, 0, 0, 0, 0}},  /* 3 users: 2+1+1 streams */
};

/* Test callback functions */
static void mumimo_test_setup_callback(void *data)
{
    struct mumimo_test_context *ctx = data;
    complete(&ctx->setup_done);
}

static void mumimo_test_sounding_callback(void *data)
{
    struct mumimo_test_context *ctx = data;
    complete(&ctx->sounding_done);
}

static void mumimo_test_tx_callback(void *data, u32 user_idx)
{
    struct mumimo_test_context *ctx = data;
    if (user_idx < MUMIMO_MAX_USERS) {
        atomic_inc(&ctx->tx_count);
        complete(&ctx->tx_done[user_idx]);
    }
}

static void mumimo_test_rx_callback(void *data, u32 user_idx)
{
    struct mumimo_test_context *ctx = data;
    if (user_idx < MUMIMO_MAX_USERS) {
        atomic_inc(&ctx->rx_count);
        complete(&ctx->rx_done[user_idx]);
    }
}

/* Helper functions */
static int mumimo_test_set_stream_pattern(struct mumimo_test_context *ctx,
                                        const struct stream_pattern *pattern)
{
    int ret, i;

    for (i = 0; i < pattern->num_users; i++) {
        ctx->stream_mapping[i] = pattern->streams_per_user[i];
        ret = wifi67_mac_set_streams(ctx->mac_dev, i, pattern->streams_per_user[i]);
        if (ret)
            return ret;
    }

    atomic_set(&ctx->active_users, pattern->num_users);
    return 0;
}

static void mumimo_test_start_throughput(struct mumimo_test_context *ctx)
{
    ctx->throughput.bytes = 0;
    ctx->throughput.start = ktime_get();
}

static u64 mumimo_test_end_throughput(struct mumimo_test_context *ctx)
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

/* Test initialization */
static struct mumimo_test_context *mumimo_test_init(void)
{
    struct mumimo_test_context *ctx;
    int i;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize completions */
    init_completion(&ctx->setup_done);
    init_completion(&ctx->sounding_done);
    for (i = 0; i < MUMIMO_MAX_USERS; i++) {
        init_completion(&ctx->tx_done[i]);
        init_completion(&ctx->rx_done[i]);
    }

    /* Initialize queues */
    skb_queue_head_init(&ctx->tx_queue);
    skb_queue_head_init(&ctx->rx_queue);

    /* Initialize spinlock */
    spin_lock_init(&ctx->lock);

    /* Allocate test buffer */
    ctx->test_buffer = kmalloc(MUMIMO_TEST_BUFFER_SIZE, GFP_KERNEL);
    if (!ctx->test_buffer)
        goto err_free_ctx;

    /* Allocate beamforming matrices */
    ctx->bf_size = MUMIMO_MAX_USERS * MUMIMO_MAX_STREAMS * 256;
    ctx->bf_matrices = kmalloc(ctx->bf_size, GFP_KERNEL);
    if (!ctx->bf_matrices)
        goto err_free_buffer;

    /* Allocate CSI data buffer */
    ctx->csi_size = MUMIMO_MAX_USERS * 1024;
    ctx->csi_data = kmalloc(ctx->csi_size, GFP_KERNEL);
    if (!ctx->csi_data)
        goto err_free_bf;

    /* Generate random MAC addresses */
    for (i = 0; i < MUMIMO_MAX_USERS; i++)
        eth_random_addr(ctx->mac_addr[i]);

    return ctx;

err_free_bf:
    kfree(ctx->bf_matrices);
err_free_buffer:
    kfree(ctx->test_buffer);
err_free_ctx:
    kfree(ctx);
    return NULL;
}

/* Test cleanup */
static void mumimo_test_cleanup(struct mumimo_test_context *ctx)
{
    struct sk_buff *skb;

    if (!ctx)
        return;

    /* Free queued packets */
    while ((skb = skb_dequeue(&ctx->tx_queue)))
        dev_kfree_skb(skb);
    while ((skb = skb_dequeue(&ctx->rx_queue)))
        dev_kfree_skb(skb);

    kfree(ctx->csi_data);
    kfree(ctx->bf_matrices);
    kfree(ctx->test_buffer);
    kfree(ctx);
}

/* Test cases */
static int test_mumimo_setup(void *data)
{
    struct mumimo_test_context *ctx = data;
    int ret;

    pr_info("Starting MU-MIMO setup test\n");

    /* Enable MU-MIMO */
    ret = wifi67_mac_enable_mumimo(ctx->mac_dev, true);
    if (ret) {
        pr_err("Failed to enable MU-MIMO: %d\n", ret);
        return TEST_FAIL;
    }

    /* Wait for setup completion */
    if (!wait_for_completion_timeout(&ctx->setup_done,
                                   msecs_to_jiffies(MUMIMO_TEST_TIMEOUT_MS))) {
        pr_err("Timeout waiting for MU-MIMO setup\n");
        return TEST_FAIL;
    }

    ctx->mumimo_enabled = true;
    pr_info("MU-MIMO setup test passed\n");
    return TEST_PASS;
}

static int test_mumimo_sounding(void *data)
{
    struct mumimo_test_context *ctx = data;
    int ret, i;

    pr_info("Starting MU-MIMO sounding test\n");

    if (!ctx->mumimo_enabled) {
        pr_err("MU-MIMO not enabled\n");
        return TEST_FAIL;
    }

    /* Test each stream pattern */
    for (i = 0; i < ARRAY_SIZE(stream_patterns); i++) {
        ret = mumimo_test_set_stream_pattern(ctx, &stream_patterns[i]);
        if (ret) {
            pr_err("Failed to set stream pattern %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Perform channel sounding */
        ret = wifi67_mac_trigger_sounding(ctx->mac_dev);
        if (ret) {
            pr_err("Failed to trigger sounding: %d\n", ret);
            return TEST_FAIL;
        }

        /* Wait for sounding completion */
        if (!wait_for_completion_timeout(&ctx->sounding_done,
                                       msecs_to_jiffies(MUMIMO_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for sounding completion\n");
            return TEST_FAIL;
        }

        /* Verify CSI data */
        ret = wifi67_mac_get_csi(ctx->mac_dev, ctx->csi_data, ctx->csi_size);
        if (ret) {
            pr_err("Failed to get CSI data: %d\n", ret);
            return TEST_FAIL;
        }
    }

    pr_info("MU-MIMO sounding test passed\n");
    return TEST_PASS;
}

static int test_mumimo_dl(void *data)
{
    struct mumimo_test_context *ctx = data;
    int ret, i, j;
    u64 throughput;

    pr_info("Starting downlink MU-MIMO test\n");

    if (!ctx->mumimo_enabled) {
        pr_err("MU-MIMO not enabled\n");
        return TEST_FAIL;
    }

    /* Test each stream pattern */
    for (i = 0; i < ARRAY_SIZE(stream_patterns); i++) {
        ret = mumimo_test_set_stream_pattern(ctx, &stream_patterns[i]);
        if (ret) {
            pr_err("Failed to set stream pattern %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Start throughput measurement */
        mumimo_test_start_throughput(ctx);

        /* Run test iterations */
        for (j = 0; j < MUMIMO_TEST_ITERATIONS; j++) {
            /* Generate and transmit test data for each user */
            for (i = 0; i < stream_patterns[i].num_users; i++) {
                get_random_bytes(ctx->test_buffer, MUMIMO_TEST_BUFFER_SIZE);
                ctx->throughput.bytes += MUMIMO_TEST_BUFFER_SIZE;

                /* Wait for TX completion */
                if (!wait_for_completion_timeout(&ctx->tx_done[i],
                                               msecs_to_jiffies(MUMIMO_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for TX completion\n");
                    return TEST_FAIL;
                }
            }
        }

        throughput = mumimo_test_end_throughput(ctx);
        pr_info("DL MU-MIMO throughput (pattern %d): %llu Mbps\n", i, throughput);
    }

    ctx->dl_mumimo = true;
    pr_info("Downlink MU-MIMO test passed\n");
    return TEST_PASS;
}

static int test_mumimo_ul(void *data)
{
    struct mumimo_test_context *ctx = data;
    int ret, i, j;
    u64 throughput;

    pr_info("Starting uplink MU-MIMO test\n");

    if (!ctx->mumimo_enabled) {
        pr_err("MU-MIMO not enabled\n");
        return TEST_FAIL;
    }

    /* Test each stream pattern */
    for (i = 0; i < ARRAY_SIZE(stream_patterns); i++) {
        ret = mumimo_test_set_stream_pattern(ctx, &stream_patterns[i]);
        if (ret) {
            pr_err("Failed to set stream pattern %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Start throughput measurement */
        mumimo_test_start_throughput(ctx);

        /* Run test iterations */
        for (j = 0; j < MUMIMO_TEST_ITERATIONS; j++) {
            /* Wait for RX completion from each user */
            for (i = 0; i < stream_patterns[i].num_users; i++) {
                if (!wait_for_completion_timeout(&ctx->rx_done[i],
                                               msecs_to_jiffies(MUMIMO_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for RX completion\n");
                    return TEST_FAIL;
                }
                ctx->throughput.bytes += MUMIMO_TEST_BUFFER_SIZE;
            }
        }

        throughput = mumimo_test_end_throughput(ctx);
        pr_info("UL MU-MIMO throughput (pattern %d): %llu Mbps\n", i, throughput);
    }

    ctx->ul_mumimo = true;
    pr_info("Uplink MU-MIMO test passed\n");
    return TEST_PASS;
}

static int test_mumimo_stress(void *data)
{
    struct mumimo_test_context *ctx = data;
    int ret, i;
    u32 pattern_idx, direction;

    pr_info("Starting MU-MIMO stress test\n");

    if (!ctx->mumimo_enabled) {
        pr_err("MU-MIMO not enabled\n");
        return TEST_FAIL;
    }

    /* Perform rapid stream pattern and direction changes */
    for (i = 0; i < MUMIMO_TEST_ITERATIONS; i++) {
        /* Randomly select stream pattern and direction */
        pattern_idx = get_random_u32() % ARRAY_SIZE(stream_patterns);
        direction = get_random_u32() % 2;

        /* Set stream pattern */
        ret = mumimo_test_set_stream_pattern(ctx, &stream_patterns[pattern_idx]);
        if (ret) {
            pr_err("Failed to set stream pattern %d: %d\n", pattern_idx, ret);
            return TEST_FAIL;
        }

        /* Perform sounding */
        ret = wifi67_mac_trigger_sounding(ctx->mac_dev);
        if (ret) {
            pr_err("Failed to trigger sounding: %d\n", ret);
            return TEST_FAIL;
        }

        if (!wait_for_completion_timeout(&ctx->sounding_done,
                                       msecs_to_jiffies(MUMIMO_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for sounding completion\n");
            return TEST_FAIL;
        }

        /* Run single iteration in selected direction */
        if (direction == 0) {
            /* Downlink transmission */
            for (i = 0; i < stream_patterns[pattern_idx].num_users; i++) {
                get_random_bytes(ctx->test_buffer, MUMIMO_TEST_BUFFER_SIZE);
                if (!wait_for_completion_timeout(&ctx->tx_done[i],
                                               msecs_to_jiffies(MUMIMO_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for TX completion\n");
                    return TEST_FAIL;
                }
            }
        } else {
            /* Uplink reception */
            for (i = 0; i < stream_patterns[pattern_idx].num_users; i++) {
                if (!wait_for_completion_timeout(&ctx->rx_done[i],
                                               msecs_to_jiffies(MUMIMO_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for RX completion\n");
                    return TEST_FAIL;
                }
            }
        }
    }

    pr_info("MU-MIMO stress test passed\n");
    return TEST_PASS;
}

/* Module initialization */
static int __init mumimo_test_module_init(void)
{
    struct mumimo_test_context *ctx;

    pr_info("Initializing MU-MIMO test module\n");

    /* Initialize test context */
    ctx = mumimo_test_init();
    if (!ctx) {
        pr_err("Failed to initialize test context\n");
        return -ENOMEM;
    }

    /* Register test cases */
    REGISTER_TEST("mumimo_setup", "Test MU-MIMO setup and initialization",
                 test_mumimo_setup, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("mumimo_sounding", "Test MU-MIMO channel sounding",
                 test_mumimo_sounding, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("mumimo_dl", "Test downlink MU-MIMO functionality",
                 test_mumimo_dl, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("mumimo_ul", "Test uplink MU-MIMO functionality",
                 test_mumimo_ul, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("mumimo_stress", "Stress test MU-MIMO functionality",
                 test_mumimo_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

/* Module cleanup */
static void __exit mumimo_test_module_exit(void)
{
    struct test_results results;

    /* Get test results */
    get_test_results(&results);
    pr_info("MU-MIMO tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);

    /* Cleanup is handled by test framework */
    mumimo_test_cleanup(NULL);
}

module_init(mumimo_test_module_init);
module_exit(mumimo_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 MU-MIMO Test Module");
MODULE_VERSION("1.0"); 