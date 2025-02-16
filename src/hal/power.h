#ifndef __WIFI67_POWER_H
#define __WIFI67_POWER_H

/* Power management registers */
#define WIFI67_REG_PWR_CTRL        0x0400
#define WIFI67_REG_PWR_STATUS      0x0404
#define WIFI67_REG_PWR_INT         0x0408
#define WIFI67_REG_PWR_MASK        0x040C

/* Per-radio power registers */
#define WIFI67_REG_RADIO_PWR       0x0410
#define WIFI67_REG_RADIO_SLEEP     0x0414
#define WIFI67_REG_RADIO_WAKE      0x0418
#define WIFI67_REG_RADIO_VOLT      0x041C

/* Control register bits */
#define WIFI67_PWR_CTRL_ENABLE     BIT(0)
#define WIFI67_PWR_CTRL_SLEEP      BIT(1)
#define WIFI67_PWR_CTRL_DEEP_SLEEP BIT(2)
#define WIFI67_PWR_CTRL_RESET      BIT(3)

/* Status register bits */
#define WIFI67_PWR_STATUS_ACTIVE   BIT(0)
#define WIFI67_PWR_STATUS_SLEEPING BIT(1)
#define WIFI67_PWR_STATUS_ERROR    BIT(2)

/* Radio power states */
#define WIFI67_RADIO_PWR_ON        0x01
#define WIFI67_RADIO_PWR_SLEEP     0x02
#define WIFI67_RADIO_PWR_OFF       0x03

/* Sleep modes */
#define WIFI67_SLEEP_LIGHT         0x01
#define WIFI67_SLEEP_DEEP          0x02

struct wifi67_power_stats {
    u32 sleep_count;
    u32 wake_count;
    u32 total_sleep_time;
    u32 last_sleep_duration;
    u32 voltage_level;
    u32 current_draw;
} __packed;

int wifi67_power_init(struct wifi67_priv *priv);
void wifi67_power_deinit(struct wifi67_priv *priv);
int wifi67_radio_sleep(struct wifi67_priv *priv, u8 radio_id, u8 sleep_mode);
int wifi67_radio_wake(struct wifi67_priv *priv, u8 radio_id);
int wifi67_get_power_stats(struct wifi67_priv *priv, u8 radio_id,
                         struct wifi67_power_stats *stats);

#endif /* __WIFI67_POWER_H */ 