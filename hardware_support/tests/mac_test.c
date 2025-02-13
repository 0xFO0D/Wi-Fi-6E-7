#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/ieee80211.h>
#include <linux/random.h>
#include "../mac/mac_core.h"
#include "test_framework.h"

#define MAC_TEST_PKT_SIZE 1500
#define MAC_TEST_ITERATIONS TEST_ITER_NORMAL
#define MAC_TEST_QUEUES 4
#define MAC_TEST_AMPDU_SIZE 64

struct mac_test_context {
    struct wifi67_mac_dev *mac;
    struct sk_buff_head tx_queue;
    struct sk_buff_head rx_queue;
    struct completion tx_done;
    struct completion rx_done;
    atomic_t tx_count;
    atomic_t rx_count;
    u8 mac_addr[ETH_ALEN];
    u8 peer_addr[ETH_ALEN];
    u16 aid;
    u8 tid;
    bool ampdu_enabled;
};

/* Test callback functions */
static void mac_test_tx_callback(struct sk_buff *skb, void *data)
{
    struct mac_test_context *ctx = data;
    atomic_inc(&ctx->tx_count);
    complete(&ctx->tx_done);
    dev_kfree_skb_any(skb);
}

static void mac_test_rx_callback(struct sk_buff *skb, void *data)
{
    struct mac_test_context *ctx = data;
    skb_queue_tail(&ctx->rx_queue, skb);
    atomic_inc(&ctx->rx_count);
    complete(&ctx->rx_done);
}

/* Test setup and cleanup */
static struct mac_test_context *mac_test_init(void)
{
    struct mac_test_context *ctx;
    struct wifi67_mac_ops ops = {
        .tx_done = mac_test_tx_callback,
        .rx = mac_test_rx_callback,
    };

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize queues and completion structures */
    skb_queue_head_init(&ctx->tx_queue);
    skb_queue_head_init(&ctx->rx_queue);
    init_completion(&ctx->tx_done);
    init_completion(&ctx->rx_done);

    /* Generate random MAC addresses */
    eth_random_addr(ctx->mac_addr);
    eth_random_addr(ctx->peer_addr);

    /* Initialize MAC device */
    ctx->mac = wifi67_mac_alloc(&ops, ctx);
    if (!ctx->mac) {
        kfree(ctx);
        return NULL;
    }

    /* Configure MAC device */
    wifi67_mac_set_addr(ctx->mac, ctx->mac_addr);
    ctx->aid = 1;
    ctx->tid = 0;

    return ctx;
}

static void mac_test_cleanup(struct mac_test_context *ctx)
{
    if (!ctx)
        return;

    wifi67_mac_free(ctx->mac);
    skb_queue_purge(&ctx->tx_queue);
    skb_queue_purge(&ctx->rx_queue);
    kfree(ctx);
}

/* Helper functions */
static struct sk_buff *mac_test_alloc_data_frame(struct mac_test_context *ctx,
                                                int len)
{
    struct sk_buff *skb;
    struct ieee80211_hdr *hdr;
    u8 *data;

    skb = dev_alloc_skb(len + IEEE80211_HDRLEN);
    if (!skb)
        return NULL;

    skb_reserve(skb, IEEE80211_HDRLEN);
    data = skb_put(skb, len);
    get_random_bytes(data, len);

    /* Add 802.11 header */
    hdr = (struct ieee80211_hdr *)skb_push(skb, IEEE80211_HDRLEN);
    memset(hdr, 0, IEEE80211_HDRLEN);
    hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA |
                                    IEEE80211_STYPE_QOS_DATA);
    memcpy(hdr->addr1, ctx->peer_addr, ETH_ALEN);
    memcpy(hdr->addr2, ctx->mac_addr, ETH_ALEN);
    memcpy(hdr->addr3, ctx->peer_addr, ETH_ALEN);
    *ieee80211_get_qos_ctl(hdr) = ctx->tid;

    return skb;
}

/* Test cases */
static int test_mac_single_frame(void *data)
{
    struct mac_test_context *ctx = data;
    struct sk_buff *skb;
    int ret;

    /* Allocate and transmit a single frame */
    skb = mac_test_alloc_data_frame(ctx, MAC_TEST_PKT_SIZE);
    TEST_ASSERT(skb != NULL, "Failed to allocate test frame");

    ret = wifi67_mac_tx(ctx->mac, skb);
    TEST_ASSERT(ret == 0, "Failed to transmit test frame");

    /* Wait for completion */
    if (!wait_for_completion_timeout(&ctx->tx_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "TX timeout");
    }

    TEST_ASSERT(atomic_read(&ctx->tx_count) == 1,
                "Incorrect TX completion count");

    TEST_PASS();
}

static int test_mac_ampdu(void *data)
{
    struct mac_test_context *ctx = data;
    struct sk_buff *skb;
    int i, ret;

    /* Enable A-MPDU */
    ret = wifi67_mac_ampdu_action(ctx->mac, IEEE80211_AMPDU_TX_START,
                                 ctx->peer_addr, ctx->tid, &ctx->aid, 0);
    TEST_ASSERT(ret == 0, "Failed to start A-MPDU session");

    /* Transmit multiple frames */
    for (i = 0; i < MAC_TEST_AMPDU_SIZE; i++) {
        skb = mac_test_alloc_data_frame(ctx, MAC_TEST_PKT_SIZE);
        TEST_ASSERT(skb != NULL, "Failed to allocate test frame %d", i);

        ret = wifi67_mac_tx(ctx->mac, skb);
        TEST_ASSERT(ret == 0, "Failed to transmit test frame %d", i);
    }

    /* Wait for all completions */
    for (i = 0; i < MAC_TEST_AMPDU_SIZE; i++) {
        if (!wait_for_completion_timeout(&ctx->tx_done,
                                       msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
            TEST_ASSERT(0, "TX timeout at frame %d", i);
        }
    }

    TEST_ASSERT(atomic_read(&ctx->tx_count) == MAC_TEST_AMPDU_SIZE,
                "Incorrect TX completion count");

    /* Stop A-MPDU */
    ret = wifi67_mac_ampdu_action(ctx->mac, IEEE80211_AMPDU_TX_STOP,
                                 ctx->peer_addr, ctx->tid, NULL, 0);
    TEST_ASSERT(ret == 0, "Failed to stop A-MPDU session");

    TEST_PASS();
}

static int test_mac_queues(void *data)
{
    struct mac_test_context *ctx = data;
    struct sk_buff *skb;
    int i, j, ret;
    u8 ac;

    /* Test each access category */
    for (ac = 0; ac < MAC_TEST_QUEUES; ac++) {
        ctx->tid = ac * 2; /* Map AC to TID */

        /* Fill queue */
        for (i = 0; i < TEST_ITER_NORMAL; i++) {
            skb = mac_test_alloc_data_frame(ctx, MAC_TEST_PKT_SIZE);
            TEST_ASSERT(skb != NULL,
                       "Failed to allocate test frame %d for AC %d",
                       i, ac);

            ret = wifi67_mac_tx(ctx->mac, skb);
            TEST_ASSERT(ret == 0,
                       "Failed to transmit test frame %d for AC %d",
                       i, ac);
        }

        /* Wait for completions */
        for (j = 0; j < TEST_ITER_NORMAL; j++) {
            if (!wait_for_completion_timeout(&ctx->tx_done,
                                           msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
                TEST_ASSERT(0, "TX timeout at frame %d for AC %d",
                           j, ac);
            }
        }
    }

    TEST_ASSERT(atomic_read(&ctx->tx_count) == TEST_ITER_NORMAL * MAC_TEST_QUEUES,
                "Incorrect TX completion count");

    TEST_PASS();
}

static int test_mac_stress(void *data)
{
    struct mac_test_context *ctx = data;
    struct sk_buff *skb;
    int i, j, ret;
    u8 ac;

    for (i = 0; i < TEST_ITER_STRESS; i++) {
        /* Randomly select access category */
        ac = prandom_u32() % MAC_TEST_QUEUES;
        ctx->tid = ac * 2;

        /* Allocate and transmit frame */
        skb = mac_test_alloc_data_frame(ctx, MAC_TEST_PKT_SIZE);
        TEST_ASSERT(skb != NULL, "Failed to allocate test frame %d", i);

        ret = wifi67_mac_tx(ctx->mac, skb);
        TEST_ASSERT(ret == 0, "Failed to transmit test frame %d", i);

        /* Wait for completion with shorter timeout */
        if (!wait_for_completion_timeout(&ctx->tx_done,
                                       msecs_to_jiffies(TEST_TIMEOUT_SHORT))) {
            TEST_ASSERT(0, "TX timeout at frame %d", i);
        }

        /* Reset completion for next iteration */
        reinit_completion(&ctx->tx_done);
    }

    TEST_ASSERT(atomic_read(&ctx->tx_count) == TEST_ITER_STRESS,
                "Incorrect TX completion count");

    TEST_PASS();
}

/* Module initialization */
static int __init mac_test_module_init(void)
{
    struct mac_test_context *ctx;

    ctx = mac_test_init();
    if (!ctx)
        return -ENOMEM;

    /* Register test cases */
    REGISTER_TEST("mac_single", "Test single frame transmission",
                 test_mac_single_frame, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("mac_ampdu", "Test A-MPDU transmission",
                 test_mac_ampdu, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("mac_queues", "Test MAC queue handling",
                 test_mac_queues, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("mac_stress", "Stress test MAC layer",
                 test_mac_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

static void __exit mac_test_module_exit(void)
{
    struct test_results results;
    get_test_results(&results);
    pr_info("MAC tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);
    mac_test_cleanup(NULL); /* ctx is managed by test framework */
}

module_init(mac_test_module_init);
module_exit(mac_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 MAC Test Module");
MODULE_VERSION("1.0"); 