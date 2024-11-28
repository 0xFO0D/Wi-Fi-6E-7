#ifndef _WIFI67_H_
#define _WIFI67_H_

#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/bitops.h>

/* Hardware capabilities */
#define WIFI67_MAX_TX_QUEUES   8
#define WIFI67_MAX_RX_QUEUES   4
#define WIFI67_MAX_VIRTUAL_IF  16
#define WIFI67_MAX_KEY_ENTRIES 64
#define WIFI67_RING_SIZE       256

/* DMA definitions */
#define WIFI67_DMA_BUF_SIZE    2048
#define WIFI67_RX_BUF_SIZE     (4096 * 2)
#define WIFI67_TX_BUF_SIZE     (4096 * 2)

/* Register map base addresses */
#define WIFI67_REG_CONTROL     0x0000
#define WIFI67_REG_DMA         0x1000
#define WIFI67_REG_MAC         0x2000
#define WIFI67_REG_PHY         0x6000
#define WIFI67_REG_RF          0x8000

struct wifi67_ring {
    void *desc;
    dma_addr_t dma;
    u32 size;
    u32 count;
    u16 read_idx;
    u16 write_idx;
    spinlock_t lock ____cacheline_aligned;
};

struct wifi67_dma {
    struct wifi67_ring tx_ring[WIFI67_MAX_TX_QUEUES];
    struct wifi67_ring rx_ring[WIFI67_MAX_RX_QUEUES];
    struct {
        dma_addr_t phys;
        void *virt;
        size_t size;
    } region[4];
};

struct wifi67_stats {
    u64 tx_packets;
    u64 rx_packets;
    u64 tx_bytes;
    u64 rx_bytes;
    u32 tx_errors;
    u32 rx_errors;
    u32 tx_dropped;
    u32 rx_dropped;
    u32 tx_fifo_errors;
    u32 rx_fifo_errors;
    u32 rx_crc_errors;
    u32 rx_frame_errors;
} __packed __aligned(8);

struct wifi67_priv {
    struct pci_dev *pdev;
    struct ieee80211_hw *hw;
    struct net_device *netdev;
    void __iomem *mmio;
    
    /* DMA management */
    struct wifi67_dma dma;
    
    /* PHY/RF state */
    struct {
        struct phy_stats stats;
        struct phy_calibration cal;
        spinlock_t lock;
    } phy;
    
    /* TX path */
    spinlock_t tx_lock ____cacheline_aligned;
    u32 tx_ring_ptr;
    DECLARE_BITMAP(tx_bitmap, 256);
    
    /* Statistics */
    struct wifi67_stats stats;
    
    /* Power management */
    u32 power_state;
    struct completion firmware_load;
    struct work_struct tx_work;
    
    /* Debug interface */
    struct dentry *debugfs_dir;
    
    /* MAC layer */
    struct {
        struct mac_stats stats;
        struct mac_ba_session *ba_sessions[MAX_TID_COUNT];
        struct mac_mlo_link *mlo_links[MAX_MLO_LINKS];
        spinlock_t lock;
    } mac;
};

/* Function prototypes */
int wifi67_alloc_dma_rings(struct wifi67_priv *priv);
void wifi67_free_dma_rings(struct wifi67_priv *priv);
int wifi67_dma_init(struct wifi67_priv *priv);
void wifi67_dma_deinit(struct wifi67_priv *priv);
int wifi67_tx_init(struct wifi67_priv *priv);
void wifi67_tx_deinit(struct wifi67_priv *priv);

/* Register access helpers */
static inline u32 wifi67_read32(struct wifi67_priv *priv, u32 reg)
{
    return readl(priv->mmio + reg);
}

static inline void wifi67_write32(struct wifi67_priv *priv, u32 reg, u32 val)
{
    writel(val, priv->mmio + reg);
}

static inline u64 wifi67_read64(struct wifi67_priv *priv, u32 reg)
{
    u64 val;
    val = readl(priv->mmio + reg + 4);
    val = (val << 32) | readl(priv->mmio + reg);
    return val;
}

static inline void wifi67_write64(struct wifi67_priv *priv, u32 reg, u64 val)
{
    writel(val & 0xFFFFFFFF, priv->mmio + reg);
    writel(val >> 32, priv->mmio + reg + 4);
}

#endif /* _WIFI67_H_ */ 