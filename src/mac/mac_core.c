#include <linux/module.h>
#include <linux/pci.h>
#include <linux/if_ether.h>
#include <net/mac80211.h>

#include "../../include/mac/mac_core.h"
#include "../../include/core/wifi67.h"

/* Register definitions */
#define WIFI67_MAC_CTRL        0x0000
#define WIFI67_MAC_STATUS      0x0004
#define WIFI67_MAC_CONFIG      0x0008
#define WIFI67_MAC_ADDR_L      0x000C
#define WIFI67_MAC_ADDR_H      0x0010
#define WIFI67_MAC_BSSID_L     0x0014
#define WIFI67_MAC_BSSID_H     0x0018

/* Control register bits */
#define MAC_CTRL_ENABLE        BIT(0)
#define MAC_CTRL_RESET         BIT(1)
#define MAC_CTRL_RX_EN         BIT(2)
#define MAC_CTRL_TX_EN         BIT(3)
#define MAC_CTRL_PROMISC       BIT(4)

static void wifi67_mac_hw_init(struct wifi67_priv *priv)
{
    struct wifi67_mac *mac = &priv->mac;
    u32 reg;

    /* Reset MAC */
    iowrite32(MAC_CTRL_RESET, mac->regs + WIFI67_MAC_CTRL);
    udelay(100);

    /* Clear reset and enable MAC */
    reg = MAC_CTRL_ENABLE | MAC_CTRL_RX_EN | MAC_CTRL_TX_EN;
    iowrite32(reg, mac->regs + WIFI67_MAC_CTRL);

    /* Set capabilities */
    reg = WIFI67_MAC_CAP_MLO | WIFI67_MAC_CAP_MU_MIMO | 
          WIFI67_MAC_CAP_OFDMA | WIFI67_MAC_CAP_TWT;
    iowrite32(reg, mac->regs + WIFI67_MAC_CONFIG);
}

static void wifi67_mac_set_addr(struct wifi67_priv *priv)
{
    struct wifi67_mac *mac = &priv->mac;
    u32 addr_l, addr_h;

    addr_l = *(u32 *)mac->addr;
    addr_h = *(u16 *)(mac->addr + 4);

    iowrite32(addr_l, mac->regs + WIFI67_MAC_ADDR_L);
    iowrite32(addr_h, mac->regs + WIFI67_MAC_ADDR_H);
}

int wifi67_mac_init(struct wifi67_priv *priv)
{
    struct wifi67_mac *mac = &priv->mac;
    int i;

    /* Initialize locks */
    spin_lock_init(&mac->lock);

    /* Initialize queues */
    for (i = 0; i < WIFI67_NUM_TX_QUEUES; i++) {
        skb_queue_head_init(&mac->tx_queues[i].skbs);
        spin_lock_init(&mac->tx_queues[i].lock);
        mac->tx_queues[i].stopped = false;
    }

    for (i = 0; i < WIFI67_NUM_RX_QUEUES; i++) {
        skb_queue_head_init(&mac->rx_queues[i].skbs);
        spin_lock_init(&mac->rx_queues[i].lock);
        mac->rx_queues[i].stopped = false;
    }

    /* Map MAC registers */
    mac->regs = priv->mmio + 0x3000;  /* MAC register offset */

    /* Set initial state */
    mac->state = WIFI67_MAC_OFF;

    /* Initialize hardware */
    wifi67_mac_hw_init(priv);

    /* Set MAC address */
    eth_random_addr(mac->addr);
    wifi67_mac_set_addr(priv);

    return 0;
}

void wifi67_mac_deinit(struct wifi67_priv *priv)
{
    struct wifi67_mac *mac = &priv->mac;
    int i;

    /* Disable MAC */
    iowrite32(0, mac->regs + WIFI67_MAC_CTRL);

    /* Free queued packets */
    for (i = 0; i < WIFI67_NUM_TX_QUEUES; i++)
        skb_queue_purge(&mac->tx_queues[i].skbs);

    for (i = 0; i < WIFI67_NUM_RX_QUEUES; i++)
        skb_queue_purge(&mac->rx_queues[i].skbs);
}

int wifi67_mac_tx(struct wifi67_priv *priv, struct sk_buff *skb,
                  struct ieee80211_tx_control *control)
{
    struct wifi67_mac *mac = &priv->mac;
    struct wifi67_mac_queue *queue;
    int queue_index = skb_get_queue_mapping(skb);

    if (queue_index >= WIFI67_NUM_TX_QUEUES)
        return -EINVAL;

    queue = &mac->tx_queues[queue_index];

    spin_lock_bh(&queue->lock);

    if (queue->stopped) {
        spin_unlock_bh(&queue->lock);
        return -EBUSY;
    }

    skb_queue_tail(&queue->skbs, skb);
    mac->stats.tx_packets++;
    mac->stats.tx_bytes += skb->len;

    spin_unlock_bh(&queue->lock);

    /* Schedule transmission */
    schedule_work(&mac->tx_work);

    return 0;
}

void wifi67_mac_rx(struct wifi67_priv *priv, struct sk_buff *skb)
{
    struct wifi67_mac *mac = &priv->mac;
    struct ieee80211_rx_status *status;

    /* Update statistics */
    mac->stats.rx_packets++;
    mac->stats.rx_bytes += skb->len;

    /* Fill RX status */
    status = IEEE80211_SKB_RXCB(skb);
    memset(status, 0, sizeof(*status));
    status->band = NL80211_BAND_6GHZ;
    status->signal = -50; /* Example signal strength */

    /* Pass to mac80211 */
    ieee80211_rx_irqsafe(priv->hw, skb);
}

