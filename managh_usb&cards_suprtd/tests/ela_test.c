#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/completion.h>
#include <linux/etherdevice.h>
#include <linux/ieee80211.h>
#include <linux/random.h>
#include "../include/mac/mac_core.h"
#include "../include/phy/phy_core.h"
#include "test_framework.h"

#define ELA_TEST_BUFFER_SIZE 8192
#define ELA_TEST_ITERATIONS 1000
#define ELA_TEST_TIMEOUT_MS 5000
#define ELA_MAX_LINKS 8
#define ELA_MAX_MCS 12
#define ELA_MAX_NSS 8
#define ELA_MAX_BW 320
#define ELA_MIN_SNR 0
#define ELA_MAX_SNR 60

/* Link adaptation parameters */
struct ela_params {
    u8 mcs;
    u8 nss;
    u32 bw;
    bool ldpc;
    bool stbc;
    bool mu_mimo;
    bool ru_allocation;
    u8 tx_power;
    u8 min_rssi;
    u8 max_rssi;
    u8 min_snr;
    u8 max_snr;
    u32 min_rate;
    u32 max_rate;
    u32 pkt_loss_threshold;
    u32 retry_threshold;
};

/* Enhanced Link Adaptation Test Context */
struct ela_test_context {
    struct wifi67_mac_dev *mac_dev;
    struct wifi67_phy_dev *phy_dev;
    struct completion setup_done;
    struct completion adapt_done;
    struct completion tx_done[ELA_MAX_LINKS];
    struct completion rx_done[ELA_MAX_LINKS];
    atomic_t active_links;
    atomic_t tx_count;
    atomic_t rx_count;
    void *test_buffer;
    struct sk_buff_head tx_queue;
    struct sk_buff_head rx_queue;
    spinlock_t lock;
    u8 mac_addr[ELA_MAX_LINKS][ETH_ALEN];
    struct ela_params params[ELA_MAX_LINKS];
    bool ela_enabled;
    bool dl_ela_enabled;
    bool ul_ela_enabled;
    struct {
        u64 bytes;
        ktime_t start;
        ktime_t end;
    } throughput;
    /* Link quality metrics */
    struct {
        u32 rssi[ELA_MAX_LINKS];
        u32 snr[ELA_MAX_LINKS];
        u32 per[ELA_MAX_LINKS];
        u32 retries[ELA_MAX_LINKS];
        u32 success_rate[ELA_MAX_LINKS];
    } metrics;
    /* Historical data */
    struct {
        u32 mcs_stats[ELA_MAX_LINKS][ELA_MAX_MCS];
        u32 nss_stats[ELA_MAX_LINKS][ELA_MAX_NSS];
        u32 bw_stats[ELA_MAX_LINKS][4]; /* 20, 40, 80, 160 MHz */
    } history;
};

/* Test callback functions */
static void ela_test_setup_callback(void *data)
{
    struct ela_test_context *ctx = data;
    complete(&ctx->setup_done);
}

static void ela_test_adapt_callback(void *data)
{
    struct ela_test_context *ctx = data;
    complete(&ctx->adapt_done);
}

static void ela_test_tx_callback(void *data, u32 link_idx)
{
    struct ela_test_context *ctx = data;
    if (link_idx < ELA_MAX_LINKS) {
        atomic_inc(&ctx->tx_count);
        complete(&ctx->tx_done[link_idx]);
    }
}

static void ela_test_rx_callback(void *data, u32 link_idx)
{
    struct ela_test_context *ctx = data;
    if (link_idx < ELA_MAX_LINKS) {
        atomic_inc(&ctx->rx_count);
        complete(&ctx->rx_done[link_idx]);
    }
}

/* Helper functions */
static void ela_test_init_params(struct ela_test_context *ctx, u32 link_idx)
{
    struct ela_params *params = &ctx->params[link_idx];

    /* Initialize with conservative values */
    params->mcs = 0;
    params->nss = 1;
    params->bw = 20;
    params->ldpc = true;
    params->stbc = false;
    params->mu_mimo = false;
    params->ru_allocation = false;
    params->tx_power = 20; /* dBm */
    params->min_rssi = -82; /* dBm */
    params->max_rssi = -30; /* dBm */
    params->min_snr = 10; /* dB */
    params->max_snr = 50; /* dB */
    params->min_rate = 6; /* Mbps */
    params->max_rate = 9608; /* Mbps (max for WiFi 7) */
    params->pkt_loss_threshold = 10; /* percentage */
    params->retry_threshold = 3;
}

static int ela_test_set_params(struct ela_test_context *ctx, u32 link_idx)
{
    struct ela_params *params = &ctx->params[link_idx];
    int ret;

    ret = wifi67_mac_set_link_params(ctx->mac_dev, link_idx,
                                   params->mcs,
                                   params->nss,
                                   params->bw,
                                   params->ldpc,
                                   params->stbc,
                                   params->mu_mimo,
                                   params->ru_allocation,
                                   params->tx_power);
    if (ret)
        return ret;

    ret = wifi67_mac_set_link_thresholds(ctx->mac_dev, link_idx,
                                       params->min_rssi,
                                       params->max_rssi,
                                       params->min_snr,
                                       params->max_snr,
                                       params->min_rate,
                                       params->max_rate,
                                       params->pkt_loss_threshold,
                                       params->retry_threshold);
    return ret;
}

static void ela_test_start_throughput(struct ela_test_context *ctx)
{
    ctx->throughput.bytes = 0;
    ctx->throughput.start = ktime_get();
}

static u64 ela_test_end_throughput(struct ela_test_context *ctx)
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

static void ela_test_update_metrics(struct ela_test_context *ctx, u32 link_idx)
{
    /* Get current link metrics */
    wifi67_mac_get_link_metrics(ctx->mac_dev, link_idx,
                              &ctx->metrics.rssi[link_idx],
                              &ctx->metrics.snr[link_idx],
                              &ctx->metrics.per[link_idx],
                              &ctx->metrics.retries[link_idx],
                              &ctx->metrics.success_rate[link_idx]);

    /* Update history */
    ctx->history.mcs_stats[link_idx][ctx->params[link_idx].mcs]++;
    ctx->history.nss_stats[link_idx][ctx->params[link_idx].nss - 1]++;
    ctx->history.bw_stats[link_idx][ilog2(ctx->params[link_idx].bw/20)]++;
}

/* Test initialization */
static struct ela_test_context *ela_test_init(void)
{
    struct ela_test_context *ctx;
    int i;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize completions */
    init_completion(&ctx->setup_done);
    init_completion(&ctx->adapt_done);
    for (i = 0; i < ELA_MAX_LINKS; i++) {
        init_completion(&ctx->tx_done[i]);
        init_completion(&ctx->rx_done[i]);
    }

    /* Initialize queues */
    skb_queue_head_init(&ctx->tx_queue);
    skb_queue_head_init(&ctx->rx_queue);

    /* Initialize spinlock */
    spin_lock_init(&ctx->lock);

    /* Allocate test buffer */
    ctx->test_buffer = kmalloc(ELA_TEST_BUFFER_SIZE, GFP_KERNEL);
    if (!ctx->test_buffer)
        goto err_free_ctx;

    /* Generate random MAC addresses */
    for (i = 0; i < ELA_MAX_LINKS; i++) {
        eth_random_addr(ctx->mac_addr[i]);
        ela_test_init_params(ctx, i);
    }

    return ctx;

err_free_ctx:
    kfree(ctx);
    return NULL;
}

/* Test cleanup */
static void ela_test_cleanup(struct ela_test_context *ctx)
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
static int test_ela_setup(void *data)
{
    struct ela_test_context *ctx = data;
    int ret, i;

    pr_info("Starting Enhanced Link Adaptation setup test\n");

    /* Enable ELA */
    ret = wifi67_mac_enable_ela(ctx->mac_dev, true);
    if (ret) {
        pr_err("Failed to enable ELA: %d\n", ret);
        return TEST_FAIL;
    }

    /* Initialize link parameters */
    for (i = 0; i < ELA_MAX_LINKS; i++) {
        ret = ela_test_set_params(ctx, i);
        if (ret) {
            pr_err("Failed to set link %d parameters: %d\n", i, ret);
            return TEST_FAIL;
        }
    }

    /* Wait for setup completion */
    if (!wait_for_completion_timeout(&ctx->setup_done,
                                   msecs_to_jiffies(ELA_TEST_TIMEOUT_MS))) {
        pr_err("Timeout waiting for ELA setup\n");
        return TEST_FAIL;
    }

    ctx->ela_enabled = true;
    pr_info("Enhanced Link Adaptation setup test passed\n");
    return TEST_PASS;
}

static int test_ela_adaptation(void *data)
{
    struct ela_test_context *ctx = data;
    int ret, i, j;
    u32 old_mcs, old_nss, old_bw;

    pr_info("Starting Enhanced Link Adaptation test\n");

    if (!ctx->ela_enabled) {
        pr_err("ELA not enabled\n");
        return TEST_FAIL;
    }

    /* Test adaptation under various conditions */
    for (i = 0; i < ELA_MAX_LINKS; i++) {
        /* Start with conservative parameters */
        ela_test_init_params(ctx, i);
        ret = ela_test_set_params(ctx, i);
        if (ret) {
            pr_err("Failed to set link %d parameters: %d\n", i, ret);
            return TEST_FAIL;
        }

        /* Run adaptation iterations */
        for (j = 0; j < ELA_TEST_ITERATIONS; j++) {
            /* Store current parameters */
            old_mcs = ctx->params[i].mcs;
            old_nss = ctx->params[i].nss;
            old_bw = ctx->params[i].bw;

            /* Simulate changing channel conditions */
            ctx->metrics.snr[i] = ELA_MIN_SNR +
                (get_random_u32() % (ELA_MAX_SNR - ELA_MIN_SNR));
            wifi67_mac_set_link_conditions(ctx->mac_dev, i,
                                        ctx->metrics.snr[i]);

            /* Wait for adaptation */
            if (!wait_for_completion_timeout(&ctx->adapt_done,
                                           msecs_to_jiffies(ELA_TEST_TIMEOUT_MS))) {
                pr_err("Timeout waiting for adaptation on link %d\n", i);
                return TEST_FAIL;
            }

            /* Update metrics */
            ela_test_update_metrics(ctx, i);

            /* Verify adaptation */
            if (ctx->metrics.snr[i] > 40) {
                /* High SNR should result in aggressive parameters */
                if (ctx->params[i].mcs <= old_mcs &&
                    ctx->params[i].nss <= old_nss &&
                    ctx->params[i].bw <= old_bw) {
                    pr_err("Link %d: Parameters not increased for high SNR\n", i);
                    return TEST_FAIL;
                }
            } else if (ctx->metrics.snr[i] < 15) {
                /* Low SNR should result in conservative parameters */
                if (ctx->params[i].mcs >= old_mcs &&
                    ctx->params[i].nss >= old_nss &&
                    ctx->params[i].bw >= old_bw) {
                    pr_err("Link %d: Parameters not decreased for low SNR\n", i);
                    return TEST_FAIL;
                }
            }
        }

        /* Print adaptation statistics */
        pr_info("Link %d adaptation statistics:\n", i);
        pr_info("  MCS distribution:");
        for (j = 0; j < ELA_MAX_MCS; j++)
            pr_cont(" %u", ctx->history.mcs_stats[i][j]);
        pr_cont("\n");

        pr_info("  NSS distribution:");
        for (j = 0; j < ELA_MAX_NSS; j++)
            pr_cont(" %u", ctx->history.nss_stats[i][j]);
        pr_cont("\n");

        pr_info("  BW distribution:");
        for (j = 0; j < 4; j++)
            pr_cont(" %u", ctx->history.bw_stats[i][j]);
        pr_cont("\n");
    }

    pr_info("Enhanced Link Adaptation test passed\n");
    return TEST_PASS;
}

static int test_ela_throughput(void *data)
{
    struct ela_test_context *ctx = data;
    int ret, i, j;
    u64 throughput;

    pr_info("Starting Enhanced Link Adaptation throughput test\n");

    if (!ctx->ela_enabled) {
        pr_err("ELA not enabled\n");
        return TEST_FAIL;
    }

    /* Test throughput under various conditions */
    for (i = 0; i < ELA_MAX_LINKS; i++) {
        /* Start throughput measurement */
        ela_test_start_throughput(ctx);

        /* Run test iterations */
        for (j = 0; j < ELA_TEST_ITERATIONS; j++) {
            /* Generate and transmit test data */
            get_random_bytes(ctx->test_buffer, ELA_TEST_BUFFER_SIZE);
            ctx->throughput.bytes += ELA_TEST_BUFFER_SIZE;

            /* Simulate varying channel conditions */
            ctx->metrics.snr[i] = ELA_MIN_SNR +
                (get_random_u32() % (ELA_MAX_SNR - ELA_MIN_SNR));
            wifi67_mac_set_link_conditions(ctx->mac_dev, i,
                                        ctx->metrics.snr[i]);

            /* Wait for adaptation */
            if (!wait_for_completion_timeout(&ctx->adapt_done,
                                           msecs_to_jiffies(ELA_TEST_TIMEOUT_MS))) {
                pr_err("Timeout waiting for adaptation on link %d\n", i);
                return TEST_FAIL;
            }

            /* Wait for TX completion */
            if (!wait_for_completion_timeout(&ctx->tx_done[i],
                                           msecs_to_jiffies(ELA_TEST_TIMEOUT_MS))) {
                pr_err("Timeout waiting for TX completion on link %d\n", i);
                return TEST_FAIL;
            }

            /* Update metrics */
            ela_test_update_metrics(ctx, i);
        }

        throughput = ela_test_end_throughput(ctx);
        pr_info("Link %d throughput: %llu Mbps (avg SNR: %u)\n",
                i, throughput, ctx->metrics.snr[i]);

        /* Verify minimum throughput */
        if (throughput < ctx->params[i].min_rate) {
            pr_err("Link %d throughput below minimum: %llu < %u\n",
                   i, throughput, ctx->params[i].min_rate);
            return TEST_FAIL;
        }
    }

    pr_info("Enhanced Link Adaptation throughput test passed\n");
    return TEST_PASS;
}

static int test_ela_stress(void *data)
{
    struct ela_test_context *ctx = data;
    int ret, i;
    u32 snr;

    pr_info("Starting Enhanced Link Adaptation stress test\n");

    if (!ctx->ela_enabled) {
        pr_err("ELA not enabled\n");
        return TEST_FAIL;
    }

    /* Perform rapid channel condition changes */
    for (i = 0; i < ELA_TEST_ITERATIONS; i++) {
        /* Randomly change SNR for all links */
        for (i = 0; i < ELA_MAX_LINKS; i++) {
            snr = ELA_MIN_SNR + (get_random_u32() % (ELA_MAX_SNR - ELA_MIN_SNR));
            wifi67_mac_set_link_conditions(ctx->mac_dev, i, snr);
        }

        /* Wait for adaptation */
        if (!wait_for_completion_timeout(&ctx->adapt_done,
                                       msecs_to_jiffies(ELA_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for adaptation\n");
            return TEST_FAIL;
        }

        /* Generate and transmit test data */
        get_random_bytes(ctx->test_buffer, ELA_TEST_BUFFER_SIZE);

        /* Wait for TX completion on random link */
        i = get_random_u32() % ELA_MAX_LINKS;
        if (!wait_for_completion_timeout(&ctx->tx_done[i],
                                       msecs_to_jiffies(ELA_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for TX completion on link %d\n", i);
            return TEST_FAIL;
        }

        /* Update metrics */
        ela_test_update_metrics(ctx, i);
    }

    pr_info("Enhanced Link Adaptation stress test passed\n");
    return TEST_PASS;
}

/* Module initialization */
static int __init ela_test_module_init(void)
{
    struct ela_test_context *ctx;

    pr_info("Initializing Enhanced Link Adaptation test module\n");

    /* Initialize test context */
    ctx = ela_test_init();
    if (!ctx) {
        pr_err("Failed to initialize test context\n");
        return -ENOMEM;
    }

    /* Register test cases */
    REGISTER_TEST("ela_setup", "Test Enhanced Link Adaptation setup",
                 test_ela_setup, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("ela_adaptation", "Test Enhanced Link Adaptation functionality",
                 test_ela_adaptation, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("ela_throughput", "Test Enhanced Link Adaptation throughput",
                 test_ela_throughput, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("ela_stress", "Stress test Enhanced Link Adaptation",
                 test_ela_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

/* Module cleanup */
static void __exit ela_test_module_exit(void)
{
    struct test_results results;

    /* Get test results */
    get_test_results(&results);
    pr_info("Enhanced Link Adaptation tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);

    /* Cleanup is handled by test framework */
    ela_test_cleanup(NULL);
}

module_init(ela_test_module_init);
module_exit(ela_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 Enhanced Link Adaptation Test Module");
MODULE_VERSION("1.0"); 