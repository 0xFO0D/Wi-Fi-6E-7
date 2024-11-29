#ifndef _WIFI67_HARDWARE_H_
#define _WIFI67_HARDWARE_H_

#include <linux/types.h>
#include "../core/wifi67.h"

/* Function Declarations */
int wifi67_hw_init(struct wifi67_priv *priv);
void wifi67_hw_deinit(struct wifi67_priv *priv);
void wifi67_hw_irq_enable(struct wifi67_priv *priv);
void wifi67_hw_irq_disable(struct wifi67_priv *priv);
irqreturn_t wifi67_hw_interrupt(int irq, void *dev);

#endif /* _WIFI67_HARDWARE_H_ */ 