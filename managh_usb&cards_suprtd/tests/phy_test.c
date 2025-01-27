#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/random.h>
#include "../phy/phy_core.h"
#include "test_framework.h"

#define PHY_TEST_ITERATIONS TEST_ITER_NORMAL
#define PHY_TEST_CHANNELS 8
#define PHY_TEST_POWER_LEVELS 8
#define PHY_TEST_MCS_MAX 11  /* Up to MCS 11 for WiFi 6/6E */

struct phy_test_context {
    struct wifi67_phy_dev *phy;
    struct completion cal_done;
    atomic_t cal_count;
    u32 supported_bands;
    u32 supported_channels[IEEE80211_NUM_BANDS];
    u8 current_band;
    u8 current_channel;
    u8 current_power;
    u8 current_mcs;
    bool calibrated;
};

/* Test callback functions */
static void phy_test_cal_callback(void *data, int status)
{
    struct phy_test_context *ctx = data;
    atomic_inc(&ctx->cal_count);
    complete(&ctx->cal_done);
}

/* Test setup and cleanup */
static struct phy_test_context *phy_test_init(void)
{
    struct phy_test_context *ctx;
    struct wifi67_phy_ops ops = {
        .calibration_done = phy_test_cal_callback,
    };

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
    if (!ctx)
        return NULL;

    /* Initialize completion structure */
    init_completion(&ctx->cal_done);

    /* Initialize PHY device */
    ctx->phy = wifi67_phy_alloc(&ops, ctx);
    if (!ctx->phy) {
        kfree(ctx);
        return NULL;
    }

    /* Query supported bands and channels */
    ctx->supported_bands = wifi67_phy_get_supported_bands(ctx->phy);
    if (ctx->supported_bands & BIT(IEEE80211_BAND_2GHZ))
        ctx->supported_channels[IEEE80211_BAND_2GHZ] =
            wifi67_phy_get_channels(ctx->phy, IEEE80211_BAND_2GHZ);
    if (ctx->supported_bands & BIT(IEEE80211_BAND_5GHZ))
        ctx->supported_channels[IEEE80211_BAND_5GHZ] =
            wifi67_phy_get_channels(ctx->phy, IEEE80211_BAND_5GHZ);
    if (ctx->supported_bands & BIT(IEEE80211_BAND_6GHZ))
        ctx->supported_channels[IEEE80211_BAND_6GHZ] =
            wifi67_phy_get_channels(ctx->phy, IEEE80211_BAND_6GHZ);

    return ctx;
}

static void phy_test_cleanup(struct phy_test_context *ctx)
{
    if (!ctx)
        return;

    wifi67_phy_free(ctx->phy);
    kfree(ctx);
}

/* Test cases */
static int test_phy_calibration(void *data)
{
    struct phy_test_context *ctx = data;
    int ret;

    /* Start calibration */
    ret = wifi67_phy_calibrate(ctx->phy);
    TEST_ASSERT(ret == 0, "Failed to start calibration");

    /* Wait for completion */
    if (!wait_for_completion_timeout(&ctx->cal_done,
                                   msecs_to_jiffies(TEST_TIMEOUT_LONG))) {
        TEST_ASSERT(0, "Calibration timeout");
    }

    TEST_ASSERT(atomic_read(&ctx->cal_count) == 1,
                "Incorrect calibration completion count");

    ctx->calibrated = true;
    TEST_PASS();
}

static int test_phy_channel_switch(void *data)
{
    struct phy_test_context *ctx = data;
    int i, ret;
    u8 band, channel;

    TEST_ASSERT(ctx->calibrated, "PHY not calibrated");

    /* Test channel switching for each supported band */
    for (band = 0; band < IEEE80211_NUM_BANDS; band++) {
        if (!(ctx->supported_bands & BIT(band)))
            continue;

        /* Switch to each channel in the band */
        for (i = 0; i < PHY_TEST_CHANNELS; i++) {
            channel = i % ctx->supported_channels[band];

            ret = wifi67_phy_set_channel(ctx->phy, band, channel);
            TEST_ASSERT(ret == 0,
                       "Failed to switch to channel %u in band %u",
                       channel, band);

            /* Verify channel setting */
            ret = wifi67_phy_get_channel(ctx->phy, &ctx->current_band,
                                       &ctx->current_channel);
            TEST_ASSERT(ret == 0, "Failed to get current channel");
            TEST_ASSERT(ctx->current_band == band &&
                       ctx->current_channel == channel,
                       "Channel verification failed");

            /* Allow hardware to settle */
            usleep_range(1000, 2000);
        }
    }

    TEST_PASS();
}

static int test_phy_power_control(void *data)
{
    struct phy_test_context *ctx = data;
    int i, ret;
    s8 power, readback;

    TEST_ASSERT(ctx->calibrated, "PHY not calibrated");

    /* Test power control across different levels */
    for (i = 0; i < PHY_TEST_POWER_LEVELS; i++) {
        power = i * (20 / PHY_TEST_POWER_LEVELS); /* 0 to 20 dBm range */

        ret = wifi67_phy_set_power(ctx->phy, power);
        TEST_ASSERT(ret == 0, "Failed to set power level %d dBm", power);

        /* Verify power setting */
        ret = wifi67_phy_get_power(ctx->phy, &readback);
        TEST_ASSERT(ret == 0, "Failed to get current power level");
        TEST_ASSERT(readback == power, "Power level verification failed");

        /* Allow hardware to settle */
        usleep_range(1000, 2000);
    }

    TEST_PASS();
}

static int test_phy_rate_control(void *data)
{
    struct phy_test_context *ctx = data;
    int i, ret;
    u8 mcs, bw, nss;
    u32 rate_flags;

    TEST_ASSERT(ctx->calibrated, "PHY not calibrated");

    /* Test different MCS rates */
    for (mcs = 0; mcs <= PHY_TEST_MCS_MAX; mcs++) {
        /* Test with different bandwidth configurations */
        for (bw = 0; bw < 4; bw++) { /* 20, 40, 80, 160 MHz */
            /* Test with different spatial streams */
            for (nss = 1; nss <= 4; nss++) {
                rate_flags = WIFI67_RATE_HE |
                           (bw << WIFI67_RATE_BW_SHIFT) |
                           (nss << WIFI67_RATE_NSS_SHIFT);

                ret = wifi67_phy_set_rate(ctx->phy, mcs, rate_flags);
                if (ret == -ENOTSUPP)
                    continue; /* Skip unsupported combinations */

                TEST_ASSERT(ret == 0,
                          "Failed to set MCS %u, BW %u, NSS %u",
                          mcs, bw, nss);

                /* Allow rate change to take effect */
                usleep_range(1000, 2000);

                /* Verify rate setting */
                ret = wifi67_phy_get_rate(ctx->phy, &ctx->current_mcs,
                                        &rate_flags);
                TEST_ASSERT(ret == 0, "Failed to get current rate");
                TEST_ASSERT(ctx->current_mcs == mcs &&
                          (rate_flags & WIFI67_RATE_MCS_MASK) == mcs,
                          "Rate verification failed");
            }
        }
    }

    TEST_PASS();
}

static int test_phy_stress(void *data)
{
    struct phy_test_context *ctx = data;
    int i, ret;
    u8 band, channel, mcs;
    s8 power;
    u32 rate_flags;

    TEST_ASSERT(ctx->calibrated, "PHY not calibrated");

    for (i = 0; i < TEST_ITER_STRESS; i++) {
        /* Randomly select band and channel */
        do {
            band = prandom_u32() % IEEE80211_NUM_BANDS;
        } while (!(ctx->supported_bands & BIT(band)));
        channel = prandom_u32() % ctx->supported_channels[band];

        /* Randomly select power level */
        power = (prandom_u32() % 20); /* 0-20 dBm */

        /* Randomly select MCS rate and configuration */
        mcs = prandom_u32() % (PHY_TEST_MCS_MAX + 1);
        rate_flags = WIFI67_RATE_HE |
                    ((prandom_u32() % 4) << WIFI67_RATE_BW_SHIFT) |
                    ((1 + (prandom_u32() % 4)) << WIFI67_RATE_NSS_SHIFT);

        /* Apply random configuration */
        ret = wifi67_phy_set_channel(ctx->phy, band, channel);
        TEST_ASSERT(ret == 0, "Stress test: channel switch failed");

        ret = wifi67_phy_set_power(ctx->phy, power);
        TEST_ASSERT(ret == 0, "Stress test: power control failed");

        ret = wifi67_phy_set_rate(ctx->phy, mcs, rate_flags);
        if (ret != -ENOTSUPP) /* Skip unsupported combinations */
            TEST_ASSERT(ret == 0, "Stress test: rate control failed");

        /* Allow hardware to settle */
        usleep_range(500, 1000);
    }

    TEST_PASS();
}

/* Module initialization */
static int __init phy_test_module_init(void)
{
    struct phy_test_context *ctx;

    ctx = phy_test_init();
    if (!ctx)
        return -ENOMEM;

    /* Register test cases */
    REGISTER_TEST("phy_cal", "Test PHY calibration",
                 test_phy_calibration, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("phy_channel", "Test channel switching",
                 test_phy_channel_switch, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("phy_power", "Test power control",
                 test_phy_power_control, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("phy_rate", "Test rate control",
                 test_phy_rate_control, ctx,
                 TEST_FLAG_HARDWARE);

    REGISTER_TEST("phy_stress", "Stress test PHY",
                 test_phy_stress, ctx,
                 TEST_FLAG_HARDWARE | TEST_FLAG_STRESS | TEST_FLAG_SLOW);

    return 0;
}

static void __exit phy_test_module_exit(void)
{
    struct test_results results;
    get_test_results(&results);
    pr_info("PHY tests completed: %d passed, %d failed, %d skipped\n",
            results.passed, results.failed, results.skipped);
    phy_test_cleanup(NULL); /* ctx is managed by test framework */
}

module_init(phy_test_module_init);
module_exit(phy_test_module_exit);

MODULE_LICENSE("MIT");
MODULE_AUTHOR("Fayssal Chokri");
MODULE_DESCRIPTION("WiFi 6E/7 PHY Test Module");
MODULE_VERSION("1.0"); 