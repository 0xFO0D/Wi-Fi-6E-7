#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include "../../include/firmware/emlfm.h"
#include "emlrc_fw.h"

/* EMLRC firmware command handlers */
static int wifi67_emlrc_fw_handle_init(struct wifi67_priv *priv,
                                     struct wifi67_emlrc_fw_cmd *cmd)
{
    u32 val;

    /* Configure EMLRC hardware registers */
    val = wifi67_hw_read32(priv, WIFI67_REG_EMLRC_CTRL);
    val |= WIFI67_EMLRC_CTRL_ENABLE;
    wifi67_hw_write32(priv, WIFI67_REG_EMLRC_CTRL, val);

    return 0;
}

static int wifi67_emlrc_fw_handle_deinit(struct wifi67_priv *priv,
                                       struct wifi67_emlrc_fw_cmd *cmd)
{
    u32 val;

    /* Disable EMLRC hardware */
    val = wifi67_hw_read32(priv, WIFI67_REG_EMLRC_CTRL);
    val &= ~WIFI67_EMLRC_CTRL_ENABLE;
    wifi67_hw_write32(priv, WIFI67_REG_EMLRC_CTRL, val);

    return 0;
}

static int wifi67_emlrc_fw_handle_set_params(struct wifi67_priv *priv,
                                           struct wifi67_emlrc_fw_cmd *cmd)
{
    struct wifi67_emlrc_fw_params *params = &cmd->params;
    u32 val;

    /* Update hardware parameters */
    val = le32_to_cpu(params->update_interval);
    wifi67_hw_write32(priv, WIFI67_REG_EMLRC_UPDATE_INT, val);

    val = le32_to_cpu(params->probe_interval);
    wifi67_hw_write32(priv, WIFI67_REG_EMLRC_PROBE_INT, val);

    val = le32_to_cpu(params->scaling_factor);
    wifi67_hw_write32(priv, WIFI67_REG_EMLRC_SCALING, val);

    val = le32_to_cpu(params->probing_enabled);
    if (val)
        val = WIFI67_EMLRC_CTRL_PROBE_EN;
    wifi67_hw_write32(priv, WIFI67_REG_EMLRC_CTRL, val);

    return 0;
}

static int wifi67_emlrc_fw_handle_get_stats(struct wifi67_priv *priv,
                                          struct wifi67_emlrc_fw_cmd *cmd)
{
    struct wifi67_emlrc_fw_event evt;
    struct wifi67_emlrc_fw_stats *stats = &evt.stats;

    /* Read hardware statistics */
    stats->attempts = cpu_to_le32(wifi67_hw_read32(priv,
                                WIFI67_REG_EMLRC_ATTEMPTS));
    stats->successes = cpu_to_le32(wifi67_hw_read32(priv,
                                 WIFI67_REG_EMLRC_SUCCESSES));
    stats->retries = cpu_to_le32(wifi67_hw_read32(priv,
                               WIFI67_REG_EMLRC_RETRIES));
    stats->failures = cpu_to_le32(wifi67_hw_read32(priv,
                                WIFI67_REG_EMLRC_FAILURES));
    stats->throughput = cpu_to_le32(wifi67_hw_read32(priv,
                                  WIFI67_REG_EMLRC_TPUT));
    stats->ewma_prob = cpu_to_le32(wifi67_hw_read32(priv,
                                 WIFI67_REG_EMLRC_PROB));
    stats->timestamp = cpu_to_le32(jiffies_to_msecs(jiffies));

    /* Send statistics event */
    evt.evt_id = cpu_to_le16(WIFI67_EMLRC_EVT_STATS);
    evt.len = cpu_to_le16(sizeof(*stats));
    evt.flags = 0;

    return wifi67_fw_send_event(priv, &evt, sizeof(evt));
}

static int wifi67_emlrc_fw_handle_update(struct wifi67_priv *priv,
                                       struct wifi67_emlrc_fw_cmd *cmd)
{
    u32 val;

    /* Trigger hardware update */
    val = wifi67_hw_read32(priv, WIFI67_REG_EMLRC_CTRL);
    val |= WIFI67_EMLRC_CTRL_UPDATE;
    wifi67_hw_write32(priv, WIFI67_REG_EMLRC_CTRL, val);

    return 0;
}

/* EMLRC firmware command dispatcher */
int wifi67_emlrc_fw_handle_cmd(struct wifi67_priv *priv,
                             struct wifi67_emlrc_fw_cmd *cmd)
{
    int ret;

    switch (le16_to_cpu(cmd->cmd_id)) {
    case WIFI67_EMLRC_CMD_INIT:
        ret = wifi67_emlrc_fw_handle_init(priv, cmd);
        break;
    case WIFI67_EMLRC_CMD_DEINIT:
        ret = wifi67_emlrc_fw_handle_deinit(priv, cmd);
        break;
    case WIFI67_EMLRC_CMD_SET_PARAMS:
        ret = wifi67_emlrc_fw_handle_set_params(priv, cmd);
        break;
    case WIFI67_EMLRC_CMD_GET_STATS:
        ret = wifi67_emlrc_fw_handle_get_stats(priv, cmd);
        break;
    case WIFI67_EMLRC_CMD_UPDATE:
        ret = wifi67_emlrc_fw_handle_update(priv, cmd);
        break;
    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

EXPORT_SYMBOL(wifi67_emlrc_fw_handle_cmd); 