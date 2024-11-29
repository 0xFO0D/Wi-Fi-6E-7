#ifndef _WIFI67_DMA_REGS_H_
#define _WIFI67_DMA_REGS_H_

/* DMA Register Base and Offsets */
#define DMA_REG_BASE         0x3000
#define DMA_CTRL_BASE       (DMA_REG_BASE + 0x00)
#define DMA_CTRL_CONFIG     (DMA_REG_BASE + 0x04)
#define DMA_INT_STATUS      (DMA_REG_BASE + 0x08)
#define DMA_INT_MASK        (DMA_REG_BASE + 0x0C)

/* DMA Control Register Bits */
#define DMA_CTRL_ENABLE     BIT(0)
#define DMA_CTRL_SG_EN      BIT(1)
#define DMA_CTRL_RESET      BIT(2)

/* DMA Interrupt Bits */
#define DMA_INT_TX_DONE     BIT(0)
#define DMA_INT_RX_DONE     BIT(1)
#define DMA_INT_TX_ERR      BIT(2)
#define DMA_INT_RX_ERR      BIT(3)
#define DMA_INT_DESC_ERR    BIT(4)

#endif /* _WIFI67_DMA_REGS_H_ */ 