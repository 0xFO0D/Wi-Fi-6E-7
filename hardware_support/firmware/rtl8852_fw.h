#ifndef _RTL8852_FW_H_
#define _RTL8852_FW_H_

/* Firmware file names */
#define RTL8852_FIRMWARE        "realtek/rtl8852_fw.bin"
#define RTL8852_ROM_IMG         "realtek/rtl8852_rom.bin"
#define RTL8852_RAM_CODE        "realtek/rtl8852_ram_code.bin"

/* Firmware version information */
#define RTL8852_FW_MAJOR_VER    0x01
#define RTL8852_FW_MINOR_VER    0x02
#define RTL8852_FW_PATCH_VER    0x03
#define RTL8852_FW_SUB_VER      0x04

/* Firmware features */
#define RTL8852_FW_FEATURE_HE           BIT(0)
#define RTL8852_FW_FEATURE_MLO          BIT(1)
#define RTL8852_FW_FEATURE_BW_160       BIT(2)
#define RTL8852_FW_FEATURE_OFDMA        BIT(3)
#define RTL8852_FW_FEATURE_MU_MIMO      BIT(4)
#define RTL8852_FW_FEATURE_MESH         BIT(5)

/* Firmware memory regions */
#define RTL8852_FW_START_ADDR   0x20000
#define RTL8852_ROM_START_ADDR  0x18000
#define RTL8852_RAM_CODE_ADDR   0x40000

/* Firmware status codes */
#define RTL8852_FW_STATUS_SUCCESS        0x000000
#define RTL8852_FW_STATUS_BUSY          0x000001
#define RTL8852_FW_STATUS_ERROR         0x000002
#define RTL8852_FW_STATUS_NO_MEMORY     0x000003
#define RTL8852_FW_STATUS_INVALID_DATA  0x000004

/* TODO: Add support for secure boot */
/* TODO: Add firmware debugging capabilities */
/* TODO: Add power calibration data support */

#endif /* _RTL8852_FW_H_ */ 