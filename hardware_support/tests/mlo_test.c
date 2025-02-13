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

#define MLO_TEST_MAX_LINKS 4
#define MLO_TEST_BUFFER_SIZE 4096
#define MLO_TEST_ITERATIONS 1000
#define MLO_TEST_TIMEOUT_MS 5000

/* MLO Test Context */
struct mlo_test_context {
    struct wifi67_mac_dev *mac_dev;
    struct wifi67_phy_dev *phy_dev[MLO_TEST_MAX_LINKS];
    struct completion setup_done;
    struct completion link_done[MLO_TEST_MAX_LINKS];
    struct completion tx_done;
    struct completion rx_done;
    atomic_t active_links;
    atomic_t tx_count;
    atomic_t rx_count;
    void *test_buffer;
    struct sk_buff_head tx_queue;
    struct sk_buff_head rx_queue;
    spinlock_t lock;
    u8 mac_addr[MLO_TEST_MAX_LINKS][ETH_ALEN];
    u32 link_states[MLO_TEST_MAX_LINKS];
    bool mlo_enabled;
};

/* Link state definitions */
#define LINK_STATE_DISABLED 0
#define LINK_STATE_ENABLED  1
#define LINK_STATE_ACTIVE   2
#define LINK_STATE_ERROR    3

/* Test callback functions */
static void mlo_test_setup_callback(void *data)
{
    struct mlo_test_context *ctx = data;
    complete(&ctx->setup_done);
}

static void mlo_test_link_callback(void *data, u32 link_id)
{
    struct mlo_test_context *ctx = data;
    if (link_id < MLO_TEST_MAX_LINKS)
        complete(&ctx->link_done[link_id]);
}

static void mlo_test_tx_callback(struct sk_buff *skb, void *data)
{
    struct mlo_test_context *ctx = data;
    atomic_inc(&ctx->tx_count);
    skb_queue_tail(&ctx->tx_queue, skb);
    complete(&ctx->tx_done);
}

static void mlo_test_rx_callback(struct sk_buff *skb, void *data)
{
    struct mlo_test_context *ctx = data;
    atomic_inc(&ctx->rx_count);
    skb_queue_tail(&ctx->rx_queue, skb);
    complete(&ctx->rx_done);
}

/* Helper functions */
static struct sk_buff *mlo_test_alloc_skb(struct mlo_test_context *ctx,
                                         u32 link_id, size_t size)
{
    struct sk_buff *skb;
    struct ieee80211_hdr *hdr;
    u8 *data;

    skb = dev_alloc_skb(size + sizeof(struct ieee80211_hdr));
    if (!skb)
        return NULL;

    /* Reserve space for 802.11 header */
    skb_reserve(skb, sizeof(struct ieee80211_hdr));

    /* Add test data */
    data = skb_put(skb, size);
    get_random_bytes(data, size);

    /* Add 802.11 header */
    hdr = (struct ieee80211_hdr *)skb_push(skb, sizeof(struct ieee80211_hdr));
    memset(hdr, 0, sizeof(struct ieee80211_hdr));
    hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA | 
                                    IEEE80211_STYPE_QOS_DATA);
    memcpy(hdr->addr1, ctx->mac_addr[link_id], ETH_ALEN);
    memcpy(hdr->addr2, ctx->mac_addr[(link_id + 1) % MLO_TEST_MAX_LINKS], 
           ETH_ALEN);
    memcpy(hdr->addr3, ctx->mac_addr[(link_id + 2) % MLO_TEST_MAX_LINKS], 
           ETH_ALEN);

    return skb;
}

/* Test initialization */
static struct mlo_test_context *mlo_test_init(void)
{
    struct mlo_test_context *ctx;
    int i;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize completions */
    init_completion(&ctx->setup_done);
    for (i = 0; i < MLO_TEST_MAX_LINKS; i++)
        init_completion(&ctx->link_done[i]);
    init_completion(&ctx->tx_done);
    init_completion(&ctx->rx_done);

    /* Initialize queues */
    skb_queue_head_init(&ctx->tx_queue);
    skb_queue_head_init(&ctx->rx_queue);

    /* Initialize spinlock */
    spin_lock_init(&ctx->lock);

    /* Allocate test buffer */
    ctx->test_buffer = kmalloc(MLO_TEST_BUFFER_SIZE, GFP_KERNEL);
    if (!ctx->test_buffer)
        goto err_free_ctx;

    /* Generate random MAC addresses */
    for (i = 0; i < MLO_TEST_MAX_LINKS; i++) {
        eth_random_addr(ctx->mac_addr[i]);
        ctx->link_states[i] = LINK_STATE_DISABLED;
    }

    return ctx;

err_free_ctx:
    kfree(ctx);
    return NULL;
}

/* Test cleanup */
static void mlo_test_cleanup(struct mlo_test_context *ctx)
{
    struct sk_buff *skb;

    if (!ctx)
        return;

    /* Free queued packets */
    while ((skb = skb_dequeue(&ctx->tx_queue)))
        dev_kfree_skb(skb);
    while ((skb = skb_dequeue(&ctx->rx_queue)))
        dev_kfree_skb(skb);

    /* Free test buffer */
    kfree(ctx->test_buffer);

    /* Free context */
    kfree(ctx);
}

/* Test cases */
static int test_mlo_setup(void *data)
{
    struct mlo_test_context *ctx = data;
    int ret, i;

    pr_info("Starting MLO setup test\n");

    /* Enable MLO */
    ret = wifi67_mac_enable_mlo(ctx->mac_dev, true);
    if (ret) {
        pr_err("Failed to enable MLO: %d\n", ret);
        return TEST_FAIL;
    }

    /* Setup links */
    for (i = 0; i < MLO_TEST_MAX_LINKS; i++) {
        ret = wifi67_mac_add_link(ctx->mac_dev, i, i == 0);
        if (ret) {
            pr_err("Failed to add link %d: %d\n", i, ret);
            return TEST_FAIL;
        }

        if (!wait_for_completion_timeout(&ctx->link_done[i],
                                       msecs_to_jiffies(MLO_TEST_TIMEOUT_MS))) {
            pr_err("Timeout waiting for link %d setup\n", i);
            return TEST_FAIL;
        }

        ctx->link_states[i] = LINK_STATE_ENABLED;
        atomic_inc(&ctx->active_links);
    }

    ctx->mlo_enabled = true;
    pr_info("MLO setup test passed\n");
    return TEST_PASS;
}

static int test_mlo_link_management(void *data)
{
    struct mlo_test_context *ctx = data;
    int ret, i;

    pr_info("Starting MLO link management test\n");

    if (!ctx->mlo_enabled) {
        pr_err("MLO not enabled\n");
        return TEST_FAIL;
    }

    /* Test link state transitions */
    for (i = 0; i < MLO_TEST_MAX_LINKS; i++) {
        /* Disable link */
        ret = wifi67_mac_set_link_state(ctx->mac_dev, i, 0);
        if (ret) {
            pr_err("Failed to disable link %d: %d\n", i, ret);
            return TEST_FAIL;
        }
        ctx->link_states[i] = LINK_STATE_DISABLED;
        atomic_dec(&ctx->active_links);

        /* Re-enable link */
        ret = wifi67_mac_set_link_state(ctx->mac_dev, i, 1);
        if (ret) {
            pr_err("Failed to enable link %d: %d\n", i, ret);
            return TEST_FAIL;
        }
        ctx->link_states[i] = LINK_STATE_ENABLED;
        atomic_inc(&ctx->active_links);
    }

    pr_info("MLO link management test passed\n");
    return TEST_PASS;
}

static int test_mlo_data_transfer(void *data)
{
    struct mlo_test_context *ctx = data;
    struct sk_buff *skb;
    int ret, i, j;

    pr_info("Starting MLO data transfer test\n");

    if (!ctx->mlo_enabled) {
        pr_err("MLO not enabled\n");
        return TEST_FAIL;
    }

    /* Test data transfer on each link */
    for (i = 0; i < MLO_TEST_MAX_LINKS; i++) {
        if (ctx->link_states[i] != LINK_STATE_ENABLED)
            continue;

        for (j = 0; j < MLO_TEST_ITERATIONS; j++) {
            /* Allocate and send test packet */
            skb = mlo_test_alloc_skb(ctx, i, MLO_TEST_BUFFER_SIZE);
            if (!skb) {
                pr_err("Failed to allocate SKB\n");
                return TEST_FAIL;
            }

            ret = wifi67_mac_tx(ctx->mac_dev, skb, i);
            if (ret) {
                pr_err("Failed to transmit on link %d: %d\n", i, ret);
                dev_kfree_skb(skb);
                return TEST_FAIL;
            }

            /* Wait for TX completion */
            if (!wait_for_completion_timeout(&ctx->tx_done,
                                           msecs_to_jiffies(MLO_TEST_TIMEOUT_MS))) {
                pr_err("Timeout waiting for TX completion on link %d\n", i);
                return TEST_FAIL;
            }
        }
    }

    pr_info("MLO data transfer test passed\n");
    return TEST_PASS;
}

static int test_mlo_stress(void *data)
{
    struct mlo_test_context *ctx = data;
    struct sk_buff *skb;
    int ret, i, j;
    u32 active_links;

    pr_info("Starting MLO stress test\n");

    if (!ctx->mlo_enabled) {
        pr_err("MLO not enabled\n");
        return TEST_FAIL;
    }

    /* Perform rapid link state changes while transferring data */
    for (i = 0; i < MLO_TEST_ITERATIONS; i++) {
        /* Randomly enable/disable links */
        for (j = 0; j < MLO_TEST_MAX_LINKS; j++) {
            if (get_random_u32() % 2) {
                ret = wifi67_mac_set_link_state(ctx->mac_dev, j, 1);
                if (ret == 0) {
                    ctx->link_states[j] = LINK_STATE_ENABLED;
                    atomic_inc(&ctx->active_links);
                }
            } else {
                ret = wifi67_mac_set_link_state(ctx->mac_dev, j, 0);
                if (ret == 0) {
                    ctx->link_states[j] = LINK_STATE_DISABLED;
                    atomic_dec(&ctx->active_links);
                }
            }
        }

        /* Ensure at least one link is active */
        active_links = atomic_read(&ctx->active_links);
        if (active_links == 0) {
            ret = wifi67_mac_set_link_state(ctx->mac_dev, 0, 1);
            if (ret == 0) {
                ctx->link_states[0] = LINK_STATE_ENABLED;
                atomic_inc(&ctx->active_links);
            }
        }

        /* Send test packets on all active links */
        for (j = 0; j < MLO_TEST_MAX_LINKS; j++) {
            if (ctx->link_states[j] != LINK_STATE_ENABLED)
                continue;

            skb = mlo_test_alloc_skb(ctx, j, MLO_TEST_BUFFER_SIZE);
            if (!skb) {
                pr_err("Failed to allocate SKB\n");
                return TEST_FAIL;
            }

            ret = wifi67_mac_tx(ctx->mac_dev, skb, j);
            if (ret) {
                pr_err("Failed to transmit on link %d: %d\n", j, ret);
                dev_kfree_skb(skb);
                return TEST_FAIL;
            }

            /* Wait for TX completion */
            if (!wait_for_completion_timeout(&ctx->tx_done,
                                           msecs_to_jiffies(MLO_TEST_TIMEOUT_MS))) {
                pr_err("Timeout waiting for TX completion on link %d\n", j);
                return TEST_FAIL;
            }
        }
    }

    pr_info("MLO stress test passed\n");
    return TEST_PASS;
}

/* Module initialization */
static int __init mlo_test_module_init(void)
{
    struct mlo_test_context *ctx;

    pr_info("Initializing MLO test module\n");

    /* Initialize test context */
    ctx = mlo_test_init();
    if (!ctx) {
        pr_err("Failed to initialize test context\n");
        return -ENOMEM;
    }

    /* Register test cases */
    REGISTER_TEST("mlo_setup", "Test MLO setup and initialization",
                 test_mlo_setup, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("mlo_link_management", "Test MLO link management",
                 test_mlo_link_management, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("mlo_data_transfer", "Test MLO data transfer",
                 test_mlo_data_transfer, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("mlo_stress", "Stress test MLO functionality",
                 test_mlo_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

/* Module cleanup */
static void __exit mlo_test_module_exit(void)
{
    struct test_results results;

    /* Get test results */
    get_test_results(&results);
    pr_info("MLO tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);

    /* Cleanup is handled by test framework */
    mlo_test_cleanup(NULL);
}

module_init(mlo_test_module_init);
module_exit(mlo_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 MLO Test Module");
MODULE_VERSION("1.0"); 