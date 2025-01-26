#ifndef _WIFI67_DMA_CORE_H_
#define _WIFI67_DMA_CORE_H_

#include <linux/types.h>
#include <linux/dma-mapping.h>
#include "../core/wifi67.h"

#define WIFI67_DMA_MAX_CHANNELS      16
#define WIFI67_DMA_RING_SIZE         4096
#define WIFI67_DMA_MAX_SEGMENTS      32
#define WIFI67_DMA_MAX_BURST_SIZE    256

/* DMA Register offsets */
#define WIFI67_DMA_REG_CTRL          0x0000
#define WIFI67_DMA_REG_STATUS        0x0004
#define WIFI67_DMA_REG_INT_MASK      0x0008
#define WIFI67_DMA_REG_INT_STATUS    0x000C
#define WIFI67_DMA_REG_RING_CTRL     0x0010
#define WIFI67_DMA_REG_RING_STATUS   0x0014
#define WIFI67_DMA_REG_DESC_CTRL     0x0018
#define WIFI67_DMA_REG_ERR_STATUS    0x001C
#define WIFI67_DMA_REG_PERF_CTRL     0x0020
#define WIFI67_DMA_REG_PERF_STATS    0x0024

/* DMA Control Register bits */
#define WIFI67_DMA_CTRL_ENABLE       BIT(0)
#define WIFI67_DMA_CTRL_RESET        BIT(1)
#define WIFI67_DMA_CTRL_TX_EN        BIT(2)
#define WIFI67_DMA_CTRL_RX_EN        BIT(3)
#define WIFI67_DMA_CTRL_BURST_EN     BIT(4)
#define WIFI67_DMA_CTRL_CHAIN_EN     BIT(5)
#define WIFI67_DMA_CTRL_PERF_EN      BIT(6)

/* DMA Status Register bits */
#define WIFI67_DMA_STATUS_READY      BIT(0)
#define WIFI67_DMA_STATUS_TX_ACTIVE  BIT(1)
#define WIFI67_DMA_STATUS_RX_ACTIVE  BIT(2)
#define WIFI67_DMA_STATUS_ERR        BIT(3)

/* DMA Ring Control bits */
#define WIFI67_DMA_RING_ENABLE       BIT(0)
#define WIFI67_DMA_RING_WRAP         BIT(1)
#define WIFI67_DMA_RING_CHAIN        BIT(2)

/* DMA descriptor flags */
#define WIFI67_DMA_DESC_OWN          BIT(31)
#define WIFI67_DMA_DESC_SOP          BIT(30)
#define WIFI67_DMA_DESC_EOP          BIT(29)
#define WIFI67_DMA_DESC_CHAIN        BIT(28)
#define WIFI67_DMA_DESC_INT          BIT(27)

struct wifi67_dma_desc {
    __le32 flags;
    __le32 buf_addr;
    __le16 buf_len;
    __le16 next_desc;
    __le32 status;
    __le32 timestamp;
    __le32 reserved[2];
} __packed;

struct wifi67_dma_ring {
    struct wifi67_dma_desc *desc;
    dma_addr_t desc_dma;
    void **buf_addr;
    dma_addr_t *buf_dma;
    u32 size;
    u32 head;
    u32 tail;
    spinlock_t lock;
    bool enabled;
};

struct wifi67_dma_channel {
    void __iomem *regs;
    struct wifi67_dma_ring tx_ring;
    struct wifi67_dma_ring rx_ring;
    u32 channel_id;
    u32 flags;
    bool enabled;
};

struct wifi67_dma_stats {
    u64 tx_packets;
    u64 tx_bytes;
    u64 tx_errors;
    u64 rx_packets;
    u64 rx_bytes;
    u64 rx_errors;
    u32 desc_errors;
    u32 ring_full;
    u32 buf_errors;
};

struct wifi67_dma {
    void __iomem *regs;
    struct wifi67_dma_channel channels[WIFI67_DMA_MAX_CHANNELS];
    struct wifi67_dma_stats stats;
    struct device *dev;
    spinlock_t lock;
    u32 num_channels;
    bool enabled;
};

/* Core DMA functions */
int wifi67_dma_init(struct wifi67_priv *priv);
void wifi67_dma_deinit(struct wifi67_priv *priv);
int wifi67_dma_start(struct wifi67_priv *priv);
void wifi67_dma_stop(struct wifi67_priv *priv);

/* Channel management */
int wifi67_dma_channel_init(struct wifi67_priv *priv, u32 channel_id);
void wifi67_dma_channel_deinit(struct wifi67_priv *priv, u32 channel_id);
int wifi67_dma_channel_start(struct wifi67_priv *priv, u32 channel_id);
void wifi67_dma_channel_stop(struct wifi67_priv *priv, u32 channel_id);

/* Ring operations */
int wifi67_dma_ring_init(struct wifi67_priv *priv, u32 channel_id, bool is_tx);
void wifi67_dma_ring_deinit(struct wifi67_priv *priv, u32 channel_id, bool is_tx);
int wifi67_dma_ring_add_buffer(struct wifi67_priv *priv, u32 channel_id,
                              bool is_tx, void *buf, u32 len);
void *wifi67_dma_ring_get_buffer(struct wifi67_priv *priv, u32 channel_id,
                                bool is_tx, u32 *len);

/* Performance monitoring */
int wifi67_dma_get_stats(struct wifi67_priv *priv, struct wifi67_dma_stats *stats);
void wifi67_dma_clear_stats(struct wifi67_priv *priv);
int wifi67_dma_set_burst_size(struct wifi67_priv *priv, u32 size);

#endif /* _WIFI67_DMA_CORE_H_ */ 