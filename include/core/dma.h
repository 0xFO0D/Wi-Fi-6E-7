#ifndef _WIFI67_DMA_H_
#define _WIFI67_DMA_H_

#include <linux/types.h>
#include <linux/dma-mapping.h>

#define WIFI67_DMA_RING_SIZE 256
#define WIFI67_DMA_DESC_SIZE 16

struct wifi67_dma_desc {
    __le32 ctrl;
    __le32 flags;
    __le32 addr_lo;
    __le32 addr_hi;
};

struct wifi67_dma_ring {
    struct wifi67_dma_desc *desc;
    dma_addr_t dma;
    u32 size;
    u32 read_idx;
    u32 write_idx;
    void **buf;
    spinlock_t lock;
};

#endif /* _WIFI67_DMA_H_ */ 