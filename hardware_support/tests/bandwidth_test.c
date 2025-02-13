#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/skbuff.h>
#include <linux/completion.h>
#include "../include/phy/phy_core.h"
#include "test_framework.h"

#define BW_TEST_BUFFER_SIZE 16384
#define BW_TEST_ITERATIONS 1000
#define BW_TEST_TIMEOUT_MS 5000

/* Bandwidth Test Context */
struct bw_test_context {
    struct wifi67_phy_dev *phy_dev;
    struct completion cal_done;
    struct completion test_done;
    atomic_t test_count;
    void *test_buffer;
    spinlock_t lock;
    u32 current_bw;
    u32 current_channel;
    u32 error_count;
    bool bw_320_enabled;
    bool calibrated;
    struct {
        u64 bytes;
        ktime_t start;
        ktime_t end;
    } throughput;
};

/* Bandwidth modes */
#define BW_MODE_20MHZ   0
#define BW_MODE_40MHZ   1
#define BW_MODE_80MHZ   2
#define BW_MODE_160MHZ  3
#define BW_MODE_320MHZ  4

/* Channel configurations for different bandwidths */
struct channel_config {
    u32 primary;
    u32 secondary;
    u32 bandwidth;
};

static const struct channel_config channel_configs[] = {
    {36, 0, BW_MODE_20MHZ},
    {36, 40, BW_MODE_40MHZ},
    {36, 44, BW_MODE_80MHZ},
    {36, 52, BW_MODE_160MHZ},
    {36, 68, BW_MODE_320MHZ}
};

/* Test callback functions */
static void bw_test_cal_callback(void *data, int status)
{
    struct bw_test_context *ctx = data;
    if (status == 0)
        ctx->calibrated = true;
    complete(&ctx->cal_done);
}

static void bw_test_done_callback(void *data, int status)
{
    struct bw_test_context *ctx = data;
    atomic_inc(&ctx->test_count);
    if (status != 0)
        ctx->error_count++;
    complete(&ctx->test_done);
}

/* Helper functions */
static int bw_test_set_channel(struct bw_test_context *ctx,
                              const struct channel_config *config)
{
    int ret;

    ret = wifi67_phy_set_bandwidth(ctx->phy_dev, config->bandwidth);
    if (ret)
        return ret;

    ret = wifi67_phy_config(ctx->phy_dev, config->primary, WIFI_BAND_6GHZ);
    if (ret)
        return ret;

    ctx->current_bw = config->bandwidth;
    ctx->current_channel = config->primary;
    return 0;
}

static void bw_test_start_throughput(struct bw_test_context *ctx)
{
    ctx->throughput.bytes = 0;
    ctx->throughput.start = ktime_get();
}

static u64 bw_test_end_throughput(struct bw_test_context *ctx)
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
static struct bw_test_context *bw_test_init(void)
{
    struct bw_test_context *ctx;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize completions */
    init_completion(&ctx->cal_done);
    init_completion(&ctx->test_done);

    /* Initialize spinlock */
    spin_lock_init(&ctx->lock);

    /* Allocate test buffer */
    ctx->test_buffer = kmalloc(BW_TEST_BUFFER_SIZE, GFP_KERNEL);
    if (!ctx->test_buffer)
        goto err_free_ctx;

    return ctx;

err_free_ctx:
    kfree(ctx);
    return NULL;
}

/* Test cleanup */
static void bw_test_cleanup(struct bw_test_context *ctx)
{
    if (!ctx)
        return;

    kfree(ctx->test_buffer);
    kfree(ctx);
}

/* Test cases */
static int test_bw_calibration(void *data)
{
    struct bw_test_context *ctx = data;
    int ret;

    pr_info("Starting bandwidth calibration test\n");

    /* Set initial bandwidth */
    ret = wifi67_phy_set_bandwidth(ctx->phy_dev, BW_MODE_20MHZ);
    if (ret) {
        pr_err("Failed to set initial bandwidth: %d\n", ret);
        return TEST_FAIL;
    }

    /* Wait for calibration */
    if (!wait_for_completion_timeout(&ctx->cal_done,
                                   msecs_to_jiffies(BW_TEST_TIMEOUT_MS))) {
        pr_err("Timeout waiting for calibration\n");
        return TEST_FAIL;
    }

    if (!ctx->calibrated) {
        pr_err("Calibration failed\n");
        return TEST_FAIL;
    }

    pr_info("Bandwidth calibration test passed\n");
    return TEST_PASS;
}

static int test_bw_transitions(void *data)
{
    struct bw_test_context *ctx = data;
    int ret, i;

    pr_info("Starting bandwidth transition test\n");

    if (!ctx->calibrated) {
        pr_err("Bandwidth not calibrated\n");
        return TEST_FAIL;
    }

    /* Test each bandwidth configuration */
    for (i = 0; i < ARRAY_SIZE(channel_configs); i++) {
        ret = bw_test_set_channel(ctx, &channel_configs[i]);
        if (ret) {
            pr_err("Failed to set channel config %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Wait for channel change to take effect */
        msleep(100);

        /* Verify bandwidth change */
        if (ctx->current_bw != channel_configs[i].bandwidth) {
            pr_err("Bandwidth mismatch: expected %d, got %d\n",
                   channel_configs[i].bandwidth, ctx->current_bw);
            return TEST_FAIL;
        }
    }

    pr_info("Bandwidth transition test passed\n");
    return TEST_PASS;
}

static int test_bw_320mhz(void *data)
{
    struct bw_test_context *ctx = data;
    int ret, i;
    u64 throughput;

    pr_info("Starting 320MHz bandwidth test\n");

    if (!ctx->calibrated) {
        pr_err("Bandwidth not calibrated\n");
        return TEST_FAIL;
    }

    /* Enable 320MHz bandwidth */
    ret = bw_test_set_channel(ctx, &channel_configs[4]); /* 320MHz config */
    if (ret) {
        pr_err("Failed to enable 320MHz bandwidth: %d\n", ret);
        return TEST_FAIL;
    }

    /* Run throughput test */
    atomic_set(&ctx->test_count, 0);
    ctx->error_count = 0;
    bw_test_start_throughput(ctx);

    for (i = 0; i < BW_TEST_ITERATIONS; i++) {
        /* Generate and transmit test data */
        get_random_bytes(ctx->test_buffer, BW_TEST_BUFFER_SIZE);
        ctx->throughput.bytes += BW_TEST_BUFFER_SIZE;

        /* Wait for test completion */
        if (!wait_for_completion_timeout(&ctx->test_done,
                                       msecs_to_jiffies(BW_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for test completion\n");
            return TEST_FAIL;
        }
    }

    throughput = bw_test_end_throughput(ctx);
    pr_info("320MHz throughput: %llu Mbps\n", throughput);

    /* Verify minimum throughput for 320MHz */
    if (throughput < 1000) { /* Minimum 1 Gbps */
        pr_err("Throughput too low for 320MHz: %llu Mbps\n", throughput);
        return TEST_FAIL;
    }

    ctx->bw_320_enabled = true;
    pr_info("320MHz bandwidth test passed\n");
    return TEST_PASS;
}

static int test_bw_stress(void *data)
{
    struct bw_test_context *ctx = data;
    int ret, i;
    u32 bw_mode;

    pr_info("Starting bandwidth stress test\n");

    if (!ctx->calibrated) {
        pr_err("Bandwidth not calibrated\n");
        return TEST_FAIL;
    }

    /* Perform rapid bandwidth changes while transmitting data */
    for (i = 0; i < BW_TEST_ITERATIONS; i++) {
        /* Randomly select bandwidth mode */
        bw_mode = get_random_u32() % (ctx->bw_320_enabled ? 5 : 4);
        ret = bw_test_set_channel(ctx, &channel_configs[bw_mode]);
        if (ret) {
            pr_err("Failed to set bandwidth mode %d: %d\n", bw_mode, ret);
            return TEST_FAIL;
        }

        /* Generate and transmit test data */
        get_random_bytes(ctx->test_buffer, BW_TEST_BUFFER_SIZE);

        /* Wait for test completion */
        if (!wait_for_completion_timeout(&ctx->test_done,
                                       msecs_to_jiffies(BW_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for test completion\n");
            return TEST_FAIL;
        }
    }

    /* Check error rate */
    if (ctx->error_count > BW_TEST_ITERATIONS / 50) {
        pr_err("Stress test error rate too high: %d/%d\n",
               ctx->error_count, BW_TEST_ITERATIONS);
        return TEST_FAIL;
    }

    pr_info("Bandwidth stress test passed\n");
    return TEST_PASS;
}

/* Module initialization */
static int __init bw_test_module_init(void)
{
    struct bw_test_context *ctx;

    pr_info("Initializing bandwidth test module\n");

    /* Initialize test context */
    ctx = bw_test_init();
    if (!ctx) {
        pr_err("Failed to initialize test context\n");
        return -ENOMEM;
    }

    /* Register test cases */
    REGISTER_TEST("bw_calibration", "Test bandwidth calibration",
                 test_bw_calibration, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("bw_transitions", "Test bandwidth transitions",
                 test_bw_transitions, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("bw_320mhz", "Test 320MHz bandwidth functionality",
                 test_bw_320mhz, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("bw_stress", "Stress test bandwidth functionality",
                 test_bw_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

/* Module cleanup */
static void __exit bw_test_module_exit(void)
{
    struct test_results results;

    /* Get test results */
    get_test_results(&results);
    pr_info("Bandwidth tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);

    /* Cleanup is handled by test framework */
    bw_test_cleanup(NULL);
}

module_init(bw_test_module_init);
module_exit(bw_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 Bandwidth Test Module");
MODULE_VERSION("1.0"); 