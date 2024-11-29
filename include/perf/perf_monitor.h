#ifndef _WIFI67_PERF_MONITOR_H_
#define _WIFI67_PERF_MONITOR_H_

#include "../core/wifi67_forward.h"
#include "perf_types.h"

/* Performance monitoring function prototypes */
int wifi67_perf_init(struct wifi67_priv *priv);
void wifi67_perf_deinit(struct wifi67_priv *priv);
void wifi67_perf_sample(struct wifi67_priv *priv);
void wifi67_perf_update_tx(struct wifi67_priv *priv, u32 bytes, u32 latency);
void wifi67_perf_update_rx(struct wifi67_priv *priv, u32 bytes, u32 latency);

#endif /* _WIFI67_PERF_MONITOR_H_ */ 