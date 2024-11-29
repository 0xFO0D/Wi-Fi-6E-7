#ifndef _WIFI67_DIAG_TYPES_H_
#define _WIFI67_DIAG_TYPES_H_

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>

/* Hardware diagnostic states */
#define WIFI67_DIAG_STATE_IDLE      0
#define WIFI67_DIAG_STATE_RUNNING   1
#define WIFI67_DIAG_STATE_ERROR     2

/* Hardware diagnostic test types */
#define WIFI67_DIAG_TEST_REG        BIT(0)
#define WIFI67_DIAG_TEST_MEM        BIT(1)
#define WIFI67_DIAG_TEST_INT        BIT(2)
#define WIFI67_DIAG_TEST_DMA        BIT(3)
#define WIFI67_DIAG_TEST_ALL        0xFFFFFFFF

struct wifi67_hw_diag {
    /* Test configuration */
    u32 test_mask;
    u32 state;
    
    /* Results */
    u32 errors;
    u32 last_error;
    char error_msg[256];
    
    /* Work queue */
    struct delayed_work dwork;
    atomic_t running;
    
    /* Locks */
    spinlock_t lock;
};

#endif /* _WIFI67_DIAG_TYPES_H_ */ 