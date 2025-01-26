#ifndef _WIFI67_DMA_MONITOR_H_
#define _WIFI67_DMA_MONITOR_H_

#include <linux/types.h>
#include "../core/wifi67.h"

/* DMA Error Types */
#define DMA_ERR_NONE              0x00000000
#define DMA_ERR_DESC_OWNERSHIP    0x00000001
#define DMA_ERR_INVALID_LEN       0x00000002
#define DMA_ERR_RING_FULL         0x00000004
#define DMA_ERR_RING_EMPTY        0x00000008
#define DMA_ERR_INVALID_ADDR      0x00000010
#define DMA_ERR_TIMEOUT           0x00000020
#define DMA_ERR_BUS_ERROR         0x00000040
#define DMA_ERR_DATA_ERROR        0x00000080
#define DMA_ERR_DESC_ERROR        0x00000100
#define DMA_ERR_FIFO_ERROR        0x00000200
#define DMA_ERR_HARDWARE          0x00000400
#define DMA_ERR_WATCHDOG          0x00000800
#define DMA_ERR_FATAL             0x80000000

/* DMA Recovery Actions */
#define DMA_RECOVER_NONE          0x00000000
#define DMA_RECOVER_RESET         0x00000001
#define DMA_RECOVER_REINIT        0x00000002
#define DMA_RECOVER_RESTART       0x00000004
#define DMA_RECOVER_RELOAD        0x00000008

/* DMA Channel States */
enum wifi67_dma_channel_state {
    DMA_CHANNEL_UNKNOWN,
    DMA_CHANNEL_STOPPED,
    DMA_CHANNEL_RUNNING,
    DMA_CHANNEL_ERROR,
    DMA_CHANNEL_RECOVERY,
    DMA_CHANNEL_SUSPENDED
};

/* DMA Performance Levels */
enum wifi67_dma_perf_level {
    DMA_PERF_LOW,
    DMA_PERF_MEDIUM,
    DMA_PERF_HIGH,
    DMA_PERF_MAX
};

/* DMA Error Recovery Policy */
struct wifi67_dma_recovery_policy {
    u32 error_mask;           /* Which errors to handle */
    u32 recovery_action;      /* What action to take */
    u32 max_retries;         /* Maximum recovery attempts */
    u32 retry_interval_ms;   /* Time between retries */
    bool auto_recover;       /* Attempt recovery automatically */
};

/* DMA Performance Policy */
struct wifi67_dma_perf_policy {
    enum wifi67_dma_perf_level level;
    u32 burst_size;          /* DMA burst size */
    u32 desc_threshold;      /* Descriptor threshold for interrupt */
    u32 timeout_us;          /* Timeout in microseconds */
    bool adaptive_tuning;    /* Enable adaptive performance tuning */
};

/* Core monitoring functions */
int wifi67_dma_monitor_init(struct wifi67_priv *priv);
void wifi67_dma_monitor_deinit(struct wifi67_priv *priv);
void wifi67_dma_monitor_irq(struct wifi67_priv *priv, u32 channel_id, bool is_error);
void wifi67_dma_monitor_ring_full(struct wifi67_priv *priv, u32 channel_id);

/* Error handling and recovery */
int wifi67_dma_set_recovery_policy(struct wifi67_priv *priv, u32 channel_id,
                                  const struct wifi67_dma_recovery_policy *policy);
int wifi67_dma_get_recovery_policy(struct wifi67_priv *priv, u32 channel_id,
                                  struct wifi67_dma_recovery_policy *policy);
int wifi67_dma_handle_error(struct wifi67_priv *priv, u32 channel_id,
                           u32 error_type);
int wifi67_dma_force_recovery(struct wifi67_priv *priv, u32 channel_id,
                             u32 recovery_action);

/* Performance monitoring and tuning */
int wifi67_dma_set_perf_policy(struct wifi67_priv *priv, u32 channel_id,
                              const struct wifi67_dma_perf_policy *policy);
int wifi67_dma_get_perf_policy(struct wifi67_priv *priv, u32 channel_id,
                              struct wifi67_dma_perf_policy *policy);
int wifi67_dma_get_channel_state(struct wifi67_priv *priv, u32 channel_id,
                                enum wifi67_dma_channel_state *state);

/* Debug interface control */
int wifi67_dma_monitor_suspend(struct wifi67_priv *priv);
int wifi67_dma_monitor_resume(struct wifi67_priv *priv);
int wifi67_dma_monitor_reset_stats(struct wifi67_priv *priv, u32 channel_id);

#endif /* _WIFI67_DMA_MONITOR_H_ */ 