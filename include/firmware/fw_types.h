#ifndef _WIFI67_FW_TYPES_H_
#define _WIFI67_FW_TYPES_H_

#include <linux/types.h>
#include <linux/firmware.h>

struct wifi67_firmware {
    /* Firmware binary */
    const struct firmware *fw;
    void __iomem *fw_mem;
    dma_addr_t fw_dma_addr;
    size_t fw_dma_size;
    
    /* Firmware state */
    bool loaded;
    bool verified;
    bool running;
    
    /* Memory regions */
    void __iomem *shared_region;
    
    /* Version info */
    u32 version;
    u32 api_version;
    
    /* Statistics */
    atomic_t tx_pkts;
    atomic_t rx_pkts;
    atomic_t errors;
    
    /* Locks */
    spinlock_t lock;
};

#endif /* _WIFI67_FW_TYPES_H_ */ 