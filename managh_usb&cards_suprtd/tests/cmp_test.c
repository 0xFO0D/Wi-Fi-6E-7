#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/completion.h>
#include <linux/etherdevice.h>
#include <linux/ieee80211.h>
#include <linux/list.h>
#include "../include/mac/mac_core.h"
#include "../include/phy/phy_core.h"
#include "test_framework.h"

#define CMP_TEST_BUFFER_SIZE 8192
#define CMP_TEST_ITERATIONS 1000
#define CMP_TEST_TIMEOUT_MS 5000
#define CMP_MAX_APS 8
#define CMP_MAX_CLIENTS 16

/* CMP Test Context */
struct cmp_test_context {
    struct wifi67_mac_dev *mac_dev;
    struct wifi67_phy_dev *phy_dev;
    struct completion setup_done;
    struct completion sync_done;
    struct completion tx_done[CMP_MAX_APS];
    struct completion rx_done[CMP_MAX_APS];
    atomic_t active_aps;
    atomic_t active_clients;
    atomic_t tx_count;
    atomic_t rx_count;
    void *test_buffer;
    struct sk_buff_head tx_queue;
    struct sk_buff_head rx_queue;
    spinlock_t lock;
    u8 ap_mac_addr[CMP_MAX_APS][ETH_ALEN];
    u8 client_mac_addr[CMP_MAX_CLIENTS][ETH_ALEN];
    struct {
        u32 num_aps;
        u32 clients_per_ap[CMP_MAX_APS];
        bool joint_tx[CMP_MAX_APS];
        bool joint_rx[CMP_MAX_APS];
    } coordination;
    bool cmp_enabled;
    bool joint_tx_enabled;
    bool joint_rx_enabled;
    struct {
        u64 bytes;
        ktime_t start;
        ktime_t end;
    } throughput;
    /* Channel state information */
    void *csi_data;
    size_t csi_size;
    /* Timing synchronization */
    struct {
        u64 timestamp[CMP_MAX_APS];
        s32 offset[CMP_MAX_APS];
        bool synced[CMP_MAX_APS];
    } sync;
};

/* AP coordination patterns */
struct coordination_pattern {
    u32 num_aps;
    u32 clients_per_ap[CMP_MAX_APS];
    bool joint_tx[CMP_MAX_APS];
    bool joint_rx[CMP_MAX_APS];
};

static const struct coordination_pattern coord_patterns[] = {
    /* 2 APs with joint transmission */
    {2, {4, 4, 0, 0, 0, 0, 0, 0},
     {true, true, false, false, false, false, false, false},
     {false, false, false, false, false, false, false, false}},
    /* 3 APs with joint reception */
    {3, {3, 3, 3, 0, 0, 0, 0, 0},
     {false, false, false, false, false, false, false, false},
     {true, true, true, false, false, false, false, false}},
    /* 4 APs with mixed coordination */
    {4, {2, 2, 2, 2, 0, 0, 0, 0},
     {true, true, false, false, false, false, false, false},
     {false, false, true, true, false, false, false, false}}
};

/* Test callback functions */
static void cmp_test_setup_callback(void *data)
{
    struct cmp_test_context *ctx = data;
    complete(&ctx->setup_done);
}

static void cmp_test_sync_callback(void *data)
{
    struct cmp_test_context *ctx = data;
    complete(&ctx->sync_done);
}

static void cmp_test_tx_callback(void *data, u32 ap_idx)
{
    struct cmp_test_context *ctx = data;
    if (ap_idx < CMP_MAX_APS) {
        atomic_inc(&ctx->tx_count);
        complete(&ctx->tx_done[ap_idx]);
    }
}

static void cmp_test_rx_callback(void *data, u32 ap_idx)
{
    struct cmp_test_context *ctx = data;
    if (ap_idx < CMP_MAX_APS) {
        atomic_inc(&ctx->rx_count);
        complete(&ctx->rx_done[ap_idx]);
    }
}

/* Helper functions */
static int cmp_test_set_coordination(struct cmp_test_context *ctx,
                                   const struct coordination_pattern *pattern)
{
    int ret, i;

    ctx->coordination.num_aps = pattern->num_aps;
    for (i = 0; i < pattern->num_aps; i++) {
        ctx->coordination.clients_per_ap[i] = pattern->clients_per_ap[i];
        ctx->coordination.joint_tx[i] = pattern->joint_tx[i];
        ctx->coordination.joint_rx[i] = pattern->joint_rx[i];

        ret = wifi67_mac_set_coordination(ctx->mac_dev, i,
                                        pattern->clients_per_ap[i],
                                        pattern->joint_tx[i],
                                        pattern->joint_rx[i]);
        if (ret)
            return ret;
    }

    atomic_set(&ctx->active_aps, pattern->num_aps);
    return 0;
}

static void cmp_test_start_throughput(struct cmp_test_context *ctx)
{
    ctx->throughput.bytes = 0;
    ctx->throughput.start = ktime_get();
}

static u64 cmp_test_end_throughput(struct cmp_test_context *ctx)
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
static struct cmp_test_context *cmp_test_init(void)
{
    struct cmp_test_context *ctx;
    int i;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize completions */
    init_completion(&ctx->setup_done);
    init_completion(&ctx->sync_done);
    for (i = 0; i < CMP_MAX_APS; i++) {
        init_completion(&ctx->tx_done[i]);
        init_completion(&ctx->rx_done[i]);
    }

    /* Initialize queues */
    skb_queue_head_init(&ctx->tx_queue);
    skb_queue_head_init(&ctx->rx_queue);

    /* Initialize spinlock */
    spin_lock_init(&ctx->lock);

    /* Allocate test buffer */
    ctx->test_buffer = kmalloc(CMP_TEST_BUFFER_SIZE, GFP_KERNEL);
    if (!ctx->test_buffer)
        goto err_free_ctx;

    /* Allocate CSI data buffer */
    ctx->csi_size = CMP_MAX_APS * CMP_MAX_CLIENTS * 1024;
    ctx->csi_data = kmalloc(ctx->csi_size, GFP_KERNEL);
    if (!ctx->csi_data)
        goto err_free_buffer;

    /* Generate random MAC addresses */
    for (i = 0; i < CMP_MAX_APS; i++)
        eth_random_addr(ctx->ap_mac_addr[i]);
    for (i = 0; i < CMP_MAX_CLIENTS; i++)
        eth_random_addr(ctx->client_mac_addr[i]);

    return ctx;

err_free_buffer:
    kfree(ctx->test_buffer);
err_free_ctx:
    kfree(ctx);
    return NULL;
}

/* Test cleanup */
static void cmp_test_cleanup(struct cmp_test_context *ctx)
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
static int test_cmp_setup(void *data)
{
    struct cmp_test_context *ctx = data;
    int ret;

    pr_info("Starting CMP setup test\n");

    /* Enable CMP */
    ret = wifi67_mac_enable_cmp(ctx->mac_dev, true);
    if (ret) {
        pr_err("Failed to enable CMP: %d\n", ret);
        return TEST_FAIL;
    }

    /* Wait for setup completion */
    if (!wait_for_completion_timeout(&ctx->setup_done,
                                   msecs_to_jiffies(CMP_TEST_TIMEOUT_MS))) {
        pr_err("Timeout waiting for CMP setup\n");
        return TEST_FAIL;
    }

    ctx->cmp_enabled = true;
    pr_info("CMP setup test passed\n");
    return TEST_PASS;
}

static int test_cmp_sync(void *data)
{
    struct cmp_test_context *ctx = data;
    int ret, i;

    pr_info("Starting CMP synchronization test\n");

    if (!ctx->cmp_enabled) {
        pr_err("CMP not enabled\n");
        return TEST_FAIL;
    }

    /* Test each coordination pattern */
    for (i = 0; i < ARRAY_SIZE(coord_patterns); i++) {
        ret = cmp_test_set_coordination(ctx, &coord_patterns[i]);
        if (ret) {
            pr_err("Failed to set coordination pattern %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Trigger timing synchronization */
        ret = wifi67_mac_trigger_sync(ctx->mac_dev);
        if (ret) {
            pr_err("Failed to trigger synchronization: %d\n", ret);
            return TEST_FAIL;
        }

        /* Wait for sync completion */
        if (!wait_for_completion_timeout(&ctx->sync_done,
                                       msecs_to_jiffies(CMP_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for synchronization\n");
            return TEST_FAIL;
        }

        /* Verify timing offsets */
        for (i = 0; i < coord_patterns[i].num_aps; i++) {
            if (abs(ctx->sync.offset[i]) > 1000) { /* 1us threshold */
                pr_err("AP %d timing offset too large: %d ns\n",
                       i, ctx->sync.offset[i]);
                return TEST_FAIL;
            }
        }
    }

    pr_info("CMP synchronization test passed\n");
    return TEST_PASS;
}

static int test_cmp_joint_tx(void *data)
{
    struct cmp_test_context *ctx = data;
    int ret, i, j;
    u64 throughput;

    pr_info("Starting CMP joint transmission test\n");

    if (!ctx->cmp_enabled) {
        pr_err("CMP not enabled\n");
        return TEST_FAIL;
    }

    /* Test each coordination pattern */
    for (i = 0; i < ARRAY_SIZE(coord_patterns); i++) {
        if (!coord_patterns[i].joint_tx[0])
            continue;

        ret = cmp_test_set_coordination(ctx, &coord_patterns[i]);
        if (ret) {
            pr_err("Failed to set coordination pattern %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Start throughput measurement */
        cmp_test_start_throughput(ctx);

        /* Run test iterations */
        for (j = 0; j < CMP_TEST_ITERATIONS; j++) {
            /* Generate and transmit test data */
            get_random_bytes(ctx->test_buffer, CMP_TEST_BUFFER_SIZE);
            ctx->throughput.bytes += CMP_TEST_BUFFER_SIZE;

            /* Wait for TX completion from coordinated APs */
            for (i = 0; i < coord_patterns[i].num_aps; i++) {
                if (!coord_patterns[i].joint_tx[i])
                    continue;

                if (!wait_for_completion_timeout(&ctx->tx_done[i],
                                               msecs_to_jiffies(CMP_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for TX completion on AP %d\n", i);
                    return TEST_FAIL;
                }
            }
        }

        throughput = cmp_test_end_throughput(ctx);
        pr_info("Joint TX throughput (pattern %d): %llu Mbps\n", i, throughput);
    }

    ctx->joint_tx_enabled = true;
    pr_info("CMP joint transmission test passed\n");
    return TEST_PASS;
}

static int test_cmp_joint_rx(void *data)
{
    struct cmp_test_context *ctx = data;
    int ret, i, j;
    u64 throughput;

    pr_info("Starting CMP joint reception test\n");

    if (!ctx->cmp_enabled) {
        pr_err("CMP not enabled\n");
        return TEST_FAIL;
    }

    /* Test each coordination pattern */
    for (i = 0; i < ARRAY_SIZE(coord_patterns); i++) {
        if (!coord_patterns[i].joint_rx[0])
            continue;

        ret = cmp_test_set_coordination(ctx, &coord_patterns[i]);
        if (ret) {
            pr_err("Failed to set coordination pattern %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Start throughput measurement */
        cmp_test_start_throughput(ctx);

        /* Run test iterations */
        for (j = 0; j < CMP_TEST_ITERATIONS; j++) {
            /* Wait for RX completion from coordinated APs */
            for (i = 0; i < coord_patterns[i].num_aps; i++) {
                if (!coord_patterns[i].joint_rx[i])
                    continue;

                if (!wait_for_completion_timeout(&ctx->rx_done[i],
                                               msecs_to_jiffies(CMP_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for RX completion on AP %d\n", i);
                    return TEST_FAIL;
                }
                ctx->throughput.bytes += CMP_TEST_BUFFER_SIZE;
            }
        }

        throughput = cmp_test_end_throughput(ctx);
        pr_info("Joint RX throughput (pattern %d): %llu Mbps\n", i, throughput);
    }

    ctx->joint_rx_enabled = true;
    pr_info("CMP joint reception test passed\n");
    return TEST_PASS;
}

static int test_cmp_stress(void *data)
{
    struct cmp_test_context *ctx = data;
    int ret, i;
    u32 pattern_idx, op_type;

    pr_info("Starting CMP stress test\n");

    if (!ctx->cmp_enabled) {
        pr_err("CMP not enabled\n");
        return TEST_FAIL;
    }

    /* Perform rapid coordination pattern and operation changes */
    for (i = 0; i < CMP_TEST_ITERATIONS; i++) {
        /* Randomly select coordination pattern and operation */
        pattern_idx = get_random_u32() % ARRAY_SIZE(coord_patterns);
        op_type = get_random_u32() % 2;

        /* Set coordination pattern */
        ret = cmp_test_set_coordination(ctx, &coord_patterns[pattern_idx]);
        if (ret) {
            pr_err("Failed to set coordination pattern %d: %d\n", pattern_idx, ret);
            return TEST_FAIL;
        }

        /* Trigger timing synchronization */
        ret = wifi67_mac_trigger_sync(ctx->mac_dev);
        if (ret) {
            pr_err("Failed to trigger synchronization: %d\n", ret);
            return TEST_FAIL;
        }

        if (!wait_for_completion_timeout(&ctx->sync_done,
                                       msecs_to_jiffies(CMP_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for synchronization\n");
            return TEST_FAIL;
        }

        /* Run single iteration of selected operation */
        if (op_type == 0) {
            /* Joint transmission */
            for (i = 0; i < coord_patterns[pattern_idx].num_aps; i++) {
                if (!coord_patterns[pattern_idx].joint_tx[i])
                    continue;

                get_random_bytes(ctx->test_buffer, CMP_TEST_BUFFER_SIZE);
                if (!wait_for_completion_timeout(&ctx->tx_done[i],
                                               msecs_to_jiffies(CMP_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for TX completion on AP %d\n", i);
                    return TEST_FAIL;
                }
            }
        } else {
            /* Joint reception */
            for (i = 0; i < coord_patterns[pattern_idx].num_aps; i++) {
                if (!coord_patterns[pattern_idx].joint_rx[i])
                    continue;

                if (!wait_for_completion_timeout(&ctx->rx_done[i],
                                               msecs_to_jiffies(CMP_TEST_TIMEOUT_MS))) {
                    pr_err("Timeout waiting for RX completion on AP %d\n", i);
                    return TEST_FAIL;
                }
            }
        }
    }

    pr_info("CMP stress test passed\n");
    return TEST_PASS;
}

/* Module initialization */
static int __init cmp_test_module_init(void)
{
    struct cmp_test_context *ctx;

    pr_info("Initializing CMP test module\n");

    /* Initialize test context */
    ctx = cmp_test_init();
    if (!ctx) {
        pr_err("Failed to initialize test context\n");
        return -ENOMEM;
    }

    /* Register test cases */
    REGISTER_TEST("cmp_setup", "Test CMP setup and initialization",
                 test_cmp_setup, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("cmp_sync", "Test CMP timing synchronization",
                 test_cmp_sync, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("cmp_joint_tx", "Test CMP joint transmission",
                 test_cmp_joint_tx, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("cmp_joint_rx", "Test CMP joint reception",
                 test_cmp_joint_rx, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("cmp_stress", "Stress test CMP functionality",
                 test_cmp_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

/* Module cleanup */
static void __exit cmp_test_module_exit(void)
{
    struct test_results results;

    /* Get test results */
    get_test_results(&results);
    pr_info("CMP tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);

    /* Cleanup is handled by test framework */
    cmp_test_cleanup(NULL);
}

module_init(cmp_test_module_init);
module_exit(cmp_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 Coordinated Multi-AP Test Module");
MODULE_VERSION("1.0"); 