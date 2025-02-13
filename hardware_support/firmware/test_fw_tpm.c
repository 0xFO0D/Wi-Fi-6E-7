#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/tpm.h>
#include "fw_keys.h"

/* Test key ID */
#define TEST_KEY_ID 0x42

/* Test PCR values */
static const u8 test_pcr_values[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
};

/* uwu let's test some quotes! */
static int test_quote_verification(void)
{
    struct tpm_quote_info quote;
    int ret;

    pr_info("OwO what's this? A quote verification test!\n");

    /* Get quote for test key */
    ret = fw_tpm_get_quote(TEST_KEY_ID, &quote);
    if (ret != KEY_ERR_NONE) {
        pr_err("Failed to get quote: %d\n", ret);
        return ret;
    }

    /* UwU time to verify the quote */
    ret = fw_tpm_verify_quote(&quote);
    if (ret != KEY_ERR_NONE) {
        pr_err("Quote verification failed: %d\n", ret);
        return ret;
    }

    pr_info("Quote verification test passed! ^_^\n");
    return 0;
}

/* rawr! let's test some policies! */
static int test_policy_evaluation(void)
{
    struct tpm_policy_info policy;
    int ret;

    pr_info("Time to test some spicy policies! >w<\n");

    /* Get policy for test key */
    ret = fw_tpm_get_policy(TEST_KEY_ID, &policy);
    if (ret != KEY_ERR_NONE) {
        pr_err("Failed to get policy: %d\n", ret);
        return ret;
    }

    /* Verify policy */
    ret = fw_tpm_verify_policy(&policy);
    if (ret != KEY_ERR_NONE) {
        pr_err("Policy verification failed: %d\n", ret);
        return ret;
    }

    /* Test policy extension */
    ret = fw_tpm_extend_policy(&policy, test_pcr_values,
                              sizeof(test_pcr_values));
    if (ret != KEY_ERR_NONE) {
        pr_err("Policy extension failed: %d\n", ret);
        return ret;
    }

    pr_info("Policy evaluation test passed! UwU\n");
    return 0;
}

/* Test cached policy behavior */
static int test_policy_cache(void)
{
    struct tpm_policy_info policy1, policy2;
    int ret;

    pr_info("Testing policy cache! OwO\n");

    /* Get policy with caching enabled */
    policy1.flags = POLICY_FLAG_CACHE;
    ret = fw_tpm_get_policy(TEST_KEY_ID, &policy1);
    if (ret != KEY_ERR_NONE) {
        pr_err("Failed to get first policy: %d\n", ret);
        return ret;
    }

    /* Get policy again, should use cache */
    policy2.flags = POLICY_FLAG_CACHE;
    ret = fw_tpm_get_policy(TEST_KEY_ID, &policy2);
    if (ret != KEY_ERR_NONE) {
        pr_err("Failed to get second policy: %d\n", ret);
        return ret;
    }

    /* Compare policy digests */
    if (memcmp(policy1.policy_digest, policy2.policy_digest,
               sizeof(policy1.policy_digest)) != 0) {
        pr_err("Cache test failed: policy mismatch\n");
        return -EINVAL;
    }

    pr_info("Policy cache test passed! ^w^\n");
    return 0;
}

static int __init test_fw_tpm_init(void)
{
    int ret;

    pr_info("Starting firmware TPM tests! UwU\n");

    ret = test_quote_verification();
    if (ret)
        return ret;

    ret = test_policy_evaluation();
    if (ret)
        return ret;

    ret = test_policy_cache();
    if (ret)
        return ret;

    pr_info("All firmware TPM tests passed! \\(^o^)/\n");
    return 0;
}

static void __exit test_fw_tpm_exit(void)
{
    pr_info("Firmware TPM tests cleanup complete! Bye bye! >w<\n");
}

module_init(test_fw_tpm_init);
module_exit(test_fw_tpm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("WiFi 6E/7 Driver Team");
MODULE_DESCRIPTION("Firmware TPM Integration Tests");
MODULE_VERSION("1.0"); 