/*
 * WiFi 7 Spatial Stream Management and MU-MIMO Coordination
 * Copyright (c) 2024 Fayssal Chokri <fayssalchokri@gmail.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include <linux/random.h>
#include <linux/crc32.h>
#include <linux/completion.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include "wifi7_spatial.h"
#include "wifi7_mac.h"
#include "../hal/wifi7_rf.h"

/* Device state */
struct wifi7_spatial_dev {
    struct wifi7_dev *dev;           /* Core device structure */
    struct wifi7_spatial_config config; /* Spatial configuration */
    struct wifi7_spatial_stats stats;  /* Spatial statistics */
    struct dentry *debugfs_dir;       /* debugfs directory */
    spinlock_t lock;                 /* Device lock */
    bool initialized;                /* Initialization flag */
    struct {
        struct wifi7_spatial_csi *csi;  /* CSI array */
        u32 num_entries;                /* Number of CSI entries */
        spinlock_t lock;                /* CSI lock */
    } csi_data;
    struct {
        struct wifi7_spatial_beam *patterns; /* Beam patterns */
        u8 active_pattern;                  /* Active pattern ID */
        spinlock_t lock;                    /* Pattern lock */
    } beamforming;
    struct {
        struct wifi7_spatial_group *groups; /* MU-MIMO groups */
        u32 num_groups;                    /* Number of groups */
        spinlock_t lock;                   /* Group lock */
    } mu_mimo;
    struct {
        struct delayed_work csi_work;      /* CSI collection */
        struct delayed_work beam_work;     /* Beam tracking */
        struct delayed_work group_work;    /* Group management */
        struct completion sounding_done;   /* Sounding completion */
    } workers;
};

/* Global device context */
static struct wifi7_spatial_dev *spatial_dev;

/* Helper functions */
static inline bool is_valid_stream_count(u8 streams)
{
    return streams > 0 && streams <= WIFI7_MAX_SPATIAL_STREAMS;
}

static inline bool is_valid_group_id(u8 group_id)
{
    return group_id < WIFI7_MAX_MU_GROUPS;
}

static inline bool is_valid_pattern_id(u8 pattern_id)
{
    return pattern_id < WIFI7_MAX_BEAM_PATTERNS;
}

/* CSI processing */
static void process_csi_update(struct wifi7_spatial_dev *dev,
                             struct wifi7_spatial_csi *csi)
{
    unsigned long flags;
    int i;

    if (!csi->magnitude || !csi->phase)
        return;

    spin_lock_irqsave(&dev->csi_data.lock, flags);

    /* Find empty or oldest CSI slot */
    int oldest_idx = 0;
    u32 oldest_time = U32_MAX;
    
    for (i = 0; i < dev->csi_data.num_entries; i++) {
        if (dev->csi_data.csi[i].timestamp == 0) {
            oldest_idx = i;
            break;
        }
        if (dev->csi_data.csi[i].timestamp < oldest_time) {
            oldest_time = dev->csi_data.csi[i].timestamp;
            oldest_idx = i;
        }
    }

    /* Update CSI data */
    memcpy(&dev->csi_data.csi[oldest_idx], csi, sizeof(*csi));
    dev->csi_data.csi[oldest_idx].timestamp = ktime_get_seconds();

    dev->stats.csi_updates++;

    spin_unlock_irqrestore(&dev->csi_data.lock, flags);
}

/* Beamforming management */
static int update_beam_pattern(struct wifi7_spatial_dev *dev,
                             struct wifi7_spatial_beam *beam)
{
    unsigned long flags;
    int ret = 0;

    if (!is_valid_pattern_id(beam->pattern_id))
        return -EINVAL;

    spin_lock_irqsave(&dev->beamforming.lock, flags);

    /* Update beam pattern */
    memcpy(&dev->beamforming.patterns[beam->pattern_id], beam, sizeof(*beam));

    /* Apply pattern if it's the active one */
    if (beam->pattern_id == dev->beamforming.active_pattern) {
        ret = wifi7_rf_set_beam_pattern(dev->dev, beam);
        if (ret == 0)
            dev->stats.beam_switches++;
    }

    spin_unlock_irqrestore(&dev->beamforming.lock, flags);
    return ret;
}

/* MU-MIMO group management */
static int update_mu_group(struct wifi7_spatial_dev *dev,
                          struct wifi7_spatial_group *group)
{
    unsigned long flags;
    int ret = 0;

    if (!is_valid_group_id(group->group_id))
        return -EINVAL;

    if (group->num_users > WIFI7_MAX_USERS_PER_GROUP)
        return -EINVAL;

    spin_lock_irqsave(&dev->mu_mimo.lock, flags);

    /* Update group configuration */
    memcpy(&dev->mu_mimo.groups[group->group_id], group, sizeof(*group));

    /* Allocate streams for the group */
    if (group->active) {
        ret = wifi7_mac_alloc_mu_streams(dev->dev, group);
        if (ret == 0)
            dev->stats.group_changes++;
    }

    spin_unlock_irqrestore(&dev->mu_mimo.lock, flags);
    return ret;
}

/* Work handlers */
static void spatial_csi_work_handler(struct work_struct *work)
{
    struct wifi7_spatial_dev *dev = spatial_dev;
    struct wifi7_spatial_csi csi;
    int ret;

    if (!dev->initialized)
        return;

    /* Perform channel sounding */
    ret = wifi7_rf_perform_sounding(dev->dev, &csi);
    if (ret == 0) {
        process_csi_update(dev, &csi);
        dev->stats.sounding.success++;
    } else {
        dev->stats.sounding.failures++;
    }

    /* Schedule next sounding */
    if (dev->config.mode != WIFI7_SPATIAL_MODE_LEGACY)
        schedule_delayed_work(&dev->workers.csi_work,
                            msecs_to_jiffies(dev->config.update_interval));
}

static void spatial_beam_work_handler(struct work_struct *work)
{
    struct wifi7_spatial_dev *dev = spatial_dev;
    struct wifi7_spatial_beam *beam;
    unsigned long flags;
    int ret;

    if (!dev->initialized || !dev->config.tracking)
        return;

    spin_lock_irqsave(&dev->beamforming.lock, flags);
    beam = &dev->beamforming.patterns[dev->beamforming.active_pattern];
    spin_unlock_irqrestore(&dev->beamforming.lock, flags);

    /* Update beam tracking */
    ret = wifi7_rf_track_beam(dev->dev, beam);
    if (ret == 0)
        dev->stats.tracking_updates++;

    /* Schedule next tracking */
    if (dev->config.tracking)
        schedule_delayed_work(&dev->workers.beam_work,
                            msecs_to_jiffies(dev->config.beamforming.update_rate));
}

static void spatial_group_work_handler(struct work_struct *work)
{
    struct wifi7_spatial_dev *dev = spatial_dev;
    int i;

    if (!dev->initialized || !dev->config.mu_enabled)
        return;

    /* Update MU-MIMO group statistics */
    spin_lock_irq(&dev->mu_mimo.lock);
    
    dev->stats.mu_mimo.active_groups = 0;
    dev->stats.mu_mimo.total_users = 0;
    
    for (i = 0; i < dev->mu_mimo.num_groups; i++) {
        if (dev->mu_mimo.groups[i].active) {
            dev->stats.mu_mimo.active_groups++;
            dev->stats.mu_mimo.total_users += dev->mu_mimo.groups[i].num_users;
        }
    }
    
    spin_unlock_irq(&dev->mu_mimo.lock);

    /* Schedule next update */
    if (dev->config.mu_enabled)
        schedule_delayed_work(&dev->workers.group_work,
                            msecs_to_jiffies(dev->config.mu_mimo.timeout));
}

/* Module initialization */
int wifi7_spatial_init(struct wifi7_dev *dev)
{
    struct wifi7_spatial_dev *sdev;
    int ret;

    /* Allocate device context */
    sdev = kzalloc(sizeof(*sdev), GFP_KERNEL);
    if (!sdev)
        return -ENOMEM;

    sdev->dev = dev;
    spin_lock_init(&sdev->lock);
    spin_lock_init(&sdev->csi_data.lock);
    spin_lock_init(&sdev->beamforming.lock);
    spin_lock_init(&sdev->mu_mimo.lock);
    spatial_dev = sdev;

    /* Initialize work queues */
    INIT_DELAYED_WORK(&sdev->workers.csi_work, spatial_csi_work_handler);
    INIT_DELAYED_WORK(&sdev->workers.beam_work, spatial_beam_work_handler);
    INIT_DELAYED_WORK(&sdev->workers.group_work, spatial_group_work_handler);
    init_completion(&sdev->workers.sounding_done);

    /* Allocate CSI buffer */
    sdev->csi_data.csi = kzalloc(sizeof(struct wifi7_spatial_csi) * 32,
                                GFP_KERNEL);
    if (!sdev->csi_data.csi) {
        ret = -ENOMEM;
        goto err_free;
    }
    sdev->csi_data.num_entries = 32;

    /* Allocate beam patterns */
    sdev->beamforming.patterns = kzalloc(sizeof(struct wifi7_spatial_beam) *
                                        WIFI7_MAX_BEAM_PATTERNS, GFP_KERNEL);
    if (!sdev->beamforming.patterns) {
        ret = -ENOMEM;
        goto err_csi;
    }

    /* Allocate MU-MIMO groups */
    sdev->mu_mimo.groups = kzalloc(sizeof(struct wifi7_spatial_group) *
                                  WIFI7_MAX_MU_GROUPS, GFP_KERNEL);
    if (!sdev->mu_mimo.groups) {
        ret = -ENOMEM;
        goto err_beam;
    }
    sdev->mu_mimo.num_groups = WIFI7_MAX_MU_GROUPS;

    /* Set default configuration */
    sdev->config.mode = WIFI7_SPATIAL_MODE_AUTO;
    sdev->config.capabilities = WIFI7_SPATIAL_CAP_SU_MIMO |
                               WIFI7_SPATIAL_CAP_MU_MIMO |
                               WIFI7_SPATIAL_CAP_BEAMFORM |
                               WIFI7_SPATIAL_CAP_SOUNDING |
                               WIFI7_SPATIAL_CAP_FEEDBACK |
                               WIFI7_SPATIAL_CAP_DYNAMIC;
    sdev->config.max_streams = 16;
    sdev->config.active_streams = 4;
    sdev->config.min_rssi = -70;
    sdev->config.update_interval = 100;
    sdev->config.auto_adjust = true;
    sdev->config.mu_enabled = true;
    sdev->config.tracking = true;

    sdev->initialized = true;
    dev_info(dev->dev, "Spatial stream management initialized\n");

    return 0;

err_beam:
    kfree(sdev->beamforming.patterns);
err_csi:
    kfree(sdev->csi_data.csi);
err_free:
    kfree(sdev);
    return ret;
}
EXPORT_SYMBOL(wifi7_spatial_init);

void wifi7_spatial_deinit(struct wifi7_dev *dev)
{
    struct wifi7_spatial_dev *sdev = spatial_dev;

    if (!sdev)
        return;

    sdev->initialized = false;

    /* Cancel workers */
    cancel_delayed_work_sync(&sdev->workers.csi_work);
    cancel_delayed_work_sync(&sdev->workers.beam_work);
    cancel_delayed_work_sync(&sdev->workers.group_work);

    kfree(sdev->mu_mimo.groups);
    kfree(sdev->beamforming.patterns);
    kfree(sdev->csi_data.csi);
    kfree(sdev);
    spatial_dev = NULL;

    dev_info(dev->dev, "Spatial stream management deinitialized\n");
}
EXPORT_SYMBOL(wifi7_spatial_deinit);

/* Module interface */
int wifi7_spatial_start(struct wifi7_dev *dev)
{
    struct wifi7_spatial_dev *sdev = spatial_dev;

    if (!sdev || !sdev->initialized)
        return -EINVAL;

    /* Start workers */
    schedule_delayed_work(&sdev->workers.csi_work,
                         msecs_to_jiffies(sdev->config.update_interval));

    if (sdev->config.tracking)
        schedule_delayed_work(&sdev->workers.beam_work,
                            msecs_to_jiffies(sdev->config.beamforming.update_rate));

    if (sdev->config.mu_enabled)
        schedule_delayed_work(&sdev->workers.group_work,
                            msecs_to_jiffies(sdev->config.mu_mimo.timeout));

    dev_info(dev->dev, "Spatial stream management started\n");
    return 0;
}
EXPORT_SYMBOL(wifi7_spatial_start);

void wifi7_spatial_stop(struct wifi7_dev *dev)
{
    struct wifi7_spatial_dev *sdev = spatial_dev;

    if (!sdev || !sdev->initialized)
        return;

    /* Cancel workers */
    cancel_delayed_work_sync(&sdev->workers.csi_work);
    cancel_delayed_work_sync(&sdev->workers.beam_work);
    cancel_delayed_work_sync(&sdev->workers.group_work);

    dev_info(dev->dev, "Spatial stream management stopped\n");
}
EXPORT_SYMBOL(wifi7_spatial_stop);

/* Configuration interface */
int wifi7_spatial_set_config(struct wifi7_dev *dev,
                           struct wifi7_spatial_config *config)
{
    struct wifi7_spatial_dev *sdev = spatial_dev;
    unsigned long flags;

    if (!sdev || !sdev->initialized || !config)
        return -EINVAL;

    if (!is_valid_stream_count(config->max_streams))
        return -EINVAL;

    spin_lock_irqsave(&sdev->lock, flags);
    memcpy(&sdev->config, config, sizeof(*config));
    spin_unlock_irqrestore(&sdev->lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_spatial_set_config);

int wifi7_spatial_get_config(struct wifi7_dev *dev,
                           struct wifi7_spatial_config *config)
{
    struct wifi7_spatial_dev *sdev = spatial_dev;
    unsigned long flags;

    if (!sdev || !sdev->initialized || !config)
        return -EINVAL;

    spin_lock_irqsave(&sdev->lock, flags);
    memcpy(config, &sdev->config, sizeof(*config));
    spin_unlock_irqrestore(&sdev->lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_spatial_get_config);

/* CSI interface */
int wifi7_spatial_update_csi(struct wifi7_dev *dev,
                           struct wifi7_spatial_csi *csi)
{
    struct wifi7_spatial_dev *sdev = spatial_dev;

    if (!sdev || !sdev->initialized || !csi)
        return -EINVAL;

    if (!is_valid_stream_count(csi->num_streams))
        return -EINVAL;

    process_csi_update(sdev, csi);
    return 0;
}
EXPORT_SYMBOL(wifi7_spatial_update_csi);

int wifi7_spatial_get_csi(struct wifi7_dev *dev,
                         struct wifi7_spatial_csi *csi)
{
    struct wifi7_spatial_dev *sdev = spatial_dev;
    unsigned long flags;
    int ret = -ENOENT;
    int i;

    if (!sdev || !sdev->initialized || !csi)
        return -EINVAL;

    spin_lock_irqsave(&sdev->csi_data.lock, flags);

    /* Find most recent CSI entry */
    u32 latest_time = 0;
    int latest_idx = -1;

    for (i = 0; i < sdev->csi_data.num_entries; i++) {
        if (sdev->csi_data.csi[i].timestamp > latest_time) {
            latest_time = sdev->csi_data.csi[i].timestamp;
            latest_idx = i;
        }
    }

    if (latest_idx >= 0) {
        memcpy(csi, &sdev->csi_data.csi[latest_idx], sizeof(*csi));
        ret = 0;
    }

    spin_unlock_irqrestore(&sdev->csi_data.lock, flags);
    return ret;
}
EXPORT_SYMBOL(wifi7_spatial_get_csi);

/* Beamforming interface */
int wifi7_spatial_add_beam(struct wifi7_dev *dev,
                         struct wifi7_spatial_beam *beam)
{
    struct wifi7_spatial_dev *sdev = spatial_dev;

    if (!sdev || !sdev->initialized || !beam)
        return -EINVAL;

    return update_beam_pattern(sdev, beam);
}
EXPORT_SYMBOL(wifi7_spatial_add_beam);

int wifi7_spatial_del_beam(struct wifi7_dev *dev, u8 pattern_id)
{
    struct wifi7_spatial_dev *sdev = spatial_dev;
    unsigned long flags;

    if (!sdev || !sdev->initialized)
        return -EINVAL;

    if (!is_valid_pattern_id(pattern_id))
        return -EINVAL;

    spin_lock_irqsave(&sdev->beamforming.lock, flags);
    memset(&sdev->beamforming.patterns[pattern_id], 0,
           sizeof(struct wifi7_spatial_beam));
    spin_unlock_irqrestore(&sdev->beamforming.lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_spatial_del_beam);

/* MU-MIMO interface */
int wifi7_spatial_create_group(struct wifi7_dev *dev,
                             struct wifi7_spatial_group *group)
{
    struct wifi7_spatial_dev *sdev = spatial_dev;

    if (!sdev || !sdev->initialized || !group)
        return -EINVAL;

    return update_mu_group(sdev, group);
}
EXPORT_SYMBOL(wifi7_spatial_create_group);

int wifi7_spatial_delete_group(struct wifi7_dev *dev, u8 group_id)
{
    struct wifi7_spatial_dev *sdev = spatial_dev;
    unsigned long flags;

    if (!sdev || !sdev->initialized)
        return -EINVAL;

    if (!is_valid_group_id(group_id))
        return -EINVAL;

    spin_lock_irqsave(&sdev->mu_mimo.lock, flags);
    memset(&sdev->mu_mimo.groups[group_id], 0,
           sizeof(struct wifi7_spatial_group));
    spin_unlock_irqrestore(&sdev->mu_mimo.lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_spatial_delete_group);

/* Statistics interface */
int wifi7_spatial_get_stats(struct wifi7_dev *dev,
                          struct wifi7_spatial_stats *stats)
{
    struct wifi7_spatial_dev *sdev = spatial_dev;
    unsigned long flags;

    if (!sdev || !sdev->initialized || !stats)
        return -EINVAL;

    spin_lock_irqsave(&sdev->lock, flags);
    memcpy(stats, &sdev->stats, sizeof(*stats));
    spin_unlock_irqrestore(&sdev->lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_spatial_get_stats);

int wifi7_spatial_clear_stats(struct wifi7_dev *dev)
{
    struct wifi7_spatial_dev *sdev = spatial_dev;
    unsigned long flags;

    if (!sdev || !sdev->initialized)
        return -EINVAL;

    spin_lock_irqsave(&sdev->lock, flags);
    memset(&sdev->stats, 0, sizeof(sdev->stats));
    spin_unlock_irqrestore(&sdev->lock, flags);

    return 0;
}
EXPORT_SYMBOL(wifi7_spatial_clear_stats);

/* Module parameters */
static bool spatial_auto_adjust = true;
module_param(spatial_auto_adjust, bool, 0644);
MODULE_PARM_DESC(spatial_auto_adjust, "Enable automatic stream adjustment");

static bool spatial_mu_enable = true;
module_param(spatial_mu_enable, bool, 0644);
MODULE_PARM_DESC(spatial_mu_enable, "Enable MU-MIMO");

static bool spatial_tracking = true;
module_param(spatial_tracking, bool, 0644);
MODULE_PARM_DESC(spatial_tracking, "Enable beam tracking");

/* Module initialization */
static int __init wifi7_spatial_init_module(void)
{
    pr_info("WiFi 7 spatial stream management loaded\n");
    return 0;
}

static void __exit wifi7_spatial_exit_module(void)
{
    pr_info("WiFi 7 spatial stream management unloaded\n");
}

module_init(wifi7_spatial_init_module);
module_exit(wifi7_spatial_exit_module);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri <fayssalchokri@gmail.com>");
MODULE_DESCRIPTION("WiFi 7 Spatial Stream Management");
MODULE_VERSION("1.0"); 