#ifndef _WIFI67_DMA_REGS_H_
#define _WIFI67_DMA_REGS_H_

/* DMA Register Map */
#define DMA_CTRL_BASE         0x3000
#define DMA_CTRL_CONFIG       (DMA_CTRL_BASE + 0x00)
#define DMA_INT_STATUS        (DMA_CTRL_BASE + 0x04)
#define DMA_INT_MASK         (DMA_CTRL_BASE + 0x08)

#define DMA_TX_RING_BASE     (DMA_CTRL_BASE + 0x100)
#define DMA_TX_RING_SIZE     (DMA_CTRL_BASE + 0x104)
#define DMA_TX_HEAD          (DMA_CTRL_BASE + 0x108)
#define DMA_TX_TAIL          (DMA_CTRL_BASE + 0x10C)

#define DMA_RX_RING_BASE     (DMA_CTRL_BASE + 0x200)
#define DMA_RX_RING_SIZE     (DMA_CTRL_BASE + 0x204)
#define DMA_RX_HEAD          (DMA_CTRL_BASE + 0x208)
#define DMA_RX_TAIL          (DMA_CTRL_BASE + 0x20C)

/* DMA Control Bits */
#define DMA_CTRL_ENABLE      BIT(0)
#define DMA_CTRL_TX_EN       BIT(1)
#define DMA_CTRL_RX_EN       BIT(2)
#define DMA_CTRL_SG_EN       BIT(3)
#define DMA_CTRL_RECOVERY    BIT(4)

/* Interrupt Status/Mask Bits */
#define DMA_INT_TX_DONE      BIT(0)
#define DMA_INT_RX_DONE      BIT(1)
#define DMA_INT_TX_ERR       BIT(2)
#define DMA_INT_RX_ERR       BIT(3)
#define DMA_INT_DESC_ERR     BIT(4)
#define DMA_INT_TIMEOUT      BIT(5)

/* DMA Descriptor Format */
struct dma_desc {
    __le64 addr;         /* Buffer physical address */
    __le32 ctrl;         /* Control bits */
    __le32 status;       /* Status bits */
    __le16 len;          /* Buffer length */
    __le16 flags;        /* Additional flags */
    __le32 next;         /* Next descriptor */
    __le32 reserved[2];  /* Padding for cache line */
} __packed __aligned(32);

/* Descriptor Control Bits */
#define DESC_CTRL_OWN       BIT(31)
#define DESC_CTRL_SOP       BIT(30)
#define DESC_CTRL_EOP       BIT(29)
#define DESC_CTRL_INT       BIT(28)
#define DESC_CTRL_CHAINED   BIT(27)

/* Descriptor Status Bits */
#define DESC_STATUS_DONE    BIT(31)
#define DESC_STATUS_ERR     BIT(30)
#define DESC_STATUS_TRUNC   BIT(29)
#define DESC_STATUS_RETRY   BIT(28)

#endif /* _WIFI67_DMA_REGS_H_ */ 