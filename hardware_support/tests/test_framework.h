#ifndef _TEST_FRAMEWORK_H_
#define _TEST_FRAMEWORK_H_

#include <linux/types.h>

/* Test result enum */
enum test_result {
    TEST_NOT_RUN = 0,
    TEST_PASS,
    TEST_FAIL,
    TEST_SKIP
};

/* Test function type */
typedef int (*test_func_t)(void *data);

/* Test flags */
#define TEST_FLAG_ASYNC      (1 << 0)  /* Run test asynchronously */
#define TEST_FLAG_PRIVILEGED (1 << 1)  /* Test requires elevated privileges */
#define TEST_FLAG_HARDWARE   (1 << 2)  /* Test requires hardware access */
#define TEST_FLAG_NETWORK    (1 << 3)  /* Test requires network access */
#define TEST_FLAG_BENCHMARK  (1 << 4)  /* Test is a performance benchmark */
#define TEST_FLAG_STRESS     (1 << 5)  /* Test is a stress test */
#define TEST_FLAG_SLOW       (1 << 6)  /* Test takes a long time to run */
#define TEST_FLAG_DANGEROUS  (1 << 7)  /* Test may affect system stability */
#define TEST_FLAG_MANUAL     (1 << 8)  /* Test requires manual intervention */
#define TEST_FLAG_OPTIONAL   (1 << 9)  /* Test is optional */

/* Test results structure */
struct test_results {
    int total;
    int passed;
    int failed;
    int skipped;
    s64 duration_ns;
};

/* Test framework functions */
int test_framework_init(void);
void test_framework_exit(void);

int register_test_case(const char *name, const char *description,
                      test_func_t test_func, void *test_data,
                      unsigned long flags);

int run_test_case(const char *name);
void run_all_tests(void);
void get_test_results(struct test_results *results);
void set_test_error(const char *name, const char *fmt, ...);
void reset_test_framework(void);

/* Helper macros */
#define REGISTER_TEST(name, desc, func, data, flags) \
    register_test_case(name, desc, func, data, flags)

#define TEST_ASSERT(cond, fmt, ...) do { \
    if (!(cond)) { \
        set_test_error(__func__, fmt, ##__VA_ARGS__); \
        return TEST_FAIL; \
    } \
} while (0)

#define TEST_SKIP(fmt, ...) do { \
    set_test_error(__func__, fmt, ##__VA_ARGS__); \
    return TEST_SKIP; \
} while (0)

#define TEST_PASS() return TEST_PASS

/* Test categories */
#define TEST_CAT_CORE      "core"
#define TEST_CAT_DMA       "dma"
#define TEST_CAT_MAC       "mac"
#define TEST_CAT_PHY       "phy"
#define TEST_CAT_FIRMWARE  "firmware"
#define TEST_CAT_CRYPTO    "crypto"
#define TEST_CAT_POWER     "power"
#define TEST_CAT_PERF      "performance"
#define TEST_CAT_STRESS    "stress"
#define TEST_CAT_HARDWARE  "hardware"

/* Test priorities */
#define TEST_PRIO_CRITICAL 0
#define TEST_PRIO_HIGH     1
#define TEST_PRIO_MEDIUM   2
#define TEST_PRIO_LOW      3

/* Test timeouts (in milliseconds) */
#define TEST_TIMEOUT_SHORT  1000
#define TEST_TIMEOUT_MEDIUM 5000
#define TEST_TIMEOUT_LONG   30000

/* Test retry counts */
#define TEST_RETRY_NONE    0
#define TEST_RETRY_ONCE    1
#define TEST_RETRY_TWICE   2
#define TEST_RETRY_MAX     5

/* Test buffer sizes */
#define TEST_BUFFER_SMALL  1024
#define TEST_BUFFER_MEDIUM 4096
#define TEST_BUFFER_LARGE  16384

/* Test iteration counts */
#define TEST_ITER_QUICK    10
#define TEST_ITER_NORMAL   100
#define TEST_ITER_EXTENDED 1000
#define TEST_ITER_STRESS   10000

/* Test thresholds */
#define TEST_THRESH_STRICT 100    /* 100% pass required */
#define TEST_THRESH_HIGH   99     /* 99% pass required */
#define TEST_THRESH_MEDIUM 95     /* 95% pass required */
#define TEST_THRESH_LOW    90     /* 90% pass required */

#endif /* _TEST_FRAMEWORK_H_ */ 