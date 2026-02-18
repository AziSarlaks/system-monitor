#include "test_config.h"

// Объявления внешних функций (они определены в других файлах)
extern void test_proc_parser_suite(void);
extern void test_json_formatter_suite(void);
extern void test_history_suite(void);
extern void test_server_mock_suite(void);

// Глобальные переменные
int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;

int main() {
    printf(COLOR_BLUE "\n╔══════════════════════════════════════════╗\n" COLOR_RESET);
    printf(COLOR_BLUE "║    System Monitor Server Tests         ║\n" COLOR_RESET);
    printf(COLOR_BLUE "╚══════════════════════════════════════════╝\n" COLOR_RESET);
    
    // Запуск тестов
    RUN_SUITE(test_proc_parser_suite);
    RUN_SUITE(test_json_formatter_suite);
    RUN_SUITE(test_history_suite);
    RUN_SUITE(test_server_mock_suite);
    
    // Итоги
    printf("\n" COLOR_BLUE "══════════════════════════════════════════\n" COLOR_RESET);
    printf("📊 Total tests: %d\n", tests_run);
    printf(COLOR_GREEN "✅ Passed: %d\n" COLOR_RESET, tests_passed);
    printf(COLOR_RED "❌ Failed: %d\n" COLOR_RESET, tests_failed);
    
    return tests_failed;
}