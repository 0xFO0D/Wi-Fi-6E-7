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

#define RU_TEST_BUFFER_SIZE 8192
#define RU_TEST_ITERATIONS 1000
#define RU_TEST_TIMEOUT_MS 5000
#define RU_MAX_USERS 8
#define RU_MAX_PATTERNS 16

/* RU sizes in tones */
enum ru_size {
    RU_26_TONES = 26,
    RU_52_TONES = 52,
    RU_106_TONES = 106,
    RU_242_TONES = 242,
    RU_484_TONES = 484,
    RU_996_TONES = 996,
    RU_2x996_TONES = 1992
};

/* RU allocation pattern */
struct ru_pattern {
    u32 num_users;
    enum ru_size ru_sizes[RU_MAX_USERS];
    u32 ru_indices[RU_MAX_USERS];
    bool punctured[RU_MAX_USERS];
};

/* Multi-RU Test Context */
struct multi_ru_test_context {
    struct wifi67_mac_dev *mac_dev;
    struct wifi67_phy_dev *phy_dev;
    struct completion setup_done;
    struct completion alloc_done;
    struct completion tx_done[RU_MAX_USERS];
    struct completion rx_done[RU_MAX_USERS];
    atomic_t active_users;
    atomic_t tx_count;
    atomic_t rx_count;
    void *test_buffer;
    struct sk_buff_head tx_queue;
    struct sk_buff_head rx_queue;
    spinlock_t lock;
    u8 mac_addr[RU_MAX_USERS][ETH_ALEN];
    struct {
        u32 num_users;
        enum ru_size ru_sizes[RU_MAX_USERS];
        u32 ru_indices[RU_MAX_USERS];
        bool punctured[RU_MAX_USERS];
    } allocation;
    bool ru_enabled;
    bool dl_ru_enabled;
    bool ul_ru_enabled;
    struct {
        u64 bytes;
        ktime_t start;
        ktime_t end;
    } throughput;
    /* Channel state information */
    void *csi_data;
    size_t csi_size;
};

/* Predefined RU allocation patterns */
static const struct ru_pattern ru_patterns[] = {
    /* 2 users with equal RU sizes */
    {2,
     {RU_484_TONES, RU_484_TONES},
     {0, 1},
     {false, false}},
    /* 4 users with mixed RU sizes */
    {4,
     {RU_242_TONES, RU_242_TONES, RU_242_TONES, RU_242_TONES},
     {0, 1, 2, 3},
     {false, false, false, false}},
    /* 8 users with small RUs */
    {8,
     {RU_106_TONES, RU_106_TONES, RU_106_TONES, RU_106_TONES,
      RU_106_TONES, RU_106_TONES, RU_106_TONES, RU_106_TONES},
     {0, 1, 2, 3, 4, 5, 6, 7},
     {false, false, false, false, false, false, false, false}},
    /* Mixed RU sizes with puncturing */
    {4,
     {RU_484_TONES, RU_242_TONES, RU_242_TONES, RU_242_TONES},
     {0, 1, 2, 3},
     {false, true, false, false}}
};

/* Test callback functions */
static void ru_test_setup_callback(void *data)
{
    struct multi_ru_test_context *ctx = data;
    complete(&ctx->setup_done);
}

static void ru_test_alloc_callback(void *data)
{
    struct multi_ru_test_context *ctx = data;
    complete(&ctx->alloc_done);
}

static void ru_test_tx_callback(void *data, u32 user_idx)
{
    struct multi_ru_test_context *ctx = data;
    if (user_idx < RU_MAX_USERS) {
        atomic_inc(&ctx->tx_count);
        complete(&ctx->tx_done[user_idx]);
    }
}

static void ru_test_rx_callback(void *data, u32 user_idx)
{
    struct multi_ru_test_context *ctx = data;
    if (user_idx < RU_MAX_USERS) {
        atomic_inc(&ctx->rx_count);
        complete(&ctx->rx_done[user_idx]);
    }
}

/* Helper functions */
static int ru_test_set_allocation(struct multi_ru_test_context *ctx,
                                const struct ru_pattern *pattern)
{
    int ret, i;

    ctx->allocation.num_users = pattern->num_users;
    for (i = 0; i < pattern->num_users; i++) {
        ctx->allocation.ru_sizes[i] = pattern->ru_sizes[i];
        ctx->allocation.ru_indices[i] = pattern->ru_indices[i];
        ctx->allocation.punctured[i] = pattern->punctured[i];

        ret = wifi67_mac_set_ru_allocation(ctx->mac_dev, i,
                                         pattern->ru_sizes[i],
                                         pattern->ru_indices[i],
                                         pattern->punctured[i]);
        if (ret)
            return ret;
    }

    atomic_set(&ctx->active_users, pattern->num_users);
    return 0;
}

static void ru_test_start_throughput(struct multi_ru_test_context *ctx)
{
    ctx->throughput.bytes = 0;
    ctx->throughput.start = ktime_get();
}

static u64 ru_test_end_throughput(struct multi_ru_test_context *ctx)
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
static struct multi_ru_test_context *ru_test_init(void)
{
    struct multi_ru_test_context *ctx;
    int i;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize completions */
    init_completion(&ctx->setup_done);
    init_completion(&ctx->alloc_done);
    for (i = 0; i < RU_MAX_USERS; i++) {
        init_completion(&ctx->tx_done[i]);
        init_completion(&ctx->rx_done[i]);
    }

    /* Initialize queues */
    skb_queue_head_init(&ctx->tx_queue);
    skb_queue_head_init(&ctx->rx_queue);

    /* Initialize spinlock */
    spin_lock_init(&ctx->lock);

    /* Allocate test buffer */
    ctx->test_buffer = kmalloc(RU_TEST_BUFFER_SIZE, GFP_KERNEL);
    if (!ctx->test_buffer)
        goto err_free_ctx;

    /* Allocate CSI data buffer */
    ctx->csi_size = RU_MAX_USERS * 1024;
    ctx->csi_data = kmalloc(ctx->csi_size, GFP_KERNEL);
    if (!ctx->csi_data)
        goto err_free_buffer;

    /* Generate random MAC addresses */
    for (i = 0; i < RU_MAX_USERS; i++)
        eth_random_addr(ctx->mac_addr[i]);

    return ctx;

err_free_buffer:
    kfree(ctx->test_buffer);
err_free_ctx:
    kfree(ctx);
    return NULL;
}

/* Test cleanup */
static void ru_test_cleanup(struct multi_ru_test_context *ctx)
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
    kfree(ctx->test_buffer);
    kfree(ctx);
}

/* Test cases */
static int test_ru_setup(void *data)
{
    struct multi_ru_test_context *ctx = data;
    int ret;

    pr_info("Starting Multi-RU setup test\n");

    /* Enable Multi-RU */
    ret = wifi67_mac_enable_multi_ru(ctx->mac_dev, true);
    if (ret) {
        pr_err("Failed to enable Multi-RU: %d\n", ret);
        return TEST_FAIL;
    }

    /* Wait for setup completion */
    if (!wait_for_completion_timeout(&ctx->setup_done,
                                   msecs_to_jiffies(RU_TEST_TIMEOUT_MS))) {
        pr_err("Timeout waiting for Multi-RU setup\n");
        return TEST_FAIL;
    }

    ctx->ru_enabled = true;
    pr_info("Multi-RU setup test passed\n");
    return TEST_PASS;
}

static int test_ru_allocation(void *data)
{
    struct multi_ru_test_context *ctx = data;
    int ret, i;

    pr_info("Starting Multi-RU allocation test\n");

    if (!ctx->ru_enabled) {
        pr_err("Multi-RU not enabled\n");
        return TEST_FAIL;
    }

    /* Test each RU pattern */
    for (i = 0; i < ARRAY_SIZE(ru_patterns); i++) {
        ret = ru_test_set_allocation(ctx, &ru_patterns[i]);
        if (ret) {
            pr_err("Failed to set RU allocation pattern %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Wait for allocation completion */
        if (!wait_for_completion_timeout(&ctx->alloc_done,
                                       msecs_to_jiffies(RU_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for RU allocation\n");
            return TEST_FAIL;
        }

        /* Verify CSI data */
        ret = wifi67_mac_get_csi(ctx->mac_dev, ctx->csi_data, ctx->csi_size);
        if (ret) {
            pr_err("Failed to get CSI data: %d\n", ret);
            return TEST_FAIL;
        }
    }

    pr_info("Multi-RU allocation test passed\n");
    return TEST_PASS;
}

static int test_ru_dl(void *data)
{
    struct multi_ru_test_context *ctx = data;
    int ret, i, j;
    u64 throughput;

    pr_info("Starting downlink Multi-RU test\n");

    if (!ctx->ru_enabled) {
        pr_err("Multi-RU not enabled\n");
        return TEST_FAIL;
    }

    /* Test each RU pattern */
    for (i = 0; i < ARRAY_SIZE(ru_patterns); i++) {
        ret = ru_test_set_allocation(ctx, &ru_patterns[i]);
        if (ret) {
            pr_err("Failed to set RU allocation pattern %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Start throughput measurement */
        ru_test_start_throughput(ctx);

        /* Run test iterations */
        for (j = 0; j < RU_TEST_ITERATIONS; j++) {
            /* Generate and transmit test data for each user */
            for (i = 0; i < ru_patterns[i].num_users; i++) {
                if (ru_patterns[i].punctured[i])
                    continue;

                get_random_bytes(ctx->test_buffer, RU_TEST_BUFFER_SIZE);
                ctx->throughput.bytes += RU_TEST_BUFFER_SIZE;

                /* Wait for TX completion */
                if (!wait_for_completion_timeout(&ctx->tx_done[i],
                                               msecs_to_jiffies(RU_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for TX completion on user %d\n", i);
                    return TEST_FAIL;
                }
            }
        }

        throughput = ru_test_end_throughput(ctx);
        pr_info("DL Multi-RU throughput (pattern %d): %llu Mbps\n", i, throughput);
    }

    ctx->dl_ru_enabled = true;
    pr_info("Downlink Multi-RU test passed\n");
    return TEST_PASS;
}

static int test_ru_ul(void *data)
{
    struct multi_ru_test_context *ctx = data;
    int ret, i, j;
    u64 throughput;

    pr_info("Starting uplink Multi-RU test\n");

    if (!ctx->ru_enabled) {
        pr_err("Multi-RU not enabled\n");
        return TEST_FAIL;
    }

    /* Test each RU pattern */
    for (i = 0; i < ARRAY_SIZE(ru_patterns); i++) {
        ret = ru_test_set_allocation(ctx, &ru_patterns[i]);
        if (ret) {
            pr_err("Failed to set RU allocation pattern %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Start throughput measurement */
        ru_test_start_throughput(ctx);

        /* Run test iterations */
        for (j = 0; j < RU_TEST_ITERATIONS; j++) {
            /* Wait for RX completion from each user */
            for (i = 0; i < ru_patterns[i].num_users; i++) {
                if (ru_patterns[i].punctured[i])
                    continue;

                if (!wait_for_completion_timeout(&ctx->rx_done[i],
                                               msecs_to_jiffies(RU_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for RX completion on user %d\n", i);
                    return TEST_FAIL;
                }
                ctx->throughput.bytes += RU_TEST_BUFFER_SIZE;
            }
        }

        throughput = ru_test_end_throughput(ctx);
        pr_info("UL Multi-RU throughput (pattern %d): %llu Mbps\n", i, throughput);
    }

    ctx->ul_ru_enabled = true;
    pr_info("Uplink Multi-RU test passed\n");
    return TEST_PASS;
}

static int test_ru_stress(void *data)
{
    struct multi_ru_test_context *ctx = data;
    int ret, i;
    u32 pattern_idx, op_type;

    pr_info("Starting Multi-RU stress test\n");

    if (!ctx->ru_enabled) {
        pr_err("Multi-RU not enabled\n");
        return TEST_FAIL;
    }

    /* Perform rapid RU allocation and operation changes */
    for (i = 0; i < RU_TEST_ITERATIONS; i++) {
        /* Randomly select RU pattern and operation */
        pattern_idx = get_random_u32() % ARRAY_SIZE(ru_patterns);
        op_type = get_random_u32() % 2;

        /* Set RU allocation */
        ret = ru_test_set_allocation(ctx, &ru_patterns[pattern_idx]);
        if (ret) {
            pr_err("Failed to set RU allocation pattern %d: %d\n", pattern_idx, ret);
            return TEST_FAIL;
        }

        /* Wait for allocation completion */
        if (!wait_for_completion_timeout(&ctx->alloc_done,
                                       msecs_to_jiffies(RU_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for RU allocation\n");
            return TEST_FAIL;
        }

        /* Run single iteration of selected operation */
        if (op_type == 0) {
            /* Downlink transmission */
            for (i = 0; i < ru_patterns[pattern_idx].num_users; i++) {
                if (ru_patterns[pattern_idx].punctured[i])
                    continue;

                get_random_bytes(ctx->test_buffer, RU_TEST_BUFFER_SIZE);
                if (!wait_for_completion_timeout(&ctx->tx_done[i],
                                               msecs_to_jiffies(RU_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for TX completion on user %d\n", i);
                    return TEST_FAIL;
                }
            }
        } else {
            /* Uplink reception */
            for (i = 0; i < ru_patterns[pattern_idx].num_users; i++) {
                if (ru_patterns[pattern_idx].punctured[i])
                    continue;

                if (!wait_for_completion_timeout(&ctx->rx_done[i],
                                               msecs_to_jiffies(RU_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for RX completion on user %d\n", i);
                    return TEST_FAIL;
                }
            }
        }
    }

    pr_info("Multi-RU stress test passed\n");
    return TEST_PASS;
}

/* Module initialization */
static int __init multi_ru_test_module_init(void)
{
    struct multi_ru_test_context *ctx;

    pr_info("Initializing Multi-RU test module\n");

    /* Initialize test context */
    ctx = ru_test_init();
    if (!ctx) {
        pr_err("Failed to initialize test context\n");
        return -ENOMEM;
    }

    /* Register test cases */
    REGISTER_TEST("ru_setup", "Test Multi-RU setup and initialization",
                 test_ru_setup, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("ru_allocation", "Test Multi-RU allocation",
                 test_ru_allocation, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("ru_dl", "Test downlink Multi-RU functionality",
                 test_ru_dl, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("ru_ul", "Test uplink Multi-RU functionality",
                 test_ru_ul, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("ru_stress", "Stress test Multi-RU functionality",
                 test_ru_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

/* Module cleanup */
static void __exit multi_ru_test_module_exit(void)
{
    struct test_results results;

    /* Get test results */
    get_test_results(&results);
    pr_info("Multi-RU tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);

    /* Cleanup is handled by test framework */
    ru_test_cleanup(NULL);
}

module_init(multi_ru_test_module_init);
module_exit(multi_ru_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 Multi-RU Test Module");
MODULE_VERSION("1.0"); 