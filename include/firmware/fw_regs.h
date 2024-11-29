#ifndef _WIFI67_FW_REGS_H_
#define _WIFI67_FW_REGS_H_

/* Firmware control registers */
#define WIFI67_FW_REG_BASE          0x1000

#define WIFI67_REG_FW_CTRL          (WIFI67_FW_REG_BASE + 0x00)
#define WIFI67_REG_FW_STATUS        (WIFI67_FW_REG_BASE + 0x04)
#define WIFI67_REG_FW_ENTRY         (WIFI67_FW_REG_BASE + 0x08)
#define WIFI67_REG_FW_START         (WIFI67_FW_REG_BASE + 0x0C)
#define WIFI67_REG_FW_VERSION       (WIFI67_FW_REG_BASE + 0x10)
#define WIFI67_REG_FW_API_VERSION   (WIFI67_FW_REG_BASE + 0x14)
#define WIFI67_REG_FW_ERROR         (WIFI67_FW_REG_BASE + 0x18)

/* Firmware control register bits */
#define WIFI67_FW_CTRL_RESET        BIT(0)
#define WIFI67_FW_CTRL_RUN          BIT(1)
#define WIFI67_FW_CTRL_DEBUG        BIT(2)
#define WIFI67_FW_CTRL_IRQ_EN       BIT(3)

/* Firmware status register bits */
#define WIFI67_FW_STATUS_READY      BIT(0)
#define WIFI67_FW_STATUS_RUNNING    BIT(1)
#define WIFI67_FW_STATUS_ERROR      BIT(2)
#define WIFI67_FW_STATUS_BUSY       BIT(3)

/* Firmware error codes */
#define WIFI67_FW_ERR_NONE          0x00
#define WIFI67_FW_ERR_INIT          0x01
#define WIFI67_FW_ERR_MEMORY        0x02
#define WIFI67_FW_ERR_TIMEOUT       0x03
#define WIFI67_FW_ERR_INVALID       0x04
#define WIFI67_FW_ERR_VERSION       0x05
#define WIFI67_FW_ERR_CHECKSUM      0x06

#endif /* _WIFI67_FW_REGS_H_ */ 