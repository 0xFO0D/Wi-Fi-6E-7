#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/random.h>
#include <crypto/aead.h>
#include <crypto/skcipher.h>
#include <crypto/hash.h>
#include "../crypto/crypto_core.h"
#include "test_framework.h"

#define CRYPTO_TEST_ITERATIONS TEST_ITER_NORMAL
#define CRYPTO_TEST_DATA_SIZE 2048
#define CRYPTO_TEST_KEY_SIZE 32
#define CRYPTO_TEST_IV_SIZE 16
#define CRYPTO_TEST_AAD_SIZE 20
#define CRYPTO_TEST_AUTH_SIZE 16

struct crypto_test_context {
    struct wifi67_crypto_dev *crypto;
    struct completion enc_done;
    struct completion dec_done;
    atomic_t enc_count;
    atomic_t dec_count;
    void *test_data;
    void *enc_data;
    void *dec_data;
    u8 key[CRYPTO_TEST_KEY_SIZE];
    u8 iv[CRYPTO_TEST_IV_SIZE];
    u8 aad[CRYPTO_TEST_AAD_SIZE];
    bool hw_crypto;
};

/* Test callback functions */
static void crypto_test_enc_callback(void *data, int status)
{
    struct crypto_test_context *ctx = data;
    atomic_inc(&ctx->enc_count);
    complete(&ctx->enc_done);
}

static void crypto_test_dec_callback(void *data, int status)
{
    struct crypto_test_context *ctx = data;
    atomic_inc(&ctx->dec_count);
    complete(&ctx->dec_done);
}

/* Test setup and cleanup */
static struct crypto_test_context *crypto_test_init(void)
{
    struct crypto_test_context *ctx;
    struct wifi67_crypto_ops ops = {
        .encrypt_complete = crypto_test_enc_callback,
        .decrypt_complete = crypto_test_dec_callback,
    };

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize completion structures */
    init_completion(&ctx->enc_done);
    init_completion(&ctx->dec_done);

    /* Allocate test buffers */
    ctx->test_data = kmalloc(CRYPTO_TEST_DATA_SIZE, GFP_KERNEL);
    if (!ctx->test_data)
        goto err_free_ctx;

    ctx->enc_data = kmalloc(CRYPTO_TEST_DATA_SIZE + CRYPTO_TEST_AUTH_SIZE,
                           GFP_KERNEL);
    if (!ctx->enc_data)
        goto err_free_test;

    ctx->dec_data = kmalloc(CRYPTO_TEST_DATA_SIZE, GFP_KERNEL);
    if (!ctx->dec_data)
        goto err_free_enc;

    /* Initialize crypto device */
    ctx->crypto = wifi67_crypto_alloc(&ops, ctx);
    if (!ctx->crypto)
        goto err_free_dec;

    /* Generate random key and IV */
    get_random_bytes(ctx->key, CRYPTO_TEST_KEY_SIZE);
    get_random_bytes(ctx->iv, CRYPTO_TEST_IV_SIZE);
    get_random_bytes(ctx->aad, CRYPTO_TEST_AAD_SIZE);

    /* Check if hardware crypto is available */
    ctx->hw_crypto = wifi67_crypto_hw_available(ctx->crypto);

    return ctx;

err_free_dec:
    kfree(ctx->dec_data);
err_free_enc:
    kfree(ctx->enc_data);
err_free_test:
    kfree(ctx->test_data);
err_free_ctx:
    kfree(ctx);
    return NULL;
}

static void crypto_test_cleanup(struct crypto_test_context *ctx)
{
    if (!ctx)
        return;

    wifi67_crypto_free(ctx->crypto);
    kfree(ctx->dec_data);
    kfree(ctx->enc_data);
    kfree(ctx->test_data);
    kfree(ctx);
}

/* Test cases */
static int test_crypto_gcm(void *data)
{
    struct crypto_test_context *ctx = data;
    int ret;

    /* Generate random test data */
    get_random_bytes(ctx->test_data, CRYPTO_TEST_DATA_SIZE);

    /* Configure GCM mode */
    ret = wifi67_crypto_config(ctx->crypto, WIFI67_CRYPTO_AES_GCM,
                             ctx->key, CRYPTO_TEST_KEY_SIZE);
    TEST_ASSERT(ret == 0, "Failed to configure GCM mode");

    /* Encrypt data */
    ret = wifi67_crypto_encrypt(ctx->crypto,
                              ctx->test_data, CRYPTO_TEST_DATA_SIZE,
                              ctx->iv, CRYPTO_TEST_IV_SIZE,
                              ctx->aad, CRYPTO_TEST_AAD_SIZE,
                              ctx->enc_data);
    TEST_ASSERT(ret == 0, "Failed to start encryption");

    /* Wait for encryption completion */
    if (!wait_for_completion_timeout(&ctx->enc_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "Encryption timeout");
    }

    /* Decrypt data */
    ret = wifi67_crypto_decrypt(ctx->crypto,
                              ctx->enc_data,
                              CRYPTO_TEST_DATA_SIZE + CRYPTO_TEST_AUTH_SIZE,
                              ctx->iv, CRYPTO_TEST_IV_SIZE,
                              ctx->aad, CRYPTO_TEST_AAD_SIZE,
                              ctx->dec_data);
    TEST_ASSERT(ret == 0, "Failed to start decryption");

    /* Wait for decryption completion */
    if (!wait_for_completion_timeout(&ctx->dec_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "Decryption timeout");
    }

    /* Verify decrypted data */
    TEST_ASSERT(memcmp(ctx->test_data, ctx->dec_data,
                      CRYPTO_TEST_DATA_SIZE) == 0,
                "Decrypted data mismatch");

    TEST_PASS();
}

static int test_crypto_ccm(void *data)
{
    struct crypto_test_context *ctx = data;
    int ret;

    /* Generate random test data */
    get_random_bytes(ctx->test_data, CRYPTO_TEST_DATA_SIZE);

    /* Configure CCM mode */
    ret = wifi67_crypto_config(ctx->crypto, WIFI67_CRYPTO_AES_CCM,
                             ctx->key, CRYPTO_TEST_KEY_SIZE);
    TEST_ASSERT(ret == 0, "Failed to configure CCM mode");

    /* Encrypt data */
    ret = wifi67_crypto_encrypt(ctx->crypto,
                              ctx->test_data, CRYPTO_TEST_DATA_SIZE,
                              ctx->iv, CRYPTO_TEST_IV_SIZE,
                              ctx->aad, CRYPTO_TEST_AAD_SIZE,
                              ctx->enc_data);
    TEST_ASSERT(ret == 0, "Failed to start encryption");

    /* Wait for encryption completion */
    if (!wait_for_completion_timeout(&ctx->enc_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "Encryption timeout");
    }

    /* Decrypt data */
    ret = wifi67_crypto_decrypt(ctx->crypto,
                              ctx->enc_data,
                              CRYPTO_TEST_DATA_SIZE + CRYPTO_TEST_AUTH_SIZE,
                              ctx->iv, CRYPTO_TEST_IV_SIZE,
                              ctx->aad, CRYPTO_TEST_AAD_SIZE,
                              ctx->dec_data);
    TEST_ASSERT(ret == 0, "Failed to start decryption");

    /* Wait for decryption completion */
    if (!wait_for_completion_timeout(&ctx->dec_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "Decryption timeout");
    }

    /* Verify decrypted data */
    TEST_ASSERT(memcmp(ctx->test_data, ctx->dec_data,
                      CRYPTO_TEST_DATA_SIZE) == 0,
                "Decrypted data mismatch");

    TEST_PASS();
}

static int test_crypto_cmac(void *data)
{
    struct crypto_test_context *ctx = data;
    u8 mac1[CRYPTO_TEST_AUTH_SIZE];
    u8 mac2[CRYPTO_TEST_AUTH_SIZE];
    int ret;

    /* Generate random test data */
    get_random_bytes(ctx->test_data, CRYPTO_TEST_DATA_SIZE);

    /* Configure CMAC mode */
    ret = wifi67_crypto_config(ctx->crypto, WIFI67_CRYPTO_AES_CMAC,
                             ctx->key, CRYPTO_TEST_KEY_SIZE);
    TEST_ASSERT(ret == 0, "Failed to configure CMAC mode");

    /* Calculate MAC */
    ret = wifi67_crypto_mac(ctx->crypto,
                          ctx->test_data, CRYPTO_TEST_DATA_SIZE,
                          mac1);
    TEST_ASSERT(ret == 0, "Failed to calculate first MAC");

    /* Calculate MAC again */
    ret = wifi67_crypto_mac(ctx->crypto,
                          ctx->test_data, CRYPTO_TEST_DATA_SIZE,
                          mac2);
    TEST_ASSERT(ret == 0, "Failed to calculate second MAC");

    /* Verify MACs match */
    TEST_ASSERT(memcmp(mac1, mac2, CRYPTO_TEST_AUTH_SIZE) == 0,
                "MAC mismatch");

    TEST_PASS();
}

static int test_crypto_stress(void *data)
{
    struct crypto_test_context *ctx = data;
    int i, ret;
    u8 mode;

    for (i = 0; i < TEST_ITER_STRESS; i++) {
        /* Generate new random test data */
        get_random_bytes(ctx->test_data, CRYPTO_TEST_DATA_SIZE);

        /* Randomly select crypto mode */
        mode = (prandom_u32() % 3);
        switch (mode) {
        case 0:
            mode = WIFI67_CRYPTO_AES_GCM;
            break;
        case 1:
            mode = WIFI67_CRYPTO_AES_CCM;
            break;
        case 2:
            mode = WIFI67_CRYPTO_AES_CMAC;
            break;
        }

        /* Configure crypto mode */
        ret = wifi67_crypto_config(ctx->crypto, mode,
                                ctx->key, CRYPTO_TEST_KEY_SIZE);
        TEST_ASSERT(ret == 0, "Failed to configure mode %u in iteration %d",
                   mode, i);

        if (mode == WIFI67_CRYPTO_AES_CMAC) {
            /* Test MAC calculation */
            ret = wifi67_crypto_mac(ctx->crypto,
                                 ctx->test_data, CRYPTO_TEST_DATA_SIZE,
                                 ctx->enc_data);
            TEST_ASSERT(ret == 0, "MAC failed in iteration %d", i);
        } else {
            /* Test encryption/decryption */
            ret = wifi67_crypto_encrypt(ctx->crypto,
                                     ctx->test_data, CRYPTO_TEST_DATA_SIZE,
                                     ctx->iv, CRYPTO_TEST_IV_SIZE,
                                     ctx->aad, CRYPTO_TEST_AAD_SIZE,
                                     ctx->enc_data);
            TEST_ASSERT(ret == 0, "Encryption failed in iteration %d", i);

            /* Wait for encryption completion */
            if (!wait_for_completion_timeout(&ctx->enc_done,
                                           msecs_to_jiffies(TEST_TIMEOUT_SHORT))) {
                TEST_ASSERT(0, "Encryption timeout in iteration %d", i);
            }

            ret = wifi67_crypto_decrypt(ctx->crypto,
                                     ctx->enc_data,
                                     CRYPTO_TEST_DATA_SIZE + CRYPTO_TEST_AUTH_SIZE,
                                     ctx->iv, CRYPTO_TEST_IV_SIZE,
                                     ctx->aad, CRYPTO_TEST_AAD_SIZE,
                                     ctx->dec_data);
            TEST_ASSERT(ret == 0, "Decryption failed in iteration %d", i);

            /* Wait for decryption completion */
            if (!wait_for_completion_timeout(&ctx->dec_done,
                                           msecs_to_jiffies(TEST_TIMEOUT_SHORT))) {
                TEST_ASSERT(0, "Decryption timeout in iteration %d", i);
            }

            /* Verify decrypted data */
            TEST_ASSERT(memcmp(ctx->test_data, ctx->dec_data,
                             CRYPTO_TEST_DATA_SIZE) == 0,
                      "Data mismatch in iteration %d", i);
        }

        /* Reset completions for next iteration */
        reinit_completion(&ctx->enc_done);
        reinit_completion(&ctx->dec_done);
    }

    TEST_PASS();
}

/* Module initialization */
static int __init crypto_test_module_init(void)
{
    struct crypto_test_context *ctx;

    ctx = crypto_test_init();
    if (!ctx)
        return -ENOMEM;

    /* Register test cases */
    REGISTER_TEST("crypto_gcm", "Test AES-GCM encryption/decryption",
                 test_crypto_gcm, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("crypto_ccm", "Test AES-CCM encryption/decryption",
                 test_crypto_ccm, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("crypto_cmac", "Test AES-CMAC authentication",
                 test_crypto_cmac, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("crypto_stress", "Stress test crypto operations",
                 test_crypto_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

static void __exit crypto_test_module_exit(void)
{
    struct test_results results;
    get_test_results(&results);
    pr_info("Crypto tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);
    crypto_test_cleanup(NULL); /* ctx is managed by test framework */
}

module_init(crypto_test_module_init);
module_exit(crypto_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 Crypto Test Module");
MODULE_VERSION("1.0"); 