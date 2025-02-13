#ifndef _MT7921_FW_H_
#define _MT7921_FW_H_

/* Firmware file names */
#define MT7921_ROM_PATCH        "mediatek/mt7921_rom_patch.bin"
#define MT7921_FIRMWARE         "mediatek/mt7921_firmware.bin"

/* Firmware version information */
#define MT7921_ROM_PATCH_VER    "1.0.0"
#define MT7921_FIRMWARE_VER     "2.0.0"

/* Firmware features */
#define MT7921_FW_FEATURE_HE            BIT(0)
#define MT7921_FW_FEATURE_MLO           BIT(1)
#define MT7921_FW_FEATURE_BW_160        BIT(2)
#define MT7921_FW_FEATURE_OFDMA         BIT(3)
#define MT7921_FW_FEATURE_MU_MIMO       BIT(4)

/* Firmware memory regions */
#define MT7921_ROM_PATCH_ADDR   0x1C000
#define MT7921_FIRMWARE_ADDR    0x20000

/* Firmware status codes */
#define MT7921_FW_STATUS_OK             0
#define MT7921_FW_STATUS_INVALID_LEN    1
#define MT7921_FW_STATUS_INVALID_CRC    2
#define MT7921_FW_STATUS_DECRYPT_FAIL   3
#define MT7921_FW_STATUS_INVALID_KEY    4
#define MT7921_FW_STATUS_NOT_DL_DONE    5

/* TODO: Add support for firmware compression */
/* TODO: Add firmware update mechanism */
/* TODO: Add firmware rollback support */

#endif /* _MT7921_FW_H_ */ 