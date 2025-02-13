/*
 * WiFi 7 QoS Test Module
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include "../../mac/wifi7_qos.h"
#include "../../mac/wifi7_mac.h"
#include "../../mac/wifi7_mlo.h"
#include "test_framework.h"

/* Test device context */
struct qos_test_dev {
    struct wifi7_dev *dev;
    struct wifi7_qos_config config;
    struct wifi7_qos_stats stats;
    struct sk_buff_head test_queue;
    bool initialized;
};

static struct qos_test_dev *test_dev;

/* Test case: QoS configuration */
static int test_qos_config(void)
{
    struct wifi7_qos_config config;
    int ret;

    TEST_START("QoS configuration");

    /* Initialize test configuration */
    memset(&config, 0, sizeof(config));
    config.capabilities = WIFI7_QOS_CAP_MULTI_TID |
                         WIFI7_QOS_CAP_MLO |
                         WIFI7_QOS_CAP_LATENCY;
    config.multi_tid = true;
    config.mlo_qos = true;
    config.dynamic_priority = true;
    config.max_queues = 8;
    config.default_tid = WIFI7_QOS_TID_BESTEFFORT;

    /* Set configuration */
    ret = wifi7_qos_set_config(test_dev->dev, &config);
    TEST_ASSERT(ret == 0, "Failed to set QoS config");

    /* Verify configuration */
    memset(&config, 0, sizeof(config));
    ret = wifi7_qos_get_config(test_dev->dev, &config);
    TEST_ASSERT(ret == 0, "Failed to get QoS config");
    TEST_ASSERT(config.multi_tid, "Multi-TID not enabled");
    TEST_ASSERT(config.mlo_qos, "MLO QoS not enabled");
    TEST_ASSERT(config.dynamic_priority, "Dynamic priority not enabled");
    TEST_ASSERT(config.max_queues == 8, "Invalid queue count");
    TEST_ASSERT(config.default_tid == WIFI7_QOS_TID_BESTEFFORT,
                "Invalid default TID");

    TEST_END();
    return 0;
}

/* Test case: Queue management */
static int test_queue_management(void)
{
    struct sk_buff *skb;
    int i, ret;

    TEST_START("Queue management");

    /* Test enqueue/dequeue for each TID */
    for (i = 0; i <= WIFI7_QOS_TID_MAX; i++) {
        /* Enqueue test packet */
        skb = dev_alloc_skb(1500);
        TEST_ASSERT(skb != NULL, "Failed to allocate SKB");

        ret = wifi7_qos_enqueue(test_dev->dev, skb, i);
        TEST_ASSERT(ret == 0, "Failed to enqueue packet for TID %d", i);

        /* Verify queue state */
        ret = wifi7_qos_start_queue(test_dev->dev, i);
        TEST_ASSERT(ret == 0, "Failed to start queue for TID %d", i);

        /* Dequeue test packet */
        skb = wifi7_qos_dequeue(test_dev->dev, i);
        TEST_ASSERT(skb != NULL, "Failed to dequeue packet for TID %d", i);
        dev_kfree_skb(skb);

        /* Stop queue */
        ret = wifi7_qos_stop_queue(test_dev->dev, i);
        TEST_ASSERT(ret == 0, "Failed to stop queue for TID %d", i);
    }

    TEST_END();
    return 0;
}

/* Test case: Statistics tracking */
static int test_statistics(void)
{
    struct wifi7_qos_stats stats;
    struct wifi7_qos_queue_stats queue_stats;
    struct sk_buff *skb;
    int i, ret;

    TEST_START("Statistics tracking");

    /* Generate test traffic */
    for (i = 0; i < 100; i++) {
        skb = dev_alloc_skb(1500);
        TEST_ASSERT(skb != NULL, "Failed to allocate SKB");

        ret = wifi7_qos_enqueue(test_dev->dev, skb,
                               WIFI7_QOS_TID_BESTEFFORT);
        TEST_ASSERT(ret == 0, "Failed to enqueue packet");

        skb = wifi7_qos_dequeue(test_dev->dev,
                               WIFI7_QOS_TID_BESTEFFORT);
        TEST_ASSERT(skb != NULL, "Failed to dequeue packet");
        dev_kfree_skb(skb);
    }

    /* Verify global statistics */
    ret = wifi7_qos_get_stats(test_dev->dev, &stats);
    TEST_ASSERT(ret == 0, "Failed to get QoS stats");
    TEST_ASSERT(stats.total_enqueued == 100, "Invalid enqueue count");
    TEST_ASSERT(stats.total_dequeued == 100, "Invalid dequeue count");

    /* Verify queue statistics */
    ret = wifi7_qos_get_queue_stats(test_dev->dev,
                                   WIFI7_QOS_TID_BESTEFFORT,
                                   &queue_stats);
    TEST_ASSERT(ret == 0, "Failed to get queue stats");
    TEST_ASSERT(queue_stats.enqueued == 100, "Invalid queue enqueue count");
    TEST_ASSERT(queue_stats.dequeued == 100, "Invalid queue dequeue count");

    /* Clear statistics */
    ret = wifi7_qos_clear_stats(test_dev->dev);
    TEST_ASSERT(ret == 0, "Failed to clear stats");

    /* Verify cleared statistics */
    ret = wifi7_qos_get_stats(test_dev->dev, &stats);
    TEST_ASSERT(ret == 0, "Failed to get QoS stats");
    TEST_ASSERT(stats.total_enqueued == 0, "Stats not cleared");
    TEST_ASSERT(stats.total_dequeued == 0, "Stats not cleared");

    TEST_END();
    return 0;
}

/* Test case: MLO integration */
static int test_mlo_integration(void)
{
    struct wifi7_qos_config config;
    struct sk_buff *skb;
    int i, ret;

    TEST_START("MLO integration");

    /* Enable MLO QoS */
    ret = wifi7_qos_get_config(test_dev->dev, &config);
    TEST_ASSERT(ret == 0, "Failed to get QoS config");

    config.mlo_qos = true;
    ret = wifi7_qos_set_config(test_dev->dev, &config);
    TEST_ASSERT(ret == 0, "Failed to set QoS config");

    /* Test traffic distribution */
    for (i = 0; i < 100; i++) {
        skb = dev_alloc_skb(1500);
        TEST_ASSERT(skb != NULL, "Failed to allocate SKB");

        ret = wifi7_qos_enqueue(test_dev->dev, skb,
                               WIFI7_QOS_TID_VIDEO);
        TEST_ASSERT(ret == 0, "Failed to enqueue packet");

        skb = wifi7_qos_dequeue(test_dev->dev,
                               WIFI7_QOS_TID_VIDEO);
        TEST_ASSERT(skb != NULL, "Failed to dequeue packet");
        dev_kfree_skb(skb);
    }

    TEST_END();
    return 0;
}

/* Test case: Stress testing */
static int test_qos_stress(void)
{
    struct sk_buff *skb;
    int i, ret;
    u8 tid;

    TEST_START("QoS stress testing");

    /* Perform rapid queue operations */
    for (i = 0; i < 1000; i++) {
        tid = prandom_u32() % (WIFI7_QOS_TID_MAX + 1);

        skb = dev_alloc_skb(1500);
        TEST_ASSERT(skb != NULL, "Failed to allocate SKB");

        ret = wifi7_qos_enqueue(test_dev->dev, skb, tid);
        TEST_ASSERT(ret == 0, "Failed to enqueue packet");

        if (i % 2 == 0) {
            ret = wifi7_qos_stop_queue(test_dev->dev, tid);
            TEST_ASSERT(ret == 0, "Failed to stop queue");
            ret = wifi7_qos_wake_queue(test_dev->dev, tid);
            TEST_ASSERT(ret == 0, "Failed to wake queue");
        }

        skb = wifi7_qos_dequeue(test_dev->dev, tid);
        TEST_ASSERT(skb != NULL, "Failed to dequeue packet");
        dev_kfree_skb(skb);
    }

    TEST_END();
    return 0;
}

/* Module initialization */
static int __init qos_test_init(void)
{
    struct wifi7_dev *dev;
    int ret;

    pr_info("WiFi 7 QoS Test Module\n");

    /* Allocate test device */
    test_dev = kzalloc(sizeof(*test_dev), GFP_KERNEL);
    if (!test_dev)
        return -ENOMEM;

    /* Initialize test device */
    dev = wifi7_alloc_dev(sizeof(*dev));
    if (!dev) {
        ret = -ENOMEM;
        goto err_free;
    }

    test_dev->dev = dev;
    skb_queue_head_init(&test_dev->test_queue);
    test_dev->initialized = true;

    /* Run test cases */
    ret = test_qos_config();
    if (ret)
        goto err_free_dev;

    ret = test_queue_management();
    if (ret)
        goto err_free_dev;

    ret = test_statistics();
    if (ret)
        goto err_free_dev;

    ret = test_mlo_integration();
    if (ret)
        goto err_free_dev;

    ret = test_qos_stress();
    if (ret)
        goto err_free_dev;

    return 0;

err_free_dev:
    wifi7_free_dev(dev);
err_free:
    kfree(test_dev);
    return ret;
}

static void __exit qos_test_exit(void)
{
    if (!test_dev)
        return;

    /* Clean up test device */
    if (test_dev->dev)
        wifi7_free_dev(test_dev->dev);

    skb_queue_purge(&test_dev->test_queue);
    kfree(test_dev);

    pr_info("WiFi 7 QoS Test Module unloaded\n");
}

module_init(qos_test_init);
module_exit(qos_test_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 QoS Test Module");
MODULE_VERSION("1.0"); 