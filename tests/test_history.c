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
        add_to_history(&history, i * 10.0, i * 5.0, i * 15.0, i * 8.0, i * 3.0);
    }
    
    TEST_ASSERT(history.count == 10);
    
    return 1;
}

static int test_history_wrap() {
    HistoryData history;
    init_history(&history);
    
    for (int i = 0; i < HISTORY_SIZE + 10; i++) {
        add_to_history(&history, i, i, i, i, i);
    }
    
    TEST_ASSERT(history.count == HISTORY_SIZE);
    TEST_ASSERT(history.index < HISTORY_SIZE);
    
    return 1;
}

static int test_history_json() {
    HistoryData history;
    init_history(&history);
    char buffer[4096];
    
    for (int i = 0; i < 5; i++) {
        add_to_history(&history, i * 10.0, i * 5.0, i * 15.0, i * 8.0, i * 3.0);
    }
    
    get_history_json(buffer, sizeof(buffer), &history);
    
    TEST_ASSERT(strstr(buffer, "cpu") != NULL);
    TEST_ASSERT(strstr(buffer, "memory") != NULL);
    TEST_ASSERT(strstr(buffer, "gpu") != NULL);
    
    return 1;
}

// Сьют тестов
void test_history_suite() {
    RUN_TEST(test_history_init);
    RUN_TEST(test_history_add);
    RUN_TEST(test_history_wrap);
    RUN_TEST(test_history_json);
}