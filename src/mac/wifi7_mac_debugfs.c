/* SPDX-License-Identifier: MIT */
/*
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "wifi7_mac.h"

/* Statistics structure */
struct wifi7_mac_stats {
    /* General statistics */
    u64 total_tx_bytes;
    u64 total_rx_bytes;
    u64 total_tx_packets;
    u64 total_rx_packets;
    u64 total_errors;
    
    /* MLO statistics */
    u32 active_links;
    u32 link_switches;
    u32 link_errors;
    
    /* Aggregation statistics */
    u32 ampdu_tx;
    u32 ampdu_rx;
    u32 multi_tid_tx;
    u32 multi_tid_rx;
    
    /* Power management */
    u32 power_save_entries;
    u32 power_save_exits;
    u64 sleep_time_ms;
};

/* Link state show function */
static int wifi7_mac_link_state_show(struct seq_file *m, void *v)
{
    struct wifi7_mac_dev *dev = m->private;
    int i;

    seq_printf(m, "Active links: %d\n\n", atomic_read(&dev->active_links));

    for (i = 0; i < max_links; i++) {
        struct wifi7_link_state *link = &dev->links[i];
        
        seq_printf(m, "Link %d:\n", i);
        seq_printf(m, "  Enabled: %s\n", link->enabled ? "yes" : "no");
        seq_printf(m, "  State: %d\n", link->mlo_state);
        seq_printf(m, "  Frequency: %u MHz\n", link->freq);
        seq_printf(m, "  Bandwidth: %u MHz\n", link->bandwidth);
        seq_printf(m, "  NSS: %u\n", link->nss);
        seq_printf(m, "  MCS: %u\n", link->mcs);
        seq_printf(m, "  LDPC: %s\n", link->ldpc ? "yes" : "no");
        seq_printf(m, "  STBC: %s\n", link->stbc ? "yes" : "no");
        seq_printf(m, "  Power save: %s\n", link->power_save ? "on" : "off");
        seq_printf(m, "  Metrics:\n");
        seq_printf(m, "    RSSI: %d dBm\n", link->metrics.rssi);
        seq_printf(m, "    SNR: %d dB\n", link->metrics.snr);
        seq_printf(m, "    TX rate: %u Mbps\n", link->metrics.tx_rate);
        seq_printf(m, "    RX rate: %u Mbps\n", link->metrics.rx_rate);
        seq_printf(m, "    Busy time: %u%%\n", link->metrics.busy_time);
        seq_printf(m, "    Throughput: %u Mbps\n", link->metrics.throughput);
        seq_printf(m, "  Statistics:\n");
        seq_printf(m, "    TX bytes: %llu\n", link->tx_bytes);
        seq_printf(m, "    RX bytes: %llu\n", link->rx_bytes);
        seq_printf(m, "    TX packets: %llu\n", link->tx_packets);
        seq_printf(m, "    RX packets: %llu\n", link->rx_packets);
        seq_printf(m, "    TX errors: %llu\n", link->tx_errors);
        seq_printf(m, "    RX errors: %llu\n", link->rx_errors);
        seq_printf(m, "\n");
    }

    return 0;
}

DEFINE_SHOW_ATTRIBUTE(wifi7_mac_link_state);

/* Queue state show function */
static int wifi7_mac_queue_state_show(struct seq_file *m, void *v)
{
    struct wifi7_mac_dev *dev = m->private;
    int i;

    for (i = 0; i < WIFI7_MAX_QUEUES; i++) {
        seq_printf(m, "Queue %d:\n", i);
        seq_printf(m, "  Length: %u\n", skb_queue_len(&dev->queues[i]));
        seq_printf(m, "  Parameters: 0x%08x\n", dev->queue_params[i]);
        seq_printf(m, "\n");
    }

    return 0;
}

DEFINE_SHOW_ATTRIBUTE(wifi7_mac_queue_state);

/* Multi-TID state show function */
static int wifi7_mac_multi_tid_show(struct seq_file *m, void *v)
{
    struct wifi7_mac_dev *dev = m->private;
    struct wifi7_multi_tid_config *config = &dev->multi_tid;

    seq_printf(m, "Multi-TID Configuration:\n");
    seq_printf(m, "  Enabled: %s\n", config->enabled ? "yes" : "no");
    seq_printf(m, "  Max TIDs: %u\n", config->max_tids);
    seq_printf(m, "  Max A-MPDU length: %u\n", config->max_ampdu_len);
    seq_printf(m, "  TID bitmap: 0x%02x\n", config->tid_bitmap);
    seq_printf(m, "  Timeout: %u ms\n", config->timeout_ms);

    return 0;
}

DEFINE_SHOW_ATTRIBUTE(wifi7_mac_multi_tid);

/* Power state show function */
static int wifi7_mac_power_state_show(struct seq_file *m, void *v)
{
    struct wifi7_mac_dev *dev = m->private;
    int i;

    seq_printf(m, "Global power state: %d\n\n",
              atomic_read(&dev->power_state));

    for (i = 0; i < max_links; i++) {
        struct wifi7_link_state *link = &dev->links[i];
        
        if (!link->enabled)
            continue;

        seq_printf(m, "Link %d:\n", i);
        seq_printf(m, "  Power save: %s\n", link->power_save ? "on" : "off");
        seq_printf(m, "  Sleep duration: %u ms\n", link->sleep_duration);
        seq_printf(m, "  Awake duration: %u ms\n", link->awake_duration);
        seq_printf(m, "\n");
    }

    return 0;
}

DEFINE_SHOW_ATTRIBUTE(wifi7_mac_power_state);

/* Initialize debugfs */
int wifi7_mac_debugfs_init(struct wifi7_mac_dev *dev)
{
    struct dentry *dir;

    dir = debugfs_create_dir("wifi7_mac", NULL);
    if (IS_ERR(dir))
        return PTR_ERR(dir);

    dev->debugfs_dir = dir;

    debugfs_create_file("link_state", 0444, dir, dev,
                       &wifi7_mac_link_state_fops);
    debugfs_create_file("queue_state", 0444, dir, dev,
                       &wifi7_mac_queue_state_fops);
    debugfs_create_file("multi_tid", 0444, dir, dev,
                       &wifi7_mac_multi_tid_fops);
    debugfs_create_file("power_state", 0444, dir, dev,
                       &wifi7_mac_power_state_fops);

    /* Module parameters */
    debugfs_create_u32("max_ampdu_len", 0644, dir,
                      (u32 *)&max_ampdu_len);
    debugfs_create_u32("max_links", 0644, dir,
                      (u32 *)&max_links);

    return 0;
}

void wifi7_mac_debugfs_remove(struct wifi7_mac_dev *dev)
{
    debugfs_remove_recursive(dev->debugfs_dir);
    dev->debugfs_dir = NULL;
}

/* Get statistics */
void wifi7_mac_get_stats(struct wifi7_mac_dev *dev,
                        struct wifi7_mac_stats *stats)
{
    int i;

    memset(stats, 0, sizeof(*stats));

    for (i = 0; i < max_links; i++) {
        struct wifi7_link_state *link = &dev->links[i];

        stats->total_tx_bytes += link->tx_bytes;
        stats->total_rx_bytes += link->rx_bytes;
        stats->total_tx_packets += link->tx_packets;
        stats->total_rx_packets += link->rx_packets;
        stats->total_errors += link->tx_errors + link->rx_errors;
    }

    stats->active_links = atomic_read(&dev->active_links);
    stats->power_save_entries = atomic_read(&dev->power_state);
} 