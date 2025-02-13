#include <linux/kernel.h>
#include <linux/tpm.h>
#include <linux/tpm_command.h>
#include "fw_keys.h"
#include "fw_rollback.h"

/* TPM NV index for firmware version counter */
#define TPM_NV_FW_VERSION_IDX 0x01500000

/* Initialize NV counter */
int fw_rollback_init(void)
{
    struct tpm_chip *chip;
    u32 attrs;
    int ret;

    chip = tpm_default_chip();
    if (!chip)
        return -ENODEV;

    /* Check if NV index exists */
    ret = tpm2_get_nv_index_attrs(chip, TPM_NV_FW_VERSION_IDX, &attrs);
    if (ret < 0) {
        /* Create NV counter */
        attrs = TPM2_NT_COUNTER << 4;  /* Counter type */
        attrs |= TPM2_ATTR_WRITTEN;    /* Already written */
        attrs |= TPM2_ATTR_WRITEDEFINE;/* Can't be redefined */
        attrs |= TPM2_ATTR_WRITE_STCLEAR; /* Write lock until restart */
        
        ret = tpm2_nv_define_space(chip, TPM_NV_FW_VERSION_IDX,
                                  attrs, 8, NULL, 0);
        if (ret < 0)
            return ret;
    }

    return 0;
}

/* Read current version counter */
int fw_rollback_get_version(u64 *version)
{
    struct tpm_chip *chip;
    u8 data[8];
    size_t len = sizeof(data);
    int ret;

    if (!version)
        return -EINVAL;

    chip = tpm_default_chip();
    if (!chip)
        return -ENODEV;

    ret = tpm2_nv_read(chip, TPM_NV_FW_VERSION_IDX, data, &len);
    if (ret < 0)
        return ret;

    *version = be64_to_cpu(*(u64 *)data);
    return 0;
}

/* Increment version counter */
int fw_rollback_increment(void)
{
    struct tpm_chip *chip;
    int ret;

    chip = tpm_default_chip();
    if (!chip)
        return -ENODEV;

    ret = tpm2_nv_increment(chip, TPM_NV_FW_VERSION_IDX);
    if (ret < 0)
        return ret;

    return 0;
}

/* Verify firmware version against counter */
int fw_rollback_verify(u64 fw_version)
{
    u64 counter_version;
    int ret;

    ret = fw_rollback_get_version(&counter_version);
    if (ret < 0)
        return ret;

    if (fw_version < counter_version)
        return -EACCES;  /* Rollback attempt detected */

    return 0;
}

EXPORT_SYMBOL_GPL(fw_rollback_init);
EXPORT_SYMBOL_GPL(fw_rollback_get_version);
EXPORT_SYMBOL_GPL(fw_rollback_increment);
EXPORT_SYMBOL_GPL(fw_rollback_verify); 