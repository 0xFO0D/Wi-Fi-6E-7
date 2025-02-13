#ifndef _INTEL_FW_H_
#define _INTEL_FW_H_

/* Firmware file names */
#define INTEL_AX211_FW         "intel/AX211_fw.bin"
#define INTEL_AX411_FW         "intel/AX411_fw.bin"
#define INTEL_BE200_FW         "intel/BE200_fw.bin"
#define INTEL_BOOT_CODE        "intel/boot_code.bin"
#define INTEL_RADIO_CFG        "intel/radio_cfg.bin"

/* Firmware version information */
#define INTEL_FW_API_VER       0x01
#define INTEL_FW_MAJOR_VER     0x02
#define INTEL_FW_MINOR_VER     0x03
#define INTEL_FW_BUILD_VER     0x04

/* Firmware features */
#define INTEL_FW_FEATURE_HE           BIT(0)
#define INTEL_FW_FEATURE_MLO          BIT(1)
#define INTEL_FW_FEATURE_BW_160       BIT(2)
#define INTEL_FW_FEATURE_BW_320       BIT(3)
#define INTEL_FW_FEATURE_OFDMA        BIT(4)
#define INTEL_FW_FEATURE_MU_MIMO      BIT(5)
#define INTEL_FW_FEATURE_SECURE_BOOT  BIT(6)

/* Firmware memory regions */
#define INTEL_FW_START_ADDR    0x40000
#define INTEL_BOOT_START_ADDR  0x20000
#define INTEL_CFG_START_ADDR   0x60000

/* Firmware status codes */
#define INTEL_FW_STATUS_OK             0x0000
#define INTEL_FW_STATUS_ERROR          0x0001
#define INTEL_FW_STATUS_INVALID_API    0x0002
#define INTEL_FW_STATUS_VERSION_MISM   0x0003
#define INTEL_FW_STATUS_MEM_ERROR      0x0004
#define INTEL_FW_STATUS_SECURE_FAIL    0x0005

/* Firmware compression support */
#define INTEL_FW_COMP_NONE     0
#define INTEL_FW_COMP_ZLIB     1
#define INTEL_FW_COMP_XZ       2

/* Firmware update flags */
#define INTEL_FW_UPDATE_FORCE         BIT(0)
#define INTEL_FW_UPDATE_PRESERVE_CFG  BIT(1)
#define INTEL_FW_UPDATE_VERIFY        BIT(2)

/* Firmware rollback protection */
#define INTEL_FW_MIN_VER_MAJOR  1
#define INTEL_FW_MIN_VER_MINOR  0

struct intel_fw_header {
    u32 magic;
    u8 api_ver;
    u8 major;
    u8 minor;
    u8 build;
    u32 size;
    u32 checksum;
    u16 features;
    u8 compression;
    u8 reserved;
} __packed;

#endif /* _INTEL_FW_H_ */ 