#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include "../include/dma/dma_core.h"
#include "../include/core/wifi67.h"

#define TEST_CHANNEL_ID 0
#define TEST_BUFFER_SIZE 4096
#define TEST_ITERATIONS 1000

static struct wifi67_priv *test_priv;
static void *test_buffer;
static int test_channel = TEST_CHANNEL_ID;
module_param(test_channel, int, 0644);
MODULE_PARM_DESC(test_channel, "DMA channel to test (default: 0)");

static int __init test_dma_init(void)
{
    struct wifi67_dma_stats stats;
    int ret, i;
    u32 len;

    pr_info("Starting DMA test module\n");

    /* Allocate test private structure */
    test_priv = kzalloc(sizeof(*test_priv), GFP_KERNEL);
    if (!test_priv)
        return -ENOMEM;

    /* Initialize DMA */
    ret = wifi67_dma_init(test_priv);
    if (ret) {
        pr_err("Failed to initialize DMA: %d\n", ret);
        goto err_free_priv;
    }

    /* Initialize test channel */
    ret = wifi67_dma_channel_init(test_priv, test_channel);
    if (ret) {
        pr_err("Failed to initialize channel %d: %d\n", test_channel, ret);
        goto err_dma_deinit;
    }

    /* Start channel */
    ret = wifi67_dma_channel_start(test_priv, test_channel);
    if (ret) {
        pr_err("Failed to start channel %d: %d\n", test_channel, ret);
        goto err_channel_deinit;
    }

    /* Allocate test buffer */
    test_buffer = kmalloc(TEST_BUFFER_SIZE, GFP_KERNEL | GFP_DMA);
    if (!test_buffer) {
        ret = -ENOMEM;
        goto err_channel_stop;
    }

    /* Run TX test */
    pr_info("Running TX test with %d iterations\n", TEST_ITERATIONS);
    for (i = 0; i < TEST_ITERATIONS; i++) {
        ret = wifi67_dma_ring_add_buffer(test_priv, test_channel, true,
                                        test_buffer, TEST_BUFFER_SIZE);
        if (ret) {
            pr_err("TX test failed at iteration %d: %d\n", i, ret);
            goto err_free_buffer;
        }
    }

    /* Run RX test */
    pr_info("Running RX test with %d iterations\n", TEST_ITERATIONS);
    for (i = 0; i < TEST_ITERATIONS; i++) {
        void *buf = wifi67_dma_ring_get_buffer(test_priv, test_channel,
                                              true, &len);
        if (!buf) {
            pr_err("RX test failed at iteration %d\n", i);
            goto err_free_buffer;
        }
        if (len != TEST_BUFFER_SIZE) {
            pr_err("RX size mismatch: expected %d, got %d\n",
                   TEST_BUFFER_SIZE, len);
            goto err_free_buffer;
        }
    }

    /* Get and print statistics */
    ret = wifi67_dma_get_stats(test_priv, &stats);
    if (ret) {
        pr_err("Failed to get DMA stats: %d\n", ret);
        goto err_free_buffer;
    }

    pr_info("DMA Test Results:\n");
    pr_info("TX packets: %llu\n", stats.tx_packets);
    pr_info("TX bytes: %llu\n", stats.tx_bytes);
    pr_info("TX errors: %llu\n", stats.tx_errors);
    pr_info("RX packets: %llu\n", stats.rx_packets);
    pr_info("RX bytes: %llu\n", stats.rx_bytes);
    pr_info("RX errors: %llu\n", stats.rx_errors);
    pr_info("Ring full events: %u\n", stats.ring_full);
    pr_info("Buffer errors: %u\n", stats.buf_errors);

    return 0;

err_free_buffer:
    kfree(test_buffer);
err_channel_stop:
    wifi67_dma_channel_stop(test_priv, test_channel);
err_channel_deinit:
    wifi67_dma_channel_deinit(test_priv, test_channel);
err_dma_deinit:
    wifi67_dma_deinit(test_priv);
err_free_priv:
    kfree(test_priv);
    return ret;
}

static void __exit test_dma_exit(void)
{
    pr_info("Cleaning up DMA test module\n");

    kfree(test_buffer);
    wifi67_dma_channel_stop(test_priv, test_channel);
    wifi67_dma_channel_deinit(test_priv, test_channel);
    wifi67_dma_deinit(test_priv);
    kfree(test_priv);
}

module_init(test_dma_init);
module_exit(test_dma_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("WiFi 6E/7 DMA Test Module");
MODULE_VERSION("1.0"); 