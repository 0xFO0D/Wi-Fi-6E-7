#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/pm_wakeup.h>
#include "../power/power_mgmt.h"
#include "test_framework.h"

#define POWER_TEST_ITERATIONS TEST_ITER_NORMAL
#define POWER_TEST_SLEEP_TIME 100  /* 100ms sleep time */
#define POWER_TEST_WAKE_TIME 50    /* 50ms wake time */

struct power_test_context {
    struct wifi67_power_dev *power;
    struct completion sleep_done;
    struct completion wake_done;
    atomic_t sleep_count;
    atomic_t wake_count;
    bool ps_enabled;
    u32 current_state;
    u32 supported_states;
};

/* Test callback functions */
static void power_test_sleep_callback(void *data)
{
    struct power_test_context *ctx = data;
    atomic_inc(&ctx->sleep_count);
    complete(&ctx->sleep_done);
}

static void power_test_wake_callback(void *data)
{
    struct power_test_context *ctx = data;
    atomic_inc(&ctx->wake_count);
    complete(&ctx->wake_done);
}

/* Test setup and cleanup */
static struct power_test_context *power_test_init(void)
{
    struct power_test_context *ctx;
    struct wifi67_power_ops ops = {
        .sleep_complete = power_test_sleep_callback,
        .wake_complete = power_test_wake_callback,
    };

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize completion structures */
    init_completion(&ctx->sleep_done);
    init_completion(&ctx->wake_done);

    /* Initialize power management device */
    ctx->power = wifi67_power_alloc(&ops, ctx);
    if (!ctx->power) {
        kfree(ctx);
        return NULL;
    }

    /* Query supported power states */
    ctx->supported_states = wifi67_power_get_supported_states(ctx->power);
    ctx->current_state = WIFI67_POWER_STATE_D0;
    ctx->ps_enabled = false;

    return ctx;
}

static void power_test_cleanup(struct power_test_context *ctx)
{
    if (!ctx)
        return;

    wifi67_power_free(ctx->power);
    kfree(ctx);
}

/* Test cases */
static int test_power_state_transitions(void *data)
{
    struct power_test_context *ctx = data;
    int ret;
    u32 state;

    /* Test transitions through all supported power states */
    for (state = WIFI67_POWER_STATE_D0; state <= WIFI67_POWER_STATE_D3; state++) {
        if (!(ctx->supported_states & BIT(state)))
            continue;

        /* Transition to new state */
        ret = wifi67_power_set_state(ctx->power, state);
        TEST_ASSERT(ret == 0, "Failed to set power state %u", state);

        /* Wait for sleep completion if going to lower power state */
        if (state > ctx->current_state) {
            if (!wait_for_completion_timeout(&ctx->sleep_done,
                                           msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
                TEST_ASSERT(0, "Sleep timeout for state %u", state);
            }
        }

        /* Wait for wake completion if going to higher power state */
        if (state < ctx->current_state) {
            if (!wait_for_completion_timeout(&ctx->wake_done,
                                           msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
                TEST_ASSERT(0, "Wake timeout for state %u", state);
            }
        }

        /* Verify state transition */
        ret = wifi67_power_get_state(ctx->power, &ctx->current_state);
        TEST_ASSERT(ret == 0, "Failed to get power state");
        TEST_ASSERT(ctx->current_state == state,
                   "Power state mismatch: expected %u, got %u",
                   state, ctx->current_state);

        /* Allow hardware to settle */
        msleep(POWER_TEST_SLEEP_TIME);
    }

    /* Return to D0 state */
    ret = wifi67_power_set_state(ctx->power, WIFI67_POWER_STATE_D0);
    TEST_ASSERT(ret == 0, "Failed to return to D0 state");

    if (!wait_for_completion_timeout(&ctx->wake_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
        TEST_ASSERT(0, "Wake timeout returning to D0");
    }

    TEST_PASS();
}

static int test_power_save_mode(void *data)
{
    struct power_test_context *ctx = data;
    int ret, i;

    /* Enable power save mode */
    ret = wifi67_power_save_enable(ctx->power);
    TEST_ASSERT(ret == 0, "Failed to enable power save mode");
    ctx->ps_enabled = true;

    /* Test power save cycles */
    for (i = 0; i < POWER_TEST_ITERATIONS; i++) {
        /* Simulate idle period */
        msleep(POWER_TEST_SLEEP_TIME);

        /* Verify device entered power save */
        ret = wifi67_power_get_state(ctx->power, &ctx->current_state);
        TEST_ASSERT(ret == 0, "Failed to get power state");
        TEST_ASSERT(ctx->current_state > WIFI67_POWER_STATE_D0,
                   "Device did not enter power save");

        /* Generate wake event */
        ret = wifi67_power_wake_event(ctx->power);
        TEST_ASSERT(ret == 0, "Failed to generate wake event");

        /* Wait for wake completion */
        if (!wait_for_completion_timeout(&ctx->wake_done,
                                       msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
            TEST_ASSERT(0, "Wake timeout in iteration %d", i);
        }

        /* Verify device woke up */
        ret = wifi67_power_get_state(ctx->power, &ctx->current_state);
        TEST_ASSERT(ret == 0, "Failed to get power state");
        TEST_ASSERT(ctx->current_state == WIFI67_POWER_STATE_D0,
                   "Device did not wake up");

        /* Simulate active period */
        msleep(POWER_TEST_WAKE_TIME);
    }

    /* Disable power save mode */
    ret = wifi67_power_save_disable(ctx->power);
    TEST_ASSERT(ret == 0, "Failed to disable power save mode");
    ctx->ps_enabled = false;

    TEST_PASS();
}

static int test_power_wake_sources(void *data)
{
    struct power_test_context *ctx = data;
    int ret, i;
    u32 sources;

    /* Get supported wake sources */
    sources = wifi67_power_get_wake_sources(ctx->power);
    TEST_ASSERT(sources != 0, "No wake sources supported");

    /* Enable each supported wake source */
    for (i = 0; i < 32; i++) {
        if (!(sources & BIT(i)))
            continue;

        ret = wifi67_power_enable_wake_source(ctx->power, BIT(i));
        TEST_ASSERT(ret == 0, "Failed to enable wake source %d", i);

        /* Verify wake source was enabled */
        ret = wifi67_power_get_enabled_wake_sources(ctx->power, &sources);
        TEST_ASSERT(ret == 0, "Failed to get enabled wake sources");
        TEST_ASSERT(sources & BIT(i),
                   "Wake source %d not enabled", i);

        /* Test wake source */
        ret = wifi67_power_trigger_wake_source(ctx->power, BIT(i));
        TEST_ASSERT(ret == 0, "Failed to trigger wake source %d", i);

        /* Wait for wake completion */
        if (!wait_for_completion_timeout(&ctx->wake_done,
                                       msecs_to_jiffies(TEST_TIMEOUT_MEDIUM))) {
            TEST_ASSERT(0, "Wake timeout for source %d", i);
        }

        /* Disable wake source */
        ret = wifi67_power_disable_wake_source(ctx->power, BIT(i));
        TEST_ASSERT(ret == 0, "Failed to disable wake source %d", i);
    }

    TEST_PASS();
}

static int test_power_stress(void *data)
{
    struct power_test_context *ctx = data;
    int i, ret;
    u32 state;

    for (i = 0; i < TEST_ITER_STRESS; i++) {
        /* Randomly select power state */
        do {
            state = prandom_u32() % (WIFI67_POWER_STATE_D3 + 1);
        } while (!(ctx->supported_states & BIT(state)));

        /* Transition to new state */
        ret = wifi67_power_set_state(ctx->power, state);
        TEST_ASSERT(ret == 0,
                   "Failed to set power state %u in iteration %d",
                   state, i);

        /* Wait for sleep/wake completion */
        if (state > ctx->current_state) {
            if (!wait_for_completion_timeout(&ctx->sleep_done,
                                           msecs_to_jiffies(TEST_TIMEOUT_SHORT))) {
                TEST_ASSERT(0,
                          "Sleep timeout for state %u in iteration %d",
                          state, i);
            }
        } else if (state < ctx->current_state) {
            if (!wait_for_completion_timeout(&ctx->wake_done,
                                           msecs_to_jiffies(TEST_TIMEOUT_SHORT))) {
                TEST_ASSERT(0,
                          "Wake timeout for state %u in iteration %d",
                          state, i);
            }
        }

        /* Verify state transition */
        ret = wifi67_power_get_state(ctx->power, &ctx->current_state);
        TEST_ASSERT(ret == 0, "Failed to get power state");
        TEST_ASSERT(ctx->current_state == state,
                   "Power state mismatch in iteration %d",
                   i);

        /* Allow hardware to settle */
        usleep_range(1000, 2000);

        /* Reset completions for next iteration */
        reinit_completion(&ctx->sleep_done);
        reinit_completion(&ctx->wake_done);
    }

    /* Return to D0 state */
    ret = wifi67_power_set_state(ctx->power, WIFI67_POWER_STATE_D0);
    TEST_ASSERT(ret == 0, "Failed to return to D0 state");

    TEST_PASS();
}

/* Module initialization */
static int __init power_test_module_init(void)
{
    struct power_test_context *ctx;

    ctx = power_test_init();
    if (!ctx)
        return -ENOMEM;

    /* Register test cases */
    REGISTER_TEST("power_states", "Test power state transitions",
                 test_power_state_transitions, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("power_save", "Test power save mode",
                 test_power_save_mode, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("power_wake", "Test wake sources",
                 test_power_wake_sources, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("power_stress", "Stress test power management",
                 test_power_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

static void __exit power_test_module_exit(void)
{
    struct test_results results;
    get_test_results(&results);
    pr_info("Power management tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);
    power_test_cleanup(NULL); /* ctx is managed by test framework */
}

module_init(power_test_module_init);
module_exit(power_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 Power Management Test Module");
MODULE_VERSION("1.0"); 