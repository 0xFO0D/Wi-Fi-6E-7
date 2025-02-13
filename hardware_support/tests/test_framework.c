#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include "test_framework.h"

/* Test case management */
static LIST_HEAD(test_cases);
static DEFINE_MUTEX(test_lock);
static struct workqueue_struct *test_wq;
static struct dentry *test_dir;

/* Test statistics */
static struct test_stats {
    atomic_t total_tests;
    atomic_t passed_tests;
    atomic_t failed_tests;
    atomic_t skipped_tests;
    ktime_t start_time;
    ktime_t end_time;
} stats;

/* Test case structure */
struct test_case {
    struct list_head list;
    const char *name;
    const char *description;
    test_func_t test_func;
    void *test_data;
    struct completion done;
    enum test_result result;
    char *error_msg;
    ktime_t start_time;
    ktime_t end_time;
    unsigned long flags;
};

/* Test work structure */
struct test_work {
    struct work_struct work;
    struct test_case *test;
};

/* Initialize test framework */
int test_framework_init(void)
{
    test_wq = alloc_workqueue("wifi67_test", WQ_HIGHPRI | WQ_UNBOUND, 0);
    if (!test_wq)
        return -ENOMEM;

    test_dir = debugfs_create_dir("wifi67_test", NULL);
    if (IS_ERR(test_dir)) {
        destroy_workqueue(test_wq);
        return PTR_ERR(test_dir);
    }

    /* Create debugfs entries */
    debugfs_create_atomic_t("total_tests", 0444, test_dir,
                           &stats.total_tests);
    debugfs_create_atomic_t("passed_tests", 0444, test_dir,
                           &stats.passed_tests);
    debugfs_create_atomic_t("failed_tests", 0444, test_dir,
                           &stats.failed_tests);
    debugfs_create_atomic_t("skipped_tests", 0444, test_dir,
                           &stats.skipped_tests);

    /* Initialize statistics */
    atomic_set(&stats.total_tests, 0);
    atomic_set(&stats.passed_tests, 0);
    atomic_set(&stats.failed_tests, 0);
    atomic_set(&stats.skipped_tests, 0);

    return 0;
}

/* Cleanup test framework */
void test_framework_exit(void)
{
    struct test_case *test, *tmp;

    mutex_lock(&test_lock);
    list_for_each_entry_safe(test, tmp, &test_cases, list) {
        list_del(&test->list);
        kfree(test->error_msg);
        kfree(test);
    }
    mutex_unlock(&test_lock);

    debugfs_remove_recursive(test_dir);
    destroy_workqueue(test_wq);
}

/* Register a test case */
int register_test_case(const char *name, const char *description,
                      test_func_t test_func, void *test_data,
                      unsigned long flags)
{
    struct test_case *test;

    if (!name || !test_func)
        return -EINVAL;

    test = kzalloc(sizeof(*test), GFP_KERNEL);
    if (!test)
        return -ENOMEM;

    test->name = name;
    test->description = description;
    test->test_func = test_func;
    test->test_data = test_data;
    test->flags = flags;
    init_completion(&test->done);

    mutex_lock(&test_lock);
    list_add_tail(&test->list, &test_cases);
    atomic_inc(&stats.total_tests);
    mutex_unlock(&test_lock);

    return 0;
}

/* Test execution worker */
static void test_worker(struct work_struct *work)
{
    struct test_work *test_work = container_of(work, struct test_work, work);
    struct test_case *test = test_work->test;
    int ret;

    test->start_time = ktime_get();
    ret = test->test_func(test->test_data);
    test->end_time = ktime_get();

    if (ret == TEST_PASS) {
        test->result = TEST_PASS;
        atomic_inc(&stats.passed_tests);
    } else if (ret == TEST_SKIP) {
        test->result = TEST_SKIP;
        atomic_inc(&stats.skipped_tests);
    } else {
        test->result = TEST_FAIL;
        atomic_inc(&stats.failed_tests);
    }

    complete(&test->done);
    kfree(test_work);
}

/* Run a specific test case */
int run_test_case(const char *name)
{
    struct test_case *test;
    struct test_work *work;
    bool found = false;

    mutex_lock(&test_lock);
    list_for_each_entry(test, &test_cases, list) {
        if (strcmp(test->name, name) == 0) {
            found = true;
            break;
        }
    }
    mutex_unlock(&test_lock);

    if (!found)
        return -ENOENT;

    work = kzalloc(sizeof(*work), GFP_KERNEL);
    if (!work)
        return -ENOMEM;

    INIT_WORK(&work->work, test_worker);
    work->test = test;

    queue_work(test_wq, &work->work);
    wait_for_completion(&test->done);

    return test->result;
}

/* Run all test cases */
void run_all_tests(void)
{
    struct test_case *test;
    struct test_work *work;

    stats.start_time = ktime_get();

    mutex_lock(&test_lock);
    list_for_each_entry(test, &test_cases, list) {
        work = kzalloc(sizeof(*work), GFP_KERNEL);
        if (!work)
            continue;

        INIT_WORK(&work->work, test_worker);
        work->test = test;
        queue_work(test_wq, &work->work);
    }
    mutex_unlock(&test_lock);

    flush_workqueue(test_wq);
    stats.end_time = ktime_get();
}

/* Get test results */
void get_test_results(struct test_results *results)
{
    if (!results)
        return;

    results->total = atomic_read(&stats.total_tests);
    results->passed = atomic_read(&stats.passed_tests);
    results->failed = atomic_read(&stats.failed_tests);
    results->skipped = atomic_read(&stats.skipped_tests);
    results->duration_ns = ktime_to_ns(ktime_sub(stats.end_time,
                                                stats.start_time));
}

/* Set test error message */
void set_test_error(const char *name, const char *fmt, ...)
{
    struct test_case *test;
    va_list args;
    char *msg;
    int len;

    mutex_lock(&test_lock);
    list_for_each_entry(test, &test_cases, list) {
        if (strcmp(test->name, name) == 0) {
            va_start(args, fmt);
            len = vsnprintf(NULL, 0, fmt, args);
            va_end(args);

            msg = kzalloc(len + 1, GFP_KERNEL);
            if (!msg)
                break;

            va_start(args, fmt);
            vsnprintf(msg, len + 1, fmt, args);
            va_end(args);

            kfree(test->error_msg);
            test->error_msg = msg;
            break;
        }
    }
    mutex_unlock(&test_lock);
}

/* Reset test framework */
void reset_test_framework(void)
{
    struct test_case *test;

    mutex_lock(&test_lock);
    list_for_each_entry(test, &test_cases, list) {
        test->result = TEST_NOT_RUN;
        kfree(test->error_msg);
        test->error_msg = NULL;
        test->start_time = 0;
        test->end_time = 0;
        init_completion(&test->done);
    }
    mutex_unlock(&test_lock);

    atomic_set(&stats.total_tests, 0);
    atomic_set(&stats.passed_tests, 0);
    atomic_set(&stats.failed_tests, 0);
    atomic_set(&stats.skipped_tests, 0);
    stats.start_time = 0;
    stats.end_time = 0;
}

EXPORT_SYMBOL_GPL(test_framework_init);
EXPORT_SYMBOL_GPL(test_framework_exit);
EXPORT_SYMBOL_GPL(register_test_case);
EXPORT_SYMBOL_GPL(run_test_case);
EXPORT_SYMBOL_GPL(run_all_tests);
EXPORT_SYMBOL_GPL(get_test_results);
EXPORT_SYMBOL_GPL(set_test_error);
EXPORT_SYMBOL_GPL(reset_test_framework); 