#ifndef TESTER_H
#define TESTER_H

#include <stdio.h>

// Глобальные счетчики тестов
extern int tests_run;
extern int tests_failed;

// Макрос проверки условия (если условие ложно, прерываем тест и пишем ошибку)
#define ASSERT_TRUE(message, test) do { \
    if (!(test)) { \
        fprintf(stderr, "\n[FAIL] %s:%d: %s\n", __FILE__, __LINE__, message); \
        tests_failed++; \
        return; \
    } \
} while (0)

// Макрос запуска теста (после успешного прохождения выводит точку)
#define RUN_TEST(test) do { \
    test(); \
    tests_run++; \
    printf("."); \
    fflush(stdout); \
} while (0)

#endif // TESTER_H