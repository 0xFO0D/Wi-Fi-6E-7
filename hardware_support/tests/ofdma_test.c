#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/skbuff.h>
#include <linux/completion.h>
#include <linux/list.h>
#include "../include/mac/mac_core.h"
#include "../include/phy/phy_core.h"
#include "test_framework.h"

#define OFDMA_TEST_BUFFER_SIZE 8192
#define OFDMA_TEST_ITERATIONS 1000
#define OFDMA_TEST_TIMEOUT_MS 5000
#define OFDMA_MAX_USERS 8
#define OFDMA_MAX_RUS 9  /* Maximum Resource Units */

/* OFDMA Test Context */
struct ofdma_test_context {
    struct wifi67_mac_dev *mac_dev;
    struct wifi67_phy_dev *phy_dev;
    struct completion setup_done;
    struct completion tx_done[OFDMA_MAX_USERS];
    struct completion rx_done[OFDMA_MAX_USERS];
    atomic_t active_users;
    atomic_t tx_count;
    atomic_t rx_count;
    void *test_buffer;
    struct sk_buff_head tx_queue;
    struct sk_buff_head rx_queue;
    spinlock_t lock;
    u8 mac_addr[OFDMA_MAX_USERS][ETH_ALEN];
    u32 ru_allocations[OFDMA_MAX_USERS];
    bool ofdma_enabled;
    bool dl_ofdma;  /* Downlink OFDMA */
    bool ul_ofdma;  /* Uplink OFDMA */
    struct {
        u64 bytes;
        ktime_t start;
        ktime_t end;
    } throughput;
};

/* Resource Unit sizes */
#define RU_SIZE_26   0
#define RU_SIZE_52   1
#define RU_SIZE_106  2
#define RU_SIZE_242  3
#define RU_SIZE_484  4
#define RU_SIZE_996  5

/* OFDMA direction */
#define OFDMA_DIR_DL 0  /* Downlink */
#define OFDMA_DIR_UL 1  /* Uplink */

/* RU allocation patterns */
struct ru_pattern {
    u32 num_users;
    u32 ru_sizes[OFDMA_MAX_USERS];
};

static const struct ru_pattern ru_patterns[] = {
    {2, {RU_SIZE_484, RU_SIZE_484}},
    {4, {RU_SIZE_242, RU_SIZE_242, RU_SIZE_242, RU_SIZE_242}},
    {8, {RU_SIZE_106, RU_SIZE_106, RU_SIZE_106, RU_SIZE_106,
         RU_SIZE_106, RU_SIZE_106, RU_SIZE_106, RU_SIZE_106}},
};

/* Test callback functions */
static void ofdma_test_setup_callback(void *data)
{
    struct ofdma_test_context *ctx = data;
    complete(&ctx->setup_done);
}

static void ofdma_test_tx_callback(void *data, u32 user_idx)
{
    struct ofdma_test_context *ctx = data;
    if (user_idx < OFDMA_MAX_USERS) {
        atomic_inc(&ctx->tx_count);
        complete(&ctx->tx_done[user_idx]);
    }
}

static void ofdma_test_rx_callback(void *data, u32 user_idx)
{
    struct ofdma_test_context *ctx = data;
    if (user_idx < OFDMA_MAX_USERS) {
        atomic_inc(&ctx->rx_count);
        complete(&ctx->rx_done[user_idx]);
    }
}

/* Helper functions */
static int ofdma_test_set_ru_pattern(struct ofdma_test_context *ctx,
                                    const struct ru_pattern *pattern)
{
    int ret, i;

    for (i = 0; i < pattern->num_users; i++) {
        ctx->ru_allocations[i] = pattern->ru_sizes[i];
        ret = wifi67_mac_set_ru_size(ctx->mac_dev, i, pattern->ru_sizes[i]);
        if (ret)
            return ret;
    }

    atomic_set(&ctx->active_users, pattern->num_users);
    return 0;
}

static void ofdma_test_start_throughput(struct ofdma_test_context *ctx)
{
    ctx->throughput.bytes = 0;
    ctx->throughput.start = ktime_get();
}

static u64 ofdma_test_end_throughput(struct ofdma_test_context *ctx)
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
static struct ofdma_test_context *ofdma_test_init(void)
{
    struct ofdma_test_context *ctx;
    int i;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize completions */
    init_completion(&ctx->setup_done);
    for (i = 0; i < OFDMA_MAX_USERS; i++) {
        init_completion(&ctx->tx_done[i]);
        init_completion(&ctx->rx_done[i]);
    }

    /* Initialize queues */
    skb_queue_head_init(&ctx->tx_queue);
    skb_queue_head_init(&ctx->rx_queue);

    /* Initialize spinlock */
    spin_lock_init(&ctx->lock);

    /* Allocate test buffer */
    ctx->test_buffer = kmalloc(OFDMA_TEST_BUFFER_SIZE, GFP_KERNEL);
    if (!ctx->test_buffer)
        goto err_free_ctx;

    /* Generate random MAC addresses */
    for (i = 0; i < OFDMA_MAX_USERS; i++)
        eth_random_addr(ctx->mac_addr[i]);

    return ctx;

err_free_ctx:
    kfree(ctx);
    return NULL;
}

/* Test cleanup */
static void ofdma_test_cleanup(struct ofdma_test_context *ctx)
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
static int test_ofdma_setup(void *data)
{
    struct ofdma_test_context *ctx = data;
    int ret;

    pr_info("Starting OFDMA setup test\n");

    /* Enable OFDMA */
    ret = wifi67_mac_enable_ofdma(ctx->mac_dev, true);
    if (ret) {
        pr_err("Failed to enable OFDMA: %d\n", ret);
        return TEST_FAIL;
    }

    /* Wait for setup completion */
    if (!wait_for_completion_timeout(&ctx->setup_done,
                                   msecs_to_jiffies(OFDMA_TEST_TIMEOUT_MS))) {
        pr_err("Timeout waiting for OFDMA setup\n");
        return TEST_FAIL;
    }

    ctx->ofdma_enabled = true;
    pr_info("OFDMA setup test passed\n");
    return TEST_PASS;
}

static int test_ofdma_dl(void *data)
{
    struct ofdma_test_context *ctx = data;
    int ret, i, j;
    u64 throughput;

    pr_info("Starting downlink OFDMA test\n");

    if (!ctx->ofdma_enabled) {
        pr_err("OFDMA not enabled\n");
        return TEST_FAIL;
    }

    /* Test each RU pattern */
    for (i = 0; i < ARRAY_SIZE(ru_patterns); i++) {
        ret = ofdma_test_set_ru_pattern(ctx, &ru_patterns[i]);
        if (ret) {
            pr_err("Failed to set RU pattern %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Start throughput measurement */
        ofdma_test_start_throughput(ctx);

        /* Run test iterations */
        for (j = 0; j < OFDMA_TEST_ITERATIONS; j++) {
            /* Generate and transmit test data for each user */
            for (i = 0; i < ru_patterns[i].num_users; i++) {
                get_random_bytes(ctx->test_buffer, OFDMA_TEST_BUFFER_SIZE);
                ctx->throughput.bytes += OFDMA_TEST_BUFFER_SIZE;

                /* Wait for TX completion */
                if (!wait_for_completion_timeout(&ctx->tx_done[i],
                                               msecs_to_jiffies(OFDMA_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for TX completion\n");
                    return TEST_FAIL;
                }
            }
        }

        throughput = ofdma_test_end_throughput(ctx);
        pr_info("DL OFDMA throughput (pattern %d): %llu Mbps\n", i, throughput);
    }

    ctx->dl_ofdma = true;
    pr_info("Downlink OFDMA test passed\n");
    return TEST_PASS;
}

static int test_ofdma_ul(void *data)
{
    struct ofdma_test_context *ctx = data;
    int ret, i, j;
    u64 throughput;

    pr_info("Starting uplink OFDMA test\n");

    if (!ctx->ofdma_enabled) {
        pr_err("OFDMA not enabled\n");
        return TEST_FAIL;
    }

    /* Test each RU pattern */
    for (i = 0; i < ARRAY_SIZE(ru_patterns); i++) {
        ret = ofdma_test_set_ru_pattern(ctx, &ru_patterns[i]);
        if (ret) {
            pr_err("Failed to set RU pattern %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Start throughput measurement */
        ofdma_test_start_throughput(ctx);

        /* Run test iterations */
        for (j = 0; j < OFDMA_TEST_ITERATIONS; j++) {
            /* Wait for RX completion from each user */
            for (i = 0; i < ru_patterns[i].num_users; i++) {
                if (!wait_for_completion_timeout(&ctx->rx_done[i],
                                               msecs_to_jiffies(OFDMA_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for RX completion\n");
                    return TEST_FAIL;
                }
                ctx->throughput.bytes += OFDMA_TEST_BUFFER_SIZE;
            }
        }

        throughput = ofdma_test_end_throughput(ctx);
        pr_info("UL OFDMA throughput (pattern %d): %llu Mbps\n", i, throughput);
    }

    ctx->ul_ofdma = true;
    pr_info("Uplink OFDMA test passed\n");
    return TEST_PASS;
}

static int test_ofdma_stress(void *data)
{
    struct ofdma_test_context *ctx = data;
    int ret, i;
    u32 pattern_idx, direction;

    pr_info("Starting OFDMA stress test\n");

    if (!ctx->ofdma_enabled) {
        pr_err("OFDMA not enabled\n");
        return TEST_FAIL;
    }

    /* Perform rapid RU pattern and direction changes */
    for (i = 0; i < OFDMA_TEST_ITERATIONS; i++) {
        /* Randomly select RU pattern and direction */
        pattern_idx = get_random_u32() % ARRAY_SIZE(ru_patterns);
        direction = get_random_u32() % 2;

        /* Set RU pattern */
        ret = ofdma_test_set_ru_pattern(ctx, &ru_patterns[pattern_idx]);
        if (ret) {
            pr_err("Failed to set RU pattern %d: %d\n", pattern_idx, ret);
            return TEST_FAIL;
        }

        /* Run single iteration in selected direction */
        if (direction == OFDMA_DIR_DL) {
            /* Downlink transmission */
            for (i = 0; i < ru_patterns[pattern_idx].num_users; i++) {
                get_random_bytes(ctx->test_buffer, OFDMA_TEST_BUFFER_SIZE);
                if (!wait_for_completion_timeout(&ctx->tx_done[i],
                                               msecs_to_jiffies(OFDMA_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for TX completion\n");
                    return TEST_FAIL;
                }
            }
        } else {
            /* Uplink reception */
            for (i = 0; i < ru_patterns[pattern_idx].num_users; i++) {
                if (!wait_for_completion_timeout(&ctx->rx_done[i],
                                               msecs_to_jiffies(OFDMA_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for RX completion\n");
                    return TEST_FAIL;
                }
            }
        }
    }

    pr_info("OFDMA stress test passed\n");
    return TEST_PASS;
}

/* Module initialization */
static int __init ofdma_test_module_init(void)
{
    struct ofdma_test_context *ctx;

    pr_info("Initializing OFDMA test module\n");

    /* Initialize test context */
    ctx = ofdma_test_init();
    if (!ctx) {
        pr_err("Failed to initialize test context\n");
        return -ENOMEM;
    }

    /* Register test cases */
    REGISTER_TEST("ofdma_setup", "Test OFDMA setup and initialization",
                 test_ofdma_setup, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("ofdma_dl", "Test downlink OFDMA functionality",
                 test_ofdma_dl, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("ofdma_ul", "Test uplink OFDMA functionality",
                 test_ofdma_ul, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("ofdma_stress", "Stress test OFDMA functionality",
                 test_ofdma_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

/* Module cleanup */
static void __exit ofdma_test_module_exit(void)
{
    struct test_results results;

    /* Get test results */
    get_test_results(&results);
    pr_info("OFDMA tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);

    /* Cleanup is handled by test framework */
    ofdma_test_cleanup(NULL);
}

module_init(ofdma_test_module_init);
module_exit(ofdma_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 OFDMA Test Module");
MODULE_VERSION("1.0"); 