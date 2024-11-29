#ifndef _WIFI67_HARDWARE_H_
#define _WIFI67_HARDWARE_H_

#include <linux/types.h>
#include "../core/wifi67_forward.h"

/* Hardware capabilities */
#define WIFI67_HW_CAP_BE          BIT(0)
#define WIFI67_HW_CAP_320MHZ      BIT(1)
#define WIFI67_HW_CAP_4K_QAM      BIT(2)
#define WIFI67_HW_CAP_MLO         BIT(3)
#define WIFI67_HW_CAP_MU_MIMO     BIT(4)
#define WIFI67_HW_CAP_OFDMA       BIT(5)

/* Hardware registers */
#define WIFI67_HW_VERSION         0x0000
#define WIFI67_HW_CONFIG          0x0004
#define WIFI67_HW_STATUS          0x0008
#define WIFI67_HW_INT_STATUS      0x000C
#define WIFI67_HW_INT_MASK        0x0010

/* Hardware states */
enum wifi67_hw_state {
    WIFI67_HW_OFF,
    WIFI67_HW_BOOTING,
    WIFI67_HW_READY,
    WIFI67_HW_RUNNING,
    WIFI67_HW_ERROR
};

struct wifi67_hw {
    /* Hardware information */
    u32 version;
    u32 capabilities;
    enum wifi67_hw_state state;
    
    /* Base registers */
    void __iomem *regs;
    
    /* Interrupt handling */
    u32 irq_mask;
    spinlock_t irq_lock;
};

/* Function declarations */
int wifi67_hw_init(struct wifi67_priv *priv);
void wifi67_hw_deinit(struct wifi67_priv *priv);
int wifi67_hw_start(struct wifi67_priv *priv);
void wifi67_hw_stop(struct wifi67_priv *priv);
bool wifi67_hw_check_version(struct wifi67_priv *priv);
void wifi67_hw_irq_enable(struct wifi67_priv *priv);
void wifi67_hw_irq_disable(struct wifi67_priv *priv);
irqreturn_t wifi67_hw_interrupt(int irq, void *data);

#endif /* _WIFI67_HARDWARE_H_ */ 