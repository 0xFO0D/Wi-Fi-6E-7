#ifndef _WIFI67_H_
#define _WIFI67_H_

#include "wifi67_types.h"
#include "wifi67_stats.h"

/* Function declarations */
int wifi67_init_device(struct wifi67_priv *priv);
void wifi67_deinit_device(struct wifi67_priv *priv);

#endif /* _WIFI67_H_ */ 