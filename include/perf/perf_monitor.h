#ifndef _WIFI67_PERF_MONITOR_H_
#define _WIFI67_PERF_MONITOR_H_

#include <linux/types.h>
#include "../core/wifi67.h"

#define WIFI67_PERF_SAMPLE_INTERVAL 1000 // 1 second default

int wifi67_perf_init(struct wifi67_priv *priv);
void wifi67_perf_deinit(struct wifi67_priv *priv);
void wifi67_perf_sample(struct wifi67_priv *priv);

static inline void wifi67_perf_tx_packet(struct wifi67_priv *priv, size_t len)
{
    atomic_inc(&priv->perf.tx_packets);
    atomic_add(len, &priv->perf.tx_bytes);
}

static inline void wifi67_perf_rx_packet(struct wifi67_priv *priv, size_t len)
{
    atomic_inc(&priv->perf.rx_packets);
    atomic_add(len, &priv->perf.rx_bytes);
}

static inline void wifi67_perf_tx_error(struct wifi67_priv *priv)
{
    atomic_inc(&priv->perf.tx_errors);
}

static inline void wifi67_perf_rx_error(struct wifi67_priv *priv)
{
    atomic_inc(&priv->perf.rx_errors);
}

#endif /* _WIFI67_PERF_MONITOR_H_ */ 