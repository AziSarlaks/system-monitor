#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

#include <setjmp.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Цвета для вывода
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_RESET   "\x1b[0m"

// Глобальные переменные для тестов
extern int tests_run;
extern int tests_passed;
extern int tests_failed;

// Макросы для тестов
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf(COLOR_RED "❌ FAILED: %s:%d: " #condition COLOR_RESET "\n", \
                   __FILE__, __LINE__); \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf(COLOR_RED "❌ FAILED: %s:%d: expected %lld but got %lld" COLOR_RESET "\n", \
                   __FILE__, __LINE__, (long long)(expected), (long long)(actual)); \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_STR_EQUAL(expected, actual) \
    do { \
        if (strcmp(expected, actual) != 0) { \
            printf(COLOR_RED "❌ FAILED: %s:%d: expected \"%s\" but got \"%s\"" COLOR_RESET "\n", \
                   __FILE__, __LINE__, expected, actual); \
            return 0; \
        } \
    } while(0)

#define TEST_ASSERT_DOUBLE_EQUAL(expected, actual, epsilon) \
    do { \
        if (fabs((expected) - (actual)) > (epsilon)) { \
            printf(COLOR_RED "❌ FAILED: %s:%d: expected %.2f but got %.2f" COLOR_RESET "\n", \
                   __FILE__, __LINE__, (double)(expected), (double)(actual)); \
            return 0; \
        } \
    } while(0)

#define RUN_TEST(test) \
    do { \
        printf(COLOR_BLUE "  Running %s..." COLOR_RESET, #test); \
        tests_run++; \
        if (test()) { \
            printf(COLOR_GREEN " ✅\n" COLOR_RESET); \
            tests_passed++; \
        } else { \
            printf(COLOR_RED " ❌\n" COLOR_RESET); \
            tests_failed++; \
        } \
    } while(0)

#define RUN_SUITE(suite) \
    do { \
        printf("\n" COLOR_YELLOW "📋 Running test suite: %s\n" COLOR_RESET, #suite); \
        suite(); \
    } while(0)

#endif