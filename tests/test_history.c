#include "test_config.h"
#include "../backend/src/history.h"
#include "../backend/src/config.h"

static int test_history_init() {
    HistoryData history;
    init_history(&history);
    
    TEST_ASSERT(history.index == 0);
    TEST_ASSERT(history.count == 0);
    
    return 1;
}

static int test_history_add() {
    HistoryData history;
    init_history(&history);
    
    for (int i = 0; i < 10; i++) {
        add_to_history(&history, i * 10.0, i * 5.0, i * 15.0, i * 8.0, i * 3.0,
                       i, i, i, i, i);
    }
    
    TEST_ASSERT(history.count == 10);
    
    return 1;
}

static int test_history_wrap() {
    HistoryData history;
    init_history(&history);
    
    for (int i = 0; i < HISTORY_SIZE + 10; i++) {
        add_to_history(&history, i, i, i, i, i, i, i, i, i, i);
    }
    
    TEST_ASSERT(history.count == HISTORY_SIZE);
    TEST_ASSERT(history.index < HISTORY_SIZE);
    
    return 1;
}

static int test_history_keeps_latest_values_after_wrap() {
    HistoryData history;
    init_history(&history);

    for (int i = 0; i < HISTORY_SIZE + 3; i++) {
        add_to_history(&history,
                       i,
                       i + 1,
                       i + 2,
                       i + 3,
                       i + 4,
                       i + 5,
                       i + 6,
                       i + 7,
                       i + 8,
                       i + 9);
    }

    int oldest = history.index;
    int newest = (history.index - 1 + HISTORY_SIZE) % HISTORY_SIZE;

    TEST_ASSERT_DOUBLE_EQUAL(3.0, history.cpu_usage[oldest], 0.01);
    TEST_ASSERT_DOUBLE_EQUAL((double)(HISTORY_SIZE + 2), history.cpu_usage[newest], 0.01);
    TEST_ASSERT_DOUBLE_EQUAL((double)(HISTORY_SIZE + 6), history.gpu_temperature[newest], 0.01);
    TEST_ASSERT_DOUBLE_EQUAL((double)(HISTORY_SIZE + 10), history.network_tx[newest], 0.01);

    return 1;
}

// Сьют тестов
void test_history_suite() {
    RUN_TEST(test_history_init);
    RUN_TEST(test_history_add);
    RUN_TEST(test_history_wrap);
    RUN_TEST(test_history_keeps_latest_values_after_wrap);
}
