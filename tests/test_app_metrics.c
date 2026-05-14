#include "test_config.h"
#include "../app/src/app_metrics.h"

static int test_clamp_percent() {
    TEST_ASSERT_DOUBLE_EQUAL(0.0, app_clamp_percent(-12.0), 0.001);
    TEST_ASSERT_DOUBLE_EQUAL(42.0, app_clamp_percent(42.0), 0.001);
    TEST_ASSERT_DOUBLE_EQUAL(100.0, app_clamp_percent(140.0), 0.001);
    return 1;
}

static int test_cpu_usage_calculation() {
    CPUStats prev = {0};
    CPUStats curr = {0};

    prev.total = 1000.0;
    prev.idle = 600.0;
    curr.total = 1200.0;
    curr.idle = 700.0;

    app_calculate_cpu_usage(&prev, &curr);
    TEST_ASSERT_DOUBLE_EQUAL(50.0, curr.usage_percent, 0.001);

    return 1;
}

static int test_cpu_usage_handles_bad_sample() {
    CPUStats prev = {0};
    CPUStats curr = {0};

    prev.total = 1000.0;
    curr.total = 1000.0;

    app_calculate_cpu_usage(&prev, &curr);
    TEST_ASSERT_DOUBLE_EQUAL(0.0, curr.usage_percent, 0.001);

    return 1;
}

static int test_format_bytes() {
    char buffer[32];

    app_format_bytes(buffer, sizeof(buffer), 512ULL * 1024ULL);
    TEST_ASSERT_STR_EQUAL("512 KB", buffer);

    app_format_bytes(buffer, sizeof(buffer), 3ULL * 1024ULL * 1024ULL);
    TEST_ASSERT_STR_EQUAL("3.0 MB", buffer);

    app_format_bytes(buffer, sizeof(buffer), 2ULL * 1024ULL * 1024ULL * 1024ULL);
    TEST_ASSERT_STR_EQUAL("2.0 GB", buffer);

    return 1;
}

static int test_visible_process_rows() {
    TEST_ASSERT_EQUAL(0, app_visible_process_rows(-5, 10));
    TEST_ASSERT_EQUAL(4, app_visible_process_rows(4, 10));
    TEST_ASSERT_EQUAL(10, app_visible_process_rows(42, 10));
    TEST_ASSERT_EQUAL(42, app_visible_process_rows(42, 0));
    return 1;
}

static int test_high_alert_requires_samples_and_recovery() {
    int active = 0;
    int samples = 0;

    TEST_ASSERT_EQUAL(0, app_alert_update_high(91.0, 90.0, 75.0, 2, &active, &samples));
    TEST_ASSERT_EQUAL(1, samples);
    TEST_ASSERT_EQUAL(0, active);

    TEST_ASSERT_EQUAL(1, app_alert_update_high(94.0, 90.0, 75.0, 2, &active, &samples));
    TEST_ASSERT_EQUAL(1, active);

    TEST_ASSERT_EQUAL(0, app_alert_update_high(95.0, 90.0, 75.0, 2, &active, &samples));
    TEST_ASSERT_EQUAL(1, active);

    TEST_ASSERT_EQUAL(0, app_alert_update_high(70.0, 90.0, 75.0, 2, &active, &samples));
    TEST_ASSERT_EQUAL(0, active);
    TEST_ASSERT_EQUAL(0, samples);

    return 1;
}

static int test_low_alert_requires_recovery() {
    int active = 0;
    int samples = 0;

    TEST_ASSERT_EQUAL(1, app_alert_update_low(14.0, 15.0, 25.0, 1, &active, &samples));
    TEST_ASSERT_EQUAL(1, active);

    TEST_ASSERT_EQUAL(0, app_alert_update_low(12.0, 15.0, 25.0, 1, &active, &samples));
    TEST_ASSERT_EQUAL(1, active);

    TEST_ASSERT_EQUAL(0, app_alert_update_low(30.0, 15.0, 25.0, 1, &active, &samples));
    TEST_ASSERT_EQUAL(0, active);
    TEST_ASSERT_EQUAL(0, samples);

    return 1;
}

static int test_alert_resets_pending_samples() {
    int active = 0;
    int samples = 0;

    TEST_ASSERT_EQUAL(0, app_alert_update_high(95.0, 90.0, 75.0, 2, &active, &samples));
    TEST_ASSERT_EQUAL(1, samples);

    TEST_ASSERT_EQUAL(0, app_alert_update_high(50.0, 90.0, 75.0, 2, &active, &samples));
    TEST_ASSERT_EQUAL(0, samples);
    TEST_ASSERT_EQUAL(0, active);

    TEST_ASSERT_EQUAL(0, app_alert_update_low(10.0, 15.0, 25.0, 2, &active, &samples));
    TEST_ASSERT_EQUAL(1, samples);

    TEST_ASSERT_EQUAL(0, app_alert_update_low(30.0, 15.0, 25.0, 2, &active, &samples));
    TEST_ASSERT_EQUAL(0, samples);
    TEST_ASSERT_EQUAL(0, active);

    return 1;
}

static int test_alert_handles_invalid_arguments() {
    int active = 0;
    int samples = 0;

    TEST_ASSERT_EQUAL(0, app_alert_update_high(100.0, 90.0, 75.0, 1, NULL, &samples));
    TEST_ASSERT_EQUAL(0, app_alert_update_low(0.0, 15.0, 25.0, 1, &active, NULL));

    TEST_ASSERT_EQUAL(1, app_alert_update_high(100.0, 90.0, 75.0, 0, &active, &samples));
    TEST_ASSERT_EQUAL(1, active);

    return 1;
}

static int test_battery_status_can_alert() {
    BatteryInfo battery = {0};

    TEST_ASSERT_EQUAL(0, app_battery_status_can_alert(NULL));
    TEST_ASSERT_EQUAL(0, app_battery_status_can_alert(&battery));

    battery.present = 1;
    snprintf(battery.status, sizeof(battery.status), "Discharging");
    TEST_ASSERT_EQUAL(1, app_battery_status_can_alert(&battery));

    snprintf(battery.status, sizeof(battery.status), "Charging");
    TEST_ASSERT_EQUAL(0, app_battery_status_can_alert(&battery));

    snprintf(battery.status, sizeof(battery.status), "Full");
    TEST_ASSERT_EQUAL(0, app_battery_status_can_alert(&battery));

    return 1;
}

void test_app_metrics_suite() {
    RUN_TEST(test_clamp_percent);
    RUN_TEST(test_cpu_usage_calculation);
    RUN_TEST(test_cpu_usage_handles_bad_sample);
    RUN_TEST(test_format_bytes);
    RUN_TEST(test_visible_process_rows);
    RUN_TEST(test_high_alert_requires_samples_and_recovery);
    RUN_TEST(test_low_alert_requires_recovery);
    RUN_TEST(test_alert_resets_pending_samples);
    RUN_TEST(test_alert_handles_invalid_arguments);
    RUN_TEST(test_battery_status_can_alert);
}
