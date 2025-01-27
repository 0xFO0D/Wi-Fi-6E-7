#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/random.h>
#include "../firmware/fw_core.h"
#include "../firmware/fw_common.h"
#include "../firmware/fw_secure.h"
#include "../firmware/fw_encrypt.h"
#include "../firmware/fw_rollback.h"
#include "test_framework.h"

#define FW_TEST_ITERATIONS TEST_ITER_NORMAL
#define FW_TEST_SIZE (512 * 1024)  /* 512 KB test firmware */
#define FW_TEST_CHUNK_SIZE (4 * 1024)  /* 4 KB chunks */

struct fw_test_context {
    struct wifi67_fw_dev *fw;
    struct completion load_done;
    struct completion save_done;
    atomic_t load_count;
    atomic_t save_count;
    void *fw_buffer;
    size_t fw_size;
    u8 *chunk_buffer;
    bool secure_boot;
    bool encryption;
};

/* Test callback functions */
static void fw_test_load_callback(void *data, int status)
{
    struct fw_test_context *ctx = data;
    atomic_inc(&ctx->load_count);
    complete(&ctx->load_done);
}

static void fw_test_save_callback(void *data, int status)
{
    struct fw_test_context *ctx = data;
    atomic_inc(&ctx->save_count);
    complete(&ctx->save_done);
}

/* Test setup and cleanup */
static struct fw_test_context *fw_test_init(void)
{
    struct fw_test_context *ctx;
    struct wifi67_fw_ops ops = {
        .load_complete = fw_test_load_callback,
        .save_complete = fw_test_save_callback,
    };

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize completion structures */
    init_completion(&ctx->load_done);
    init_completion(&ctx->save_done);

    /* Allocate test buffers */
    ctx->fw_buffer = vmalloc(FW_TEST_SIZE);
    if (!ctx->fw_buffer)
        goto err_free_ctx;

    ctx->chunk_buffer = kmalloc(FW_TEST_CHUNK_SIZE, GFP_KERNEL);
    if (!ctx->chunk_buffer)
        goto err_free_fw;

    /* Initialize firmware device */
    ctx->fw = wifi67_fw_alloc(&ops, ctx);
    if (!ctx->fw)
        goto err_free_chunk;

    /* Query capabilities */
    ctx->secure_boot = wifi67_fw_has_secure_boot(ctx->fw);
    ctx->encryption = wifi67_fw_has_encryption(ctx->fw);

    return ctx;

err_free_chunk:
    kfree(ctx->chunk_buffer);
err_free_fw:
    vfree(ctx->fw_buffer);
err_free_ctx:
    kfree(ctx);
    return NULL;
}

static void fw_test_cleanup(struct fw_test_context *ctx)
{
    if (!ctx)
        return;

    wifi67_fw_free(ctx->fw);
    kfree(ctx->chunk_buffer);
    vfree(ctx->fw_buffer);
    kfree(ctx);
}

/* Helper functions */
static void fw_test_generate_firmware(struct fw_test_context *ctx)
{
    u32 *data = ctx->fw_buffer;
    int i;

    /* Generate random firmware data */
    get_random_bytes(ctx->fw_buffer, FW_TEST_SIZE);

    /* Add firmware header */
    data[0] = WIFI67_FW_MAGIC;
    data[1] = WIFI67_FW_VERSION;
    data[2] = FW_TEST_SIZE;
    data[3] = 0; /* Reserved */

    /* Calculate checksum */
    for (i = 4; i < FW_TEST_SIZE / 4; i++)
        data[3] ^= data[i];

    ctx->fw_size = FW_TEST_SIZE;
}

/* Test cases */
static int test_fw_basic_load(void *data)
{
    struct fw_test_context *ctx = data;
    int ret;

    /* Generate test firmware */
    fw_test_generate_firmware(ctx);

    /* Load firmware */
    ret = wifi67_fw_load(ctx->fw, ctx->fw_buffer, ctx->fw_size);
    TEST_ASSERT(ret == 0, "Failed to start firmware load");

    /* Wait for completion */
    if (!wait_for_completion_timeout(&ctx->load_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "Firmware load timeout");
    }

    TEST_ASSERT(atomic_read(&ctx->load_count) == 1,
                "Incorrect load completion count");

    /* Verify firmware */
    ret = wifi67_fw_verify(ctx->fw);
    TEST_ASSERT(ret == 0, "Firmware verification failed");

    TEST_PASS();
}

static int test_fw_chunked_load(void *data)
{
    struct fw_test_context *ctx = data;
    int i, chunks, ret;
    size_t offset = 0;

    /* Generate test firmware */
    fw_test_generate_firmware(ctx);
    chunks = (ctx->fw_size + FW_TEST_CHUNK_SIZE - 1) / FW_TEST_CHUNK_SIZE;

    /* Start chunked load */
    ret = wifi67_fw_start_load(ctx->fw, ctx->fw_size);
    TEST_ASSERT(ret == 0, "Failed to start chunked firmware load");

    /* Load firmware in chunks */
    for (i = 0; i < chunks; i++) {
        size_t chunk_size = min_t(size_t, FW_TEST_CHUNK_SIZE,
                                ctx->fw_size - offset);

        memcpy(ctx->chunk_buffer,
               ctx->fw_buffer + offset,
               chunk_size);

        ret = wifi67_fw_load_chunk(ctx->fw, ctx->chunk_buffer,
                                 offset, chunk_size);
        TEST_ASSERT(ret == 0, "Failed to load chunk %d", i);

        offset += chunk_size;
    }

    /* Finish load */
    ret = wifi67_fw_finish_load(ctx->fw);
    TEST_ASSERT(ret == 0, "Failed to finish firmware load");

    /* Wait for completion */
    if (!wait_for_completion_timeout(&ctx->load_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "Firmware load timeout");
    }

    /* Verify firmware */
    ret = wifi67_fw_verify(ctx->fw);
    TEST_ASSERT(ret == 0, "Firmware verification failed");

    TEST_PASS();
}

static int test_fw_secure_boot(void *data)
{
    struct fw_test_context *ctx = data;
    int ret;

    if (!ctx->secure_boot)
        TEST_SKIP("Secure boot not supported");

    /* Generate test firmware */
    fw_test_generate_firmware(ctx);

    /* Sign firmware */
    ret = wifi67_fw_sign(ctx->fw, ctx->fw_buffer, ctx->fw_size);
    TEST_ASSERT(ret == 0, "Failed to sign firmware");

    /* Load and verify signed firmware */
    ret = wifi67_fw_load(ctx->fw, ctx->fw_buffer, ctx->fw_size);
    TEST_ASSERT(ret == 0, "Failed to start firmware load");

    /* Wait for completion */
    if (!wait_for_completion_timeout(&ctx->load_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "Firmware load timeout");
    }

    /* Verify signature */
    ret = wifi67_fw_verify_signature(ctx->fw);
    TEST_ASSERT(ret == 0, "Signature verification failed");

    TEST_PASS();
}

static int test_fw_encryption(void *data)
{
    struct fw_test_context *ctx = data;
    void *encrypted_buffer;
    size_t encrypted_size;
    int ret;

    if (!ctx->encryption)
        TEST_SKIP("Firmware encryption not supported");

    /* Generate test firmware */
    fw_test_generate_firmware(ctx);

    /* Allocate encryption buffer */
    encrypted_buffer = vmalloc(FW_TEST_SIZE + WIFI67_FW_ENCRYPT_OVERHEAD);
    if (!encrypted_buffer)
        TEST_SKIP("Failed to allocate encryption buffer");

    /* Encrypt firmware */
    ret = wifi67_fw_encrypt(ctx->fw, ctx->fw_buffer, ctx->fw_size,
                          encrypted_buffer, &encrypted_size);
    TEST_ASSERT(ret == 0, "Failed to encrypt firmware");

    /* Load and decrypt firmware */
    ret = wifi67_fw_load(ctx->fw, encrypted_buffer, encrypted_size);
    TEST_ASSERT(ret == 0, "Failed to start firmware load");

    /* Wait for completion */
    if (!wait_for_completion_timeout(&ctx->load_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "Firmware load timeout");
    }

    /* Verify decrypted firmware */
    ret = wifi67_fw_verify(ctx->fw);
    TEST_ASSERT(ret == 0, "Firmware verification failed");

    vfree(encrypted_buffer);
    TEST_PASS();
}

static int test_fw_stress(void *data)
{
    struct fw_test_context *ctx = data;
    int i, ret;

    for (i = 0; i < TEST_ITER_STRESS; i++) {
        /* Generate new random firmware */
        fw_test_generate_firmware(ctx);

        if (ctx->secure_boot && (i % 2)) {
            /* Test with secure boot */
            ret = wifi67_fw_sign(ctx->fw, ctx->fw_buffer, ctx->fw_size);
            TEST_ASSERT(ret == 0, "Failed to sign firmware in iteration %d", i);
        }

        /* Load firmware */
        ret = wifi67_fw_load(ctx->fw, ctx->fw_buffer, ctx->fw_size);
        TEST_ASSERT(ret == 0, "Failed to start firmware load in iteration %d", i);

        /* Wait for completion */
        if (!wait_for_completion_timeout(&ctx->load_done,
                                       msecs_to_jiffies(TEST_TIMEOUT_SHORT))) {
            TEST_ASSERT(0, "Firmware load timeout in iteration %d", i);
        }

        /* Verify firmware */
        ret = wifi67_fw_verify(ctx->fw);
        TEST_ASSERT(ret == 0, "Firmware verification failed in iteration %d", i);

        if (ctx->secure_boot && (i % 2)) {
            ret = wifi67_fw_verify_signature(ctx->fw);
            TEST_ASSERT(ret == 0,
                       "Signature verification failed in iteration %d", i);
        }

        /* Reset completion for next iteration */
        reinit_completion(&ctx->load_done);
    }

    TEST_PASS();
}

/* Module initialization */
static int __init fw_test_module_init(void)
{
    struct fw_test_context *ctx;

    ctx = fw_test_init();
    if (!ctx)
        return -ENOMEM;

    /* Register test cases */
    REGISTER_TEST("fw_basic", "Test basic firmware loading",
                 test_fw_basic_load, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("fw_chunked", "Test chunked firmware loading",
                 test_fw_chunked_load, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("fw_secure", "Test secure boot",
                 test_fw_secure_boot, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("fw_encrypt", "Test firmware encryption",
                 test_fw_encryption, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("fw_stress", "Stress test firmware loading",
                 test_fw_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

static void __exit fw_test_module_exit(void)
{
    struct test_results results;
    get_test_results(&results);
    pr_info("Firmware tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);
    fw_test_cleanup(NULL); /* ctx is managed by test framework */
}

module_init(fw_test_module_init);
module_exit(fw_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 Firmware Test Module");
MODULE_VERSION("1.0"); 