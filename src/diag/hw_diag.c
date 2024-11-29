#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include "../../include/core/wifi67.h"
#include "../../include/diag/hw_diag.h"

static void wifi67_hw_diag_work(struct work_struct *work)
{
    struct wifi67_hw_diag *diag = container_of(to_delayed_work(work),
                                             struct wifi67_hw_diag,
                                             dwork);
    struct wifi67_priv *priv = container_of(diag, struct wifi67_priv, hw_diag);
    unsigned long flags;

    if (!atomic_read(&diag->running))
        return;

    spin_lock_irqsave(&diag->lock, flags);
    
    /* Run enabled diagnostic tests */
    if (diag->test_mask & WIFI67_DIAG_TEST_REG) {
        /* Register access test */
        // ... implementation ...
    }
    
    if (diag->test_mask & WIFI67_DIAG_TEST_MEM) {
        /* Memory test */
        // ... implementation ...
    }
    
    spin_unlock_irqrestore(&diag->lock, flags);

    /* Schedule next diagnostic run */
    if (atomic_read(&diag->running)) {
        schedule_delayed_work(&diag->dwork, HZ);
    }
}

int wifi67_hw_diag_init(struct wifi67_priv *priv)
{
    struct wifi67_hw_diag *diag = &priv->hw_diag;

    spin_lock_init(&diag->lock);
    atomic_set(&diag->running, 0);
    diag->test_mask = WIFI67_DIAG_TEST_ALL;
    diag->state = WIFI67_DIAG_STATE_IDLE;
    diag->errors = 0;
    diag->last_error = 0;
    memset(diag->error_msg, 0, sizeof(diag->error_msg));

    INIT_DELAYED_WORK(&diag->dwork, wifi67_hw_diag_work);

    return 0;
}

void wifi67_hw_diag_deinit(struct wifi67_priv *priv)
{
    struct wifi67_hw_diag *diag = &priv->hw_diag;

    atomic_set(&diag->running, 0);
    cancel_delayed_work_sync(&diag->dwork);
} 