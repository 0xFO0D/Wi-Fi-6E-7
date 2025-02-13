/*
 * WiFi 7 Rate Control Test Module
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include "../../mac/wifi7_rate.h"
#include "../../mac/wifi7_mac.h"
#include "../../mac/wifi7_mlo.h"
#include "test_framework.h"

/* Test device context */
struct rate_test_dev {
    struct wifi7_dev *dev;
    struct wifi7_rate_config config;
    struct wifi7_rate_table table;
    struct wifi7_rate_stats stats;
    struct sk_buff_head test_queue;
    bool initialized;
};

static struct rate_test_dev *test_dev;

/* Test case: Rate table initialization */
static int test_rate_table_init(void)
{
    struct wifi7_rate_table table;
    int i;

    TEST_START("Rate table initialization");

    /* Initialize rate table */
    memset(&table, 0, sizeof(table));
    init_rate_table(&table);

    /* Verify table parameters */
    TEST_ASSERT(table.max_mcs == WIFI7_RATE_MAX_MCS, "Invalid max MCS");
    TEST_ASSERT(table.max_nss == WIFI7_RATE_MAX_NSS, "Invalid max NSS");
    TEST_ASSERT(table.max_bw == WIFI7_RATE_MAX_BW, "Invalid max bandwidth");
    TEST_ASSERT(table.max_gi == WIFI7_RATE_MAX_GI, "Invalid max GI");

    /* Verify capabilities */
    TEST_ASSERT(table.capabilities & WIFI7_RATE_CAP_MCS_15, "MCS 15 not supported");
    TEST_ASSERT(table.capabilities & WIFI7_RATE_CAP_4K_QAM, "4K-QAM not supported");
    TEST_ASSERT(table.capabilities & WIFI7_RATE_CAP_320MHZ, "320MHz not supported");
    TEST_ASSERT(table.capabilities & WIFI7_RATE_CAP_16_SS, "16 SS not supported");

    /* Verify rate entries */
    for (i = 0; i <= table.max_mcs; i++) {
        struct wifi7_rate_entry *entry = &table.entries[i];
        TEST_ASSERT(entry->valid, "Invalid rate entry");
        TEST_ASSERT(entry->mcs == i, "Invalid MCS value");
        TEST_ASSERT(entry->nss > 0, "Invalid NSS value");
        TEST_ASSERT(entry->bitrate > 0, "Invalid bitrate");
    }

    TEST_END();
    return 0;
}

/* Test case: Rate selection algorithms */
static int test_rate_selection(void)
{
    struct wifi7_rate_config config;
    struct wifi7_rate_entry rate;
    struct sk_buff *skb;
    int ret;

    TEST_START("Rate selection algorithms");

    /* Initialize test device */
    memset(&config, 0, sizeof(config));
    config.algorithm = WIFI7_RATE_ALGO_MINSTREL;
    config.capabilities = WIFI7_RATE_CAP_MCS_15 | WIFI7_RATE_CAP_4K_QAM;
    config.max_retry = WIFI7_RATE_MAX_RETRY;
    config.update_interval = 100;

    ret = wifi7_rate_set_config(test_dev->dev, &config);
    TEST_ASSERT(ret == 0, "Failed to set rate config");

    /* Test Minstrel algorithm */
    skb = dev_alloc_skb(1500);
    TEST_ASSERT(skb != NULL, "Failed to allocate SKB");

    ret = wifi7_rate_select(test_dev->dev, skb, &rate);
    TEST_ASSERT(ret == 0, "Minstrel rate selection failed");
    TEST_ASSERT(rate.valid, "Invalid rate selected");

    /* Test PID algorithm */
    config.algorithm = WIFI7_RATE_ALGO_PID;
    ret = wifi7_rate_set_config(test_dev->dev, &config);
    TEST_ASSERT(ret == 0, "Failed to set PID config");

    ret = wifi7_rate_select(test_dev->dev, skb, &rate);
    TEST_ASSERT(ret == 0, "PID rate selection failed");
    TEST_ASSERT(rate.valid, "Invalid rate selected");

    /* Test ML algorithm */
    config.algorithm = WIFI7_RATE_ALGO_ML;
    ret = wifi7_rate_set_config(test_dev->dev, &config);
    TEST_ASSERT(ret == 0, "Failed to set ML config");

    ret = wifi7_rate_select(test_dev->dev, skb, &rate);
    TEST_ASSERT(ret == 0, "ML rate selection failed");
    TEST_ASSERT(rate.valid, "Invalid rate selected");

    dev_kfree_skb(skb);

    TEST_END();
    return 0;
}

/* Test case: Rate adaptation */
static int test_rate_adaptation(void)
{
    struct wifi7_rate_entry rate;
    struct wifi7_rate_stats stats;
    struct sk_buff *skb;
    int i, ret;

    TEST_START("Rate adaptation");

    /* Initialize test packet */
    skb = dev_alloc_skb(1500);
    TEST_ASSERT(skb != NULL, "Failed to allocate SKB");

    /* Test successful transmissions */
    for (i = 0; i < 100; i++) {
        ret = wifi7_rate_select(test_dev->dev, skb, &rate);
        TEST_ASSERT(ret == 0, "Rate selection failed");

        ret = wifi7_rate_update(test_dev->dev, skb, &rate, true);
        TEST_ASSERT(ret == 0, "Rate update failed");
    }

    /* Verify statistics */
    ret = wifi7_rate_get_stats(test_dev->dev, &stats);
    TEST_ASSERT(ret == 0, "Failed to get stats");
    TEST_ASSERT(stats.tx_packets == 100, "Invalid packet count");
    TEST_ASSERT(stats.tx_success == 100, "Invalid success count");

    /* Test failed transmissions */
    for (i = 0; i < 50; i++) {
        ret = wifi7_rate_select(test_dev->dev, skb, &rate);
        TEST_ASSERT(ret == 0, "Rate selection failed");

        ret = wifi7_rate_update(test_dev->dev, skb, &rate, false);
        TEST_ASSERT(ret == 0, "Rate update failed");
    }

    /* Verify updated statistics */
    ret = wifi7_rate_get_stats(test_dev->dev, &stats);
    TEST_ASSERT(ret == 0, "Failed to get stats");
    TEST_ASSERT(stats.tx_packets == 150, "Invalid packet count");
    TEST_ASSERT(stats.tx_failures == 50, "Invalid failure count");

    dev_kfree_skb(skb);

    TEST_END();
    return 0;
}

/* Test case: MLO integration */
static int test_mlo_integration(void)
{
    struct wifi7_rate_entry rates[WIFI7_MAX_LINKS];
    struct wifi7_mlo_metrics metrics;
    struct sk_buff *skb;
    int i, ret;

    TEST_START("MLO integration");

    /* Initialize test packet */
    skb = dev_alloc_skb(1500);
    TEST_ASSERT(skb != NULL, "Failed to allocate SKB");

    /* Test rate selection for each link */
    for (i = 0; i < WIFI7_MAX_LINKS; i++) {
        ret = wifi7_rate_select(test_dev->dev, skb, &rates[i]);
        TEST_ASSERT(ret == 0, "Rate selection failed for link %d", i);
        TEST_ASSERT(rates[i].valid, "Invalid rate for link %d", i);
    }

    /* Verify link metrics */
    ret = wifi7_mlo_get_metrics(test_dev->dev, &metrics);
    TEST_ASSERT(ret == 0, "Failed to get MLO metrics");

    dev_kfree_skb(skb);

    TEST_END();
    return 0;
}

/* Test case: Stress testing */
static int test_rate_stress(void)
{
    struct wifi7_rate_entry rate;
    struct sk_buff *skb;
    int i, ret;
    bool success;

    TEST_START("Rate stress testing");

    /* Initialize test packet */
    skb = dev_alloc_skb(1500);
    TEST_ASSERT(skb != NULL, "Failed to allocate SKB");

    /* Perform rapid rate selections and updates */
    for (i = 0; i < 1000; i++) {
        ret = wifi7_rate_select(test_dev->dev, skb, &rate);
        TEST_ASSERT(ret == 0, "Rate selection failed");

        success = (prandom_u32() % 2) == 0;
        ret = wifi7_rate_update(test_dev->dev, skb, &rate, success);
        TEST_ASSERT(ret == 0, "Rate update failed");
    }

    dev_kfree_skb(skb);

    TEST_END();
    return 0;
}

/* Module initialization */
static int __init rate_test_init(void)
{
    struct wifi7_dev *dev;
    int ret;

    pr_info("WiFi 7 Rate Control Test Module\n");

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
    ret = test_rate_table_init();
    if (ret)
        goto err_free_dev;

    ret = test_rate_selection();
    if (ret)
        goto err_free_dev;

    ret = test_rate_adaptation();
    if (ret)
        goto err_free_dev;

    ret = test_mlo_integration();
    if (ret)
        goto err_free_dev;

    ret = test_rate_stress();
    if (ret)
        goto err_free_dev;

    return 0;

err_free_dev:
    wifi7_free_dev(dev);
err_free:
    kfree(test_dev);
    return ret;
}

static void __exit rate_test_exit(void)
{
    if (!test_dev)
        return;

    /* Clean up test device */
    if (test_dev->dev)
        wifi7_free_dev(test_dev->dev);

    skb_queue_purge(&test_dev->test_queue);
    kfree(test_dev);

    pr_info("WiFi 7 Rate Control Test Module unloaded\n");
}

module_init(rate_test_init);
module_exit(rate_test_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Rate Control Test Module");
MODULE_VERSION("1.0"); 