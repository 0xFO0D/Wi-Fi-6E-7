#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include "fw_attest.h"

/* Test session ID */
static const u8 test_session_id[16] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10
};

/* Test data */
static const u8 test_data[] = "Hello, Attestation!";

/* Test challenge generation */
static int test_challenge_gen(void)
{
    struct attest_challenge challenge;
    int ret;

    pr_info("Testing challenge generation...\n");

    ret = fw_attest_challenge(test_session_id, &challenge);
    if (ret < 0) {
        pr_err("Challenge generation failed: %d\n", ret);
        return ret;
    }

    pr_info("Challenge generation test passed!\n");
    return 0;
}

/* Test attestation verification */
static int test_attest_verify(void)
{
    struct attest_response response;
    int ret;

    pr_info("Testing attestation verification...\n");

    /* Initialize response with test data */
    memset(&response, 0, sizeof(response));
    response.data = (void *)test_data;
    response.data_len = sizeof(test_data);

    ret = fw_attest_verify(test_session_id, &response);
    if (ret < 0) {
        pr_err("Attestation verification failed: %d\n", ret);
        return ret;
    }

    pr_info("Attestation verification test passed!\n");
    return 0;
}

/* Test data export */
static int test_attest_export(void)
{
    struct attest_export export;
    u8 *data;
    int ret;

    pr_info("Testing attestation data export...\n");

    data = kmalloc(sizeof(test_data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    memcpy(data, test_data, sizeof(test_data));
    export.data = data;
    export.data_len = sizeof(test_data);

    ret = fw_attest_export(test_session_id, &export);
    if (ret < 0) {
        pr_err("Attestation export failed: %d\n", ret);
        goto out;
    }

    pr_info("Attestation export test passed!\n");
    ret = 0;

out:
    kfree(data);
    return ret;
}

/* Test full attestation flow */
static int test_attest_flow(void)
{
    struct attest_challenge challenge;
    struct attest_response response;
    struct attest_export export;
    u8 *data;
    int ret;

    pr_info("Testing full attestation flow...\n");

    /* Generate challenge */
    ret = fw_attest_challenge(test_session_id, &challenge);
    if (ret < 0) {
        pr_err("Challenge generation failed: %d\n", ret);
        return ret;
    }

    /* Prepare response */
    data = kmalloc(sizeof(test_data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    memcpy(data, test_data, sizeof(test_data));
    response.data = data;
    response.data_len = sizeof(test_data);
    memcpy(response.nonce, challenge.nonce, sizeof(response.nonce));
    response.timestamp = challenge.timestamp;

    /* Verify response */
    ret = fw_attest_verify(test_session_id, &response);
    if (ret < 0) {
        pr_err("Response verification failed: %d\n", ret);
        goto out;
    }

    /* Export data */
    export.data = data;
    export.data_len = sizeof(test_data);

    ret = fw_attest_export(test_session_id, &export);
    if (ret < 0) {
        pr_err("Data export failed: %d\n", ret);
        goto out;
    }

    pr_info("Full attestation flow test passed!\n");
    ret = 0;

out:
    kfree(data);
    return ret;
}

static int __init test_fw_attest_init(void)
{
    int ret;

    pr_info("Starting firmware attestation tests!\n");

    ret = test_challenge_gen();
    if (ret)
        return ret;

    ret = test_attest_verify();
    if (ret)
        return ret;

    ret = test_attest_export();
    if (ret)
        return ret;

    ret = test_attest_flow();
    if (ret)
        return ret;

    pr_info("All firmware attestation tests passed!\n");
    return 0;
}

static void __exit test_fw_attest_exit(void)
{
    pr_info("Firmware attestation tests cleanup complete!\n");
}

module_init(test_fw_attest_init);
module_exit(test_fw_attest_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("WiFi 6E/7 Driver Team");
MODULE_DESCRIPTION("Firmware Attestation Tests");
MODULE_VERSION("1.0"); 