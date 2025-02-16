#ifndef __WIFI67_METRICS_H
#define __WIFI67_METRICS_H

/* Hardware metrics registers */
#define WIFI67_REG_METRIC_CTRL     0x0300
#define WIFI67_REG_METRIC_STATUS   0x0304
#define WIFI67_REG_METRIC_INT      0x0308
#define WIFI67_REG_METRIC_MASK     0x030C

/* Per-radio metric registers */
#define WIFI67_REG_RADIO_RSSI      0x0310
#define WIFI67_REG_RADIO_NOISE     0x0314
#define WIFI67_REG_RADIO_SNR       0x0318
#define WIFI67_REG_RADIO_TEMP      0x031C
#define WIFI67_REG_RADIO_TXPOWER   0x0320
#define WIFI67_REG_RADIO_BUSY      0x0324

/* Per-link metric registers */
#define WIFI67_REG_LINK_QUALITY    0x0330
#define WIFI67_REG_LINK_AIRTIME    0x0334
#define WIFI67_REG_LINK_LATENCY    0x0338
#define WIFI67_REG_LINK_JITTER     0x033C
#define WIFI67_REG_LINK_LOSS       0x0340

/* Control register bits */
#define WIFI67_METRIC_CTRL_ENABLE  BIT(0)
#define WIFI67_METRIC_CTRL_RESET   BIT(1)
#define WIFI67_METRIC_CTRL_AUTO    BIT(2)
#define WIFI67_METRIC_CTRL_SAMPLE  BIT(3)

/* Status register bits */
#define WIFI67_METRIC_STATUS_READY BIT(0)
#define WIFI67_METRIC_STATUS_BUSY  BIT(1)
#define WIFI67_METRIC_STATUS_ERROR BIT(2)

/* Interrupt bits */
#define WIFI67_METRIC_INT_DONE     BIT(0)
#define WIFI67_METRIC_INT_ERROR    BIT(1)
#define WIFI67_METRIC_INT_THRESH   BIT(2)

struct wifi67_radio_metrics {
    s8 rssi;
    s8 noise;
    u8 snr;
    u8 temperature;
    u8 tx_power;
    u8 busy_percent;
} __packed;

struct wifi67_link_metrics {
    u8 quality;
    u8 airtime;
    u16 latency;
    u16 jitter;
    u8 loss_percent;
    u8 reserved;
} __packed;

int wifi67_metrics_init(struct wifi67_priv *priv);
void wifi67_metrics_deinit(struct wifi67_priv *priv);
int wifi67_metrics_start(struct wifi67_priv *priv);
void wifi67_metrics_stop(struct wifi67_priv *priv);
int wifi67_get_radio_metrics(struct wifi67_priv *priv, u8 radio_id,
                           struct wifi67_radio_metrics *metrics);
int wifi67_get_link_metrics(struct wifi67_priv *priv, u8 link_id,
                          struct wifi67_link_metrics *metrics);

#endif /* __WIFI67_METRICS_H */ 