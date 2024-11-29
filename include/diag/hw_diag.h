#ifndef _WIFI67_HW_DIAG_H_
#define _WIFI67_HW_DIAG_H_

#include <linux/types.h>
#include "../core/wifi67_types.h"

/* Function prototypes only */
int wifi67_hw_diag_init(struct wifi67_priv *priv);
void wifi67_hw_diag_deinit(struct wifi67_priv *priv);

#endif /* _WIFI67_HW_DIAG_H_ */ 