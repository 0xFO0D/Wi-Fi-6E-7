#ifndef _WIFI67_HW_DIAG_H_
#define _WIFI67_HW_DIAG_H_

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>

/* Diagnostic test masks */
#define WIFI67_DIAG_TEST_REG  BIT(0)  /* Register access tests */
#define WIFI67_DIAG_TEST_MEM  BIT(1)  /* Memory tests */
#define WIFI67_DIAG_TEST_DMA  BIT(2)  /* DMA tests */
#define WIFI67_DIAG_TEST_IRQ  BIT(3)  /* Interrupt tests */
#define WIFI67_DIAG_TEST_PHY  BIT(4)  /* PHY tests */
#define WIFI67_DIAG_TEST_MAC  BIT(5)  /* MAC tests */
#define WIFI67_DIAG_TEST_RF   BIT(6)  /* RF tests */
#define WIFI67_DIAG_TEST_ALL  0xFF    /* All tests */

/* Diagnostic states */
enum wifi67_diag_state {
    WIFI67_DIAG_STATE_IDLE,
    WIFI67_DIAG_STATE_RUNNING,
    WIFI67_DIAG_STATE_ERROR,
    WIFI67_DIAG_STATE_COMPLETE
};

/* Hardware diagnostic structure */
struct wifi67_hw_diag {
    struct delayed_work dwork;
    spinlock_t lock;
    atomic_t running;
    u32 test_mask;
    enum wifi67_diag_state state;
    u32 errors;
    u32 last_error;
    char error_msg[256];
    
    /* Test results */
    struct {
        u32 reg_errors;
        u32 mem_errors;
        u32 dma_errors;
        u32 irq_errors;
        u32 phy_errors;
        u32 mac_errors;
        u32 rf_errors;
    } results;
};

int wifi67_hw_diag_init(struct wifi67_priv *priv);
void wifi67_hw_diag_deinit(struct wifi67_priv *priv);
int wifi67_hw_diag_start(struct wifi67_priv *priv);
void wifi67_hw_diag_stop(struct wifi67_priv *priv);

#endif /* _WIFI67_HW_DIAG_H_ */ 