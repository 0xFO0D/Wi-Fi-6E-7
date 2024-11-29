#ifndef _WIFI67_UTILS_H_
#define _WIFI67_UTILS_H_

#include <linux/types.h>
#include <linux/io.h>
#include <linux/delay.h>

static inline int wifi67_wait_bit(void __iomem *addr, u32 mask, unsigned int timeout)
{
    unsigned long end = jiffies + msecs_to_jiffies(timeout);
    u32 val;

    do {
        val = readl(addr);
        if (val & mask)
            return 0;
        
        cpu_relax();
        udelay(10);
    } while (time_before(jiffies, end));

    return -ETIMEDOUT;
}

#endif /* _WIFI67_UTILS_H_ */ 