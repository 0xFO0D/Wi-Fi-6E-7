#ifndef __WIFI67_EMLRC_FW_H
#define __WIFI67_EMLRC_FW_H

/* EMLRC firmware command IDs */
#define WIFI67_EMLRC_CMD_INIT       0x01
#define WIFI67_EMLRC_CMD_DEINIT     0x02
#define WIFI67_EMLRC_CMD_SET_PARAMS 0x03
#define WIFI67_EMLRC_CMD_GET_STATS  0x04
#define WIFI67_EMLRC_CMD_UPDATE     0x05

/* EMLRC firmware event IDs */
#define WIFI67_EMLRC_EVT_READY      0x81
#define WIFI67_EMLRC_EVT_ERROR      0x82
#define WIFI67_EMLRC_EVT_STATS      0x83
#define WIFI67_EMLRC_EVT_UPDATE     0x84

/* EMLRC firmware parameter IDs */
#define WIFI67_EMLRC_PARAM_INTERVAL 0x01
#define WIFI67_EMLRC_PARAM_THRESH   0x02
#define WIFI67_EMLRC_PARAM_SCALING  0x03
#define WIFI67_EMLRC_PARAM_PROBING  0x04

/* EMLRC firmware structures */
struct wifi67_emlrc_fw_stats {
    __le32 attempts;
    __le32 successes;
    __le32 retries;
    __le32 failures;
    __le32 throughput;
    __le32 ewma_prob;
    __le32 timestamp;
} __packed;

struct wifi67_emlrc_fw_params {
    __le32 update_interval;
    __le32 probe_interval;
    __le32 scaling_factor;
    __le32 probing_enabled;
    __le32 reserved[4];
} __packed;

struct wifi67_emlrc_fw_cmd {
    __le16 cmd_id;
    __le16 len;
    __le32 flags;
    union {
        struct wifi67_emlrc_fw_params params;
        u8 data[0];
    };
} __packed;

struct wifi67_emlrc_fw_event {
    __le16 evt_id;
    __le16 len;
    __le32 flags;
    union {
        struct wifi67_emlrc_fw_stats stats;
        u8 data[0];
    };
} __packed;

#endif /* __WIFI67_EMLRC_FW_H */ 