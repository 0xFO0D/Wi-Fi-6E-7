/*
 * WiFi 7 Automotive Components Test Module
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include "../../automotive/v2x/wifi7_v2x.h"
#include "../../automotive/can/wifi7_can.h"
#include "../../automotive/signal/wifi7_auto_signal.h"
#include "test_framework.h"

/* Test device context */
struct auto_test_dev {
    struct wifi7_dev *dev;
    struct wifi7_v2x_config v2x_config;
    struct wifi7_can_config can_config;
    struct wifi7_auto_signal_config signal_config;
    struct sk_buff_head test_queue;
    bool initialized;
};

static struct auto_test_dev *test_dev;

/* V2X Communication Tests */
static int test_v2x_config(void)
{
    struct wifi7_v2x_config config = {
        .mode = WIFI7_V2X_MODE_HYBRID,
        .low_latency = true,
        .congestion_control = true,
        .security_enabled = true,
        .max_range = 500,
        .channel_interval = 100,
        .max_retries = 3,
        .intervals = {
            .emergency = 50,
            .safety = 100,
            .mobility = 200,
            .info = 500,
        },
    };
    int ret;

    TEST_START("V2X Configuration Test");

    ret = wifi7_v2x_set_config(test_dev->dev, &config);
    TEST_ASSERT(ret == 0, "Failed to set V2X config");

    memset(&config, 0, sizeof(config));
    ret = wifi7_v2x_get_config(test_dev->dev, &config);
    TEST_ASSERT(ret == 0, "Failed to get V2X config");
    TEST_ASSERT(config.mode == WIFI7_V2X_MODE_HYBRID, "Invalid mode");
    TEST_ASSERT(config.low_latency, "Low latency not enabled");
    TEST_ASSERT(config.security_enabled, "Security not enabled");

    TEST_END();
    return 0;
}

static int test_v2x_messaging(void)
{
    struct sk_buff *skb;
    struct wifi7_v2x_stats stats;
    int ret;

    TEST_START("V2X Messaging Test");

    /* Test emergency message */
    skb = dev_alloc_skb(256);
    TEST_ASSERT(skb != NULL, "Failed to allocate SKB");
    skb_put(skb, 256);
    ret = wifi7_v2x_send_msg(test_dev->dev, skb, WIFI7_V2X_MSG_EVA, 
                            WIFI7_V2X_PRIO_EMERGENCY);
    TEST_ASSERT(ret == 0, "Failed to send emergency message");

    /* Test safety message */
    skb = dev_alloc_skb(256);
    TEST_ASSERT(skb != NULL, "Failed to allocate SKB");
    skb_put(skb, 256);
    ret = wifi7_v2x_send_msg(test_dev->dev, skb, WIFI7_V2X_MSG_BSM,
                            WIFI7_V2X_PRIO_SAFETY);
    TEST_ASSERT(ret == 0, "Failed to send safety message");

    /* Verify statistics */
    ret = wifi7_v2x_get_stats(test_dev->dev, &stats);
    TEST_ASSERT(ret == 0, "Failed to get V2X stats");
    TEST_ASSERT(stats.msgs_tx >= 2, "Message count mismatch");

    TEST_END();
    return 0;
}

/* CAN Integration Tests */
static int test_can_config(void)
{
    struct wifi7_can_config config = {
        .enabled = true,
        .bitrate = 500000,
        .sjw = 1,
        .tseg1 = 6,
        .tseg2 = 3,
        .listen_only = false,
        .loopback = false,
        .one_shot = false,
        .berr_reporting = true,
        .queue = {
            .rx_queue_size = 1024,
            .tx_queue_size = 1024,
            .timeout = 100,
        },
    };
    int ret;

    TEST_START("CAN Configuration Test");

    ret = wifi7_can_set_config(test_dev->dev, &config);
    TEST_ASSERT(ret == 0, "Failed to set CAN config");

    memset(&config, 0, sizeof(config));
    ret = wifi7_can_get_config(test_dev->dev, &config);
    TEST_ASSERT(ret == 0, "Failed to get CAN config");
    TEST_ASSERT(config.enabled, "CAN not enabled");
    TEST_ASSERT(config.bitrate == 500000, "Invalid bitrate");
    TEST_ASSERT(config.berr_reporting, "Error reporting not enabled");

    TEST_END();
    return 0;
}

static int test_can_frame_handling(void)
{
    struct can_frame frame;
    struct wifi7_can_stats stats;
    int ret;

    TEST_START("CAN Frame Handling Test");

    /* Test frame transmission */
    memset(&frame, 0, sizeof(frame));
    frame.can_id = 0x123;
    frame.can_dlc = 8;
    memset(frame.data, 0xAA, 8);

    ret = wifi7_can_send_frame(test_dev->dev, &frame, WIFI7_CAN_PRIO_HIGH);
    TEST_ASSERT(ret == 0, "Failed to send CAN frame");

    /* Test frame reception */
    memset(&frame, 0, sizeof(frame));
    ret = wifi7_can_recv_frame(test_dev->dev, &frame);
    TEST_ASSERT(ret == 0, "Failed to receive CAN frame");

    /* Verify statistics */
    ret = wifi7_can_get_stats(test_dev->dev, &stats);
    TEST_ASSERT(ret == 0, "Failed to get CAN stats");
    TEST_ASSERT(stats.frames_tx >= 1, "TX frame count mismatch");
    TEST_ASSERT(stats.frames_rx >= 1, "RX frame count mismatch");

    TEST_END();
    return 0;
}

/* Signal Management Tests */
static int test_signal_config(void)
{
    struct wifi7_auto_signal_config config = {
        .environment = WIFI7_ENV_URBAN,
        .interference_mask = WIFI7_INTERFERENCE_EMI | WIFI7_INTERFERENCE_METAL,
        .adaptive_power = true,
        .beam_forming = true,
        .mimo_optimize = true,
        .min_rssi = -75,
        .max_retry = 5,
        .intervals = {
            .scan_interval = 1000,
            .adapt_interval = 2000,
            .report_interval = 5000,
        },
        .radio = {
            .power_min = 0,
            .power_max = 20,
            .power_step = 2,
            .spatial_streams = 2,
        },
    };
    int ret;

    TEST_START("Signal Configuration Test");

    ret = wifi7_auto_signal_set_config(test_dev->dev, &config);
    TEST_ASSERT(ret == 0, "Failed to set signal config");

    memset(&config, 0, sizeof(config));
    ret = wifi7_auto_signal_get_config(test_dev->dev, &config);
    TEST_ASSERT(ret == 0, "Failed to get signal config");
    TEST_ASSERT(config.environment == WIFI7_ENV_URBAN, "Invalid environment");
    TEST_ASSERT(config.adaptive_power, "Adaptive power not enabled");
    TEST_ASSERT(config.beam_forming, "Beam forming not enabled");

    TEST_END();
    return 0;
}

static int test_signal_adaptation(void)
{
    struct wifi7_signal_metrics metrics;
    struct wifi7_auto_signal_stats stats;
    int ret;

    TEST_START("Signal Adaptation Test");

    /* Get initial metrics */
    ret = wifi7_auto_signal_get_metrics(test_dev->dev, &metrics);
    TEST_ASSERT(ret == 0, "Failed to get signal metrics");

    /* Force adaptation */
    ret = wifi7_auto_signal_force_adapt(test_dev->dev);
    TEST_ASSERT(ret == 0, "Failed to force adaptation");

    /* Verify adaptation occurred */
    ret = wifi7_auto_signal_get_stats(test_dev->dev, &stats);
    TEST_ASSERT(ret == 0, "Failed to get signal stats");
    TEST_ASSERT(stats.adaptations >= 1, "No adaptation recorded");

    TEST_END();
    return 0;
}

static int test_automotive_integration(void)
{
    struct wifi7_v2x_stats v2x_stats;
    struct wifi7_can_stats can_stats;
    struct wifi7_auto_signal_stats signal_stats;
    struct sk_buff *skb;
    struct can_frame frame;
    int ret;

    TEST_START("Automotive Integration Test");

    /* Send V2X message */
    skb = dev_alloc_skb(256);
    TEST_ASSERT(skb != NULL, "Failed to allocate SKB");
    skb_put(skb, 256);
    ret = wifi7_v2x_send_msg(test_dev->dev, skb, WIFI7_V2X_MSG_BSM,
                            WIFI7_V2X_PRIO_SAFETY);
    TEST_ASSERT(ret == 0, "Failed to send V2X message");

    /* Send CAN frame */
    memset(&frame, 0, sizeof(frame));
    frame.can_id = 0x123;
    frame.can_dlc = 8;
    memset(frame.data, 0xAA, 8);
    ret = wifi7_can_send_frame(test_dev->dev, &frame, WIFI7_CAN_PRIO_HIGH);
    TEST_ASSERT(ret == 0, "Failed to send CAN frame");

    /* Force signal adaptation */
    ret = wifi7_auto_signal_force_adapt(test_dev->dev);
    TEST_ASSERT(ret == 0, "Failed to force adaptation");

    /* Verify all components working */
    ret = wifi7_v2x_get_stats(test_dev->dev, &v2x_stats);
    TEST_ASSERT(ret == 0, "Failed to get V2X stats");
    TEST_ASSERT(v2x_stats.msgs_tx >= 1, "V2X message not sent");

    ret = wifi7_can_get_stats(test_dev->dev, &can_stats);
    TEST_ASSERT(ret == 0, "Failed to get CAN stats");
    TEST_ASSERT(can_stats.frames_tx >= 1, "CAN frame not sent");

    ret = wifi7_auto_signal_get_stats(test_dev->dev, &signal_stats);
    TEST_ASSERT(ret == 0, "Failed to get signal stats");
    TEST_ASSERT(signal_stats.adaptations >= 1, "Signal not adapted");

    TEST_END();
    return 0;
}

static int test_automotive_stress(void)
{
    struct sk_buff *skb;
    struct can_frame frame;
    int i, ret;

    TEST_START("Automotive Stress Test");

    /* Rapid V2X messaging */
    for (i = 0; i < 100; i++) {
        skb = dev_alloc_skb(256);
        TEST_ASSERT(skb != NULL, "Failed to allocate SKB");
        skb_put(skb, 256);
        ret = wifi7_v2x_send_msg(test_dev->dev, skb, WIFI7_V2X_MSG_BSM,
                                WIFI7_V2X_PRIO_SAFETY);
        TEST_ASSERT(ret == 0, "V2X message %d failed", i);
    }

    /* Rapid CAN frame transmission */
    for (i = 0; i < 100; i++) {
        memset(&frame, 0, sizeof(frame));
        frame.can_id = i;
        frame.can_dlc = 8;
        memset(frame.data, i, 8);
        ret = wifi7_can_send_frame(test_dev->dev, &frame, WIFI7_CAN_PRIO_HIGH);
        TEST_ASSERT(ret == 0, "CAN frame %d failed", i);
    }

    /* Rapid signal adaptations */
    for (i = 0; i < 10; i++) {
        ret = wifi7_auto_signal_force_adapt(test_dev->dev);
        TEST_ASSERT(ret == 0, "Signal adaptation %d failed", i);
    }

    TEST_END();
    return 0;
}

static int __init auto_test_init(void)
{
    struct wifi7_dev *dev;
    int ret;

    printk(KERN_INFO "WiFi 7 Automotive Test Module\n");

    /* Allocate test device */
    test_dev = kzalloc(sizeof(*test_dev), GFP_KERNEL);
    if (!test_dev)
        return -ENOMEM;

    /* Initialize core device */
    dev = wifi7_alloc_dev(sizeof(*dev));
    if (!dev) {
        ret = -ENOMEM;
        goto err_free_test;
    }

    test_dev->dev = dev;
    skb_queue_head_init(&test_dev->test_queue);

    /* Run test cases */
    ret = test_v2x_config();
    if (ret)
        goto err_free_dev;

    ret = test_v2x_messaging();
    if (ret)
        goto err_free_dev;

    ret = test_can_config();
    if (ret)
        goto err_free_dev;

    ret = test_can_frame_handling();
    if (ret)
        goto err_free_dev;

    ret = test_signal_config();
    if (ret)
        goto err_free_dev;

    ret = test_signal_adaptation();
    if (ret)
        goto err_free_dev;

    ret = test_automotive_integration();
    if (ret)
        goto err_free_dev;

    ret = test_automotive_stress();
    if (ret)
        goto err_free_dev;

    test_dev->initialized = true;
    return 0;

err_free_dev:
    wifi7_free_dev(dev);
err_free_test:
    kfree(test_dev);
    return ret;
}

static void __exit auto_test_exit(void)
{
    if (!test_dev)
        return;

    if (test_dev->initialized) {
        skb_queue_purge(&test_dev->test_queue);
        wifi7_free_dev(test_dev->dev);
    }

    kfree(test_dev);
    printk(KERN_INFO "WiFi 7 Automotive Test Module unloaded\n");
}

module_init(auto_test_init);
module_exit(auto_test_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Automotive Components Test Module");
MODULE_VERSION("1.0"); 