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

void test_app_metrics_suite() {
    RUN_TEST(test_clamp_percent);
    RUN_TEST(test_cpu_usage_calculation);
    RUN_TEST(test_cpu_usage_handles_bad_sample);
    RUN_TEST(test_format_bytes);
    RUN_TEST(test_visible_process_rows);
}
