#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include "fw_common.h"

/* Test data */
static const u8 test_data[] = {
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0
};
static const u32 test_checksum = 0x7D42FB05;

/* Test compressed data (zlib format) */
static const u8 test_compressed[] = {
    0x78, 0x9C, 0x63, 0x64, 0x62, 0x66, 0x61, 0x65,
    0x63, 0x67, 0x00, 0x00, 0x0C, 0x8C, 0x03, 0x01
};
static const u8 test_decompressed[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
};

static int __init test_fw_common_init(void)
{
    int ret;
    char version_str[32];
    u8 *decomp_buf;
    size_t decomp_size = 16;
    struct fw_version v1 = {1, 2, 3, 4, "test1"};
    struct fw_version v2 = {1, 2, 3, 5, "test2"};

    pr_info("Starting firmware common tests\n");

    /* Test checksum verification */
    ret = fw_verify_checksum(test_data, sizeof(test_data),
                            test_checksum);
    if (ret != FW_ERR_NONE) {
        pr_err("Checksum verification failed\n");
        return -EINVAL;
    }
    pr_info("Checksum verification passed\n");

    /* Test zlib decompression */
    decomp_buf = kmalloc(decomp_size, GFP_KERNEL);
    if (!decomp_buf)
        return -ENOMEM;

    ret = fw_decompress_zlib(test_compressed, sizeof(test_compressed),
                            decomp_buf, &decomp_size);
    if (ret != FW_ERR_NONE) {
        pr_err("Zlib decompression failed\n");
        kfree(decomp_buf);
        return -EINVAL;
    }

    if (decomp_size != sizeof(test_decompressed) ||
        memcmp(decomp_buf, test_decompressed,
               sizeof(test_decompressed)) != 0) {
        pr_err("Decompressed data mismatch\n");
        kfree(decomp_buf);
        return -EINVAL;
    }
    pr_info("Zlib decompression passed\n");
    kfree(decomp_buf);

    /* Test version comparison */
    if (fw_version_compare(&v1, &v2) >= 0) {
        pr_err("Version comparison failed\n");
        return -EINVAL;
    }
    pr_info("Version comparison passed\n");

    /* Test version string conversion */
    fw_version_to_string(&v1, version_str, sizeof(version_str));
    if (strcmp(version_str, "1.2.3.4-test1") != 0) {
        pr_err("Version string conversion failed\n");
        return -EINVAL;
    }
    pr_info("Version string conversion passed\n");

    pr_info("All firmware common tests passed\n");
    return 0;
}

static void __exit test_fw_common_exit(void)
{
    pr_info("Firmware common tests cleanup complete\n");
}

module_init(test_fw_common_init);
module_exit(test_fw_common_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("WiFi 6E/7 Driver Team");
MODULE_DESCRIPTION("Firmware Common Functionality Tests");
MODULE_VERSION("1.0"); 