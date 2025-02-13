#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/random.h>
#include "../dma/dma_core.h"
#include "test_framework.h"

#define DMA_TEST_BUFFER_SIZE TEST_BUFFER_LARGE
#define DMA_TEST_ITERATIONS  TEST_ITER_NORMAL
#define DMA_TEST_CHANNELS    4
#define DMA_TEST_SEGMENTS    8

struct dma_test_context {
    struct wifi67_dma_channel *channels[DMA_TEST_CHANNELS];
    void *tx_buffer;
    void *rx_buffer;
    dma_addr_t tx_dma;
    dma_addr_t rx_dma;
    struct scatterlist tx_sg[DMA_TEST_SEGMENTS];
    struct scatterlist rx_sg[DMA_TEST_SEGMENTS];
    struct completion tx_done;
    struct completion rx_done;
    atomic_t tx_count;
    atomic_t rx_count;
    bool use_sg;
    struct device *dev;
};

/* Test callback functions */
static void dma_test_tx_callback(void *data)
{
    struct dma_test_context *ctx = data;
    atomic_inc(&ctx->tx_count);
    complete(&ctx->tx_done);
}

static void dma_test_rx_callback(void *data)
{
    struct dma_test_context *ctx = data;
    atomic_inc(&ctx->rx_count);
    complete(&ctx->rx_done);
}

/* Test setup and cleanup */
static struct dma_test_context *dma_test_init(void)
{
    struct dma_test_context *ctx;
    int i;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize completion structures */
    init_completion(&ctx->tx_done);
    init_completion(&ctx->rx_done);

    /* Allocate DMA buffers */
    ctx->tx_buffer = dma_alloc_coherent(ctx->dev, DMA_TEST_BUFFER_SIZE,
                                       &ctx->tx_dma, GFP_KERNEL);
    if (!ctx->tx_buffer)
        goto err_free_ctx;

    ctx->rx_buffer = dma_alloc_coherent(ctx->dev, DMA_TEST_BUFFER_SIZE,
                                       &ctx->rx_dma, GFP_KERNEL);
    if (!ctx->rx_buffer)
        goto err_free_tx;

    /* Initialize scatter-gather lists */
    sg_init_table(ctx->tx_sg, DMA_TEST_SEGMENTS);
    sg_init_table(ctx->rx_sg, DMA_TEST_SEGMENTS);

    /* Setup DMA channels */
    for (i = 0; i < DMA_TEST_CHANNELS; i++) {
        ctx->channels[i] = wifi67_dma_channel_alloc();
        if (!ctx->channels[i])
            goto err_free_channels;
    }

    return ctx;

err_free_channels:
    while (--i >= 0)
        wifi67_dma_channel_free(ctx->channels[i]);
    dma_free_coherent(ctx->dev, DMA_TEST_BUFFER_SIZE,
                      ctx->rx_buffer, ctx->rx_dma);
err_free_tx:
    dma_free_coherent(ctx->dev, DMA_TEST_BUFFER_SIZE,
                      ctx->tx_buffer, ctx->tx_dma);
err_free_ctx:
    kfree(ctx);
    return NULL;
}

static void dma_test_cleanup(struct dma_test_context *ctx)
{
    int i;

    if (!ctx)
        return;

    for (i = 0; i < DMA_TEST_CHANNELS; i++)
        wifi67_dma_channel_free(ctx->channels[i]);

    dma_free_coherent(ctx->dev, DMA_TEST_BUFFER_SIZE,
                      ctx->rx_buffer, ctx->rx_dma);
    dma_free_coherent(ctx->dev, DMA_TEST_BUFFER_SIZE,
                      ctx->tx_buffer, ctx->tx_dma);
    kfree(ctx);
}

/* Test cases */
static int test_dma_single_transfer(void *data)
{
    struct dma_test_context *ctx = data;
    u8 *tx_buf = ctx->tx_buffer;
    u8 *rx_buf = ctx->rx_buffer;
    int i, ret;

    /* Fill TX buffer with random data */
    get_random_bytes(tx_buf, DMA_TEST_BUFFER_SIZE);

    /* Configure channels */
    ret = wifi67_dma_channel_config(ctx->channels[0],
                                   DMA_MEM_TO_DEV,
                                   dma_test_tx_callback,
                                   ctx);
    TEST_ASSERT(ret == 0, "Failed to configure TX channel");

    ret = wifi67_dma_channel_config(ctx->channels[1],
                                   DMA_DEV_TO_MEM,
                                   dma_test_rx_callback,
                                   ctx);
    TEST_ASSERT(ret == 0, "Failed to configure RX channel");

    /* Start transfer */
    ret = wifi67_dma_transfer(ctx->channels[0],
                             ctx->tx_dma,
                             DMA_TEST_BUFFER_SIZE);
    TEST_ASSERT(ret == 0, "Failed to start TX transfer");

    ret = wifi67_dma_transfer(ctx->channels[1],
                             ctx->rx_dma,
                             DMA_TEST_BUFFER_SIZE);
    TEST_ASSERT(ret == 0, "Failed to start RX transfer");

    /* Wait for completion */
    if (!wait_for_completion_timeout(&ctx->tx_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "TX transfer timeout");
    }

    if (!wait_for_completion_timeout(&ctx->rx_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "RX transfer timeout");
    }

    /* Verify data */
    for (i = 0; i < DMA_TEST_BUFFER_SIZE; i++) {
        if (tx_buf[i] != rx_buf[i]) {
            TEST_ASSERT(0, "Data mismatch at offset %d: %02x != %02x",
                       i, tx_buf[i], rx_buf[i]);
        }
    }

    TEST_PASS();
}

static int test_dma_scatter_gather(void *data)
{
    struct dma_test_context *ctx = data;
    u8 *tx_buf = ctx->tx_buffer;
    u8 *rx_buf = ctx->rx_buffer;
    int i, ret, seg_size;
    dma_addr_t addr;

    seg_size = DMA_TEST_BUFFER_SIZE / DMA_TEST_SEGMENTS;

    /* Fill TX buffer with random data */
    get_random_bytes(tx_buf, DMA_TEST_BUFFER_SIZE);

    /* Setup scatter-gather lists */
    addr = ctx->tx_dma;
    for (i = 0; i < DMA_TEST_SEGMENTS; i++) {
        sg_set_buf(&ctx->tx_sg[i], tx_buf + (i * seg_size),
                  seg_size);
        sg_dma_address(&ctx->tx_sg[i]) = addr;
        addr += seg_size;
    }

    addr = ctx->rx_dma;
    for (i = 0; i < DMA_TEST_SEGMENTS; i++) {
        sg_set_buf(&ctx->rx_sg[i], rx_buf + (i * seg_size),
                  seg_size);
        sg_dma_address(&ctx->rx_sg[i]) = addr;
        addr += seg_size;
    }

    /* Configure channels */
    ret = wifi67_dma_channel_config(ctx->channels[0],
                                   DMA_MEM_TO_DEV,
                                   dma_test_tx_callback,
                                   ctx);
    TEST_ASSERT(ret == 0, "Failed to configure TX channel");

    ret = wifi67_dma_channel_config(ctx->channels[1],
                                   DMA_DEV_TO_MEM,
                                   dma_test_rx_callback,
                                   ctx);
    TEST_ASSERT(ret == 0, "Failed to configure RX channel");

    /* Start scatter-gather transfer */
    ret = wifi67_dma_transfer_sg(ctx->channels[0],
                                ctx->tx_sg,
                                DMA_TEST_SEGMENTS);
    TEST_ASSERT(ret == 0, "Failed to start TX scatter-gather transfer");

    ret = wifi67_dma_transfer_sg(ctx->channels[1],
                                ctx->rx_sg,
                                DMA_TEST_SEGMENTS);
    TEST_ASSERT(ret == 0, "Failed to start RX scatter-gather transfer");

    /* Wait for completion */
    if (!wait_for_completion_timeout(&ctx->tx_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "TX scatter-gather transfer timeout");
    }

    if (!wait_for_completion_timeout(&ctx->rx_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "RX scatter-gather transfer timeout");
    }

    /* Verify data */
    for (i = 0; i < DMA_TEST_BUFFER_SIZE; i++) {
        if (tx_buf[i] != rx_buf[i]) {
            TEST_ASSERT(0, "Data mismatch at offset %d: %02x != %02x",
                       i, tx_buf[i], rx_buf[i]);
        }
    }

    TEST_PASS();
}

static int test_dma_stress(void *data)
{
    struct dma_test_context *ctx = data;
    int i, j, ret;
    u8 *tx_buf = ctx->tx_buffer;
    u8 *rx_buf = ctx->rx_buffer;

    for (i = 0; i < TEST_ITER_STRESS; i++) {
        /* Fill TX buffer with new random data */
        get_random_bytes(tx_buf, DMA_TEST_BUFFER_SIZE);

        /* Configure channels */
        ret = wifi67_dma_channel_config(ctx->channels[0],
                                       DMA_MEM_TO_DEV,
                                       dma_test_tx_callback,
                                       ctx);
        TEST_ASSERT(ret == 0, "Failed to configure TX channel");

        ret = wifi67_dma_channel_config(ctx->channels[1],
                                       DMA_DEV_TO_MEM,
                                       dma_test_rx_callback,
                                       ctx);
        TEST_ASSERT(ret == 0, "Failed to configure RX channel");

        /* Start transfer */
        ret = wifi67_dma_transfer(ctx->channels[0],
                                 ctx->tx_dma,
                                 DMA_TEST_BUFFER_SIZE);
        TEST_ASSERT(ret == 0, "Failed to start TX transfer");

        ret = wifi67_dma_transfer(ctx->channels[1],
                                 ctx->rx_dma,
                                 DMA_TEST_BUFFER_SIZE);
        TEST_ASSERT(ret == 0, "Failed to start RX transfer");

        /* Wait for completion */
        if (!wait_for_completion_timeout(&ctx->tx_done,
                                       msecs_to_jiffies(TEST_TIMEOUT_SHORT))) {
            TEST_ASSERT(0, "TX transfer timeout in iteration %d", i);
        }

        if (!wait_for_completion_timeout(&ctx->rx_done,
                                       msecs_to_jiffies(TEST_TIMEOUT_SHORT))) {
            TEST_ASSERT(0, "RX transfer timeout in iteration %d", i);
        }

        /* Verify data */
        for (j = 0; j < DMA_TEST_BUFFER_SIZE; j++) {
            if (tx_buf[j] != rx_buf[j]) {
                TEST_ASSERT(0, "Data mismatch at offset %d in iteration %d",
                           j, i);
            }
        }

        /* Reset completion for next iteration */
        reinit_completion(&ctx->tx_done);
        reinit_completion(&ctx->rx_done);
    }

    TEST_PASS();
}

/* Module initialization */
static int __init dma_test_module_init(void)
{
    struct dma_test_context *ctx;

    ctx = dma_test_init();
    if (!ctx)
        return -ENOMEM;

    /* Register test cases */
    REGISTER_TEST("dma_single", "Test single DMA transfer",
                 test_dma_single_transfer, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("dma_sg", "Test scatter-gather DMA transfer",
                 test_dma_scatter_gather, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("dma_stress", "Stress test DMA transfers",
                 test_dma_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

static void __exit dma_test_module_exit(void)
{
    struct test_results results;
    get_test_results(&results);
    pr_info("DMA tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);
    dma_test_cleanup(NULL); /* ctx is managed by test framework */
}

module_init(dma_test_module_init);
module_exit(dma_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 DMA Test Module");
MODULE_VERSION("1.0"); 