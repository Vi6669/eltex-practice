#ifndef TESTER_H
#define TESTER_H

#include <stdio.h>

extern int tests_run;
extern int tests_failed;

// Макрос утверждения. Если условие ложно, выводит ошибку и прерывает текущий тест.
#define ASSERT_TRUE(message, test) do { \
    if (!(test)) { \
        fprintf(stderr, "\n\t[FAIL]: %s (Строка %d): %s\n", __func__, __LINE__, message); \
        tests_failed++; \
        return; \
    } \
} while (0)

// Макрос запуска теста. Увеличивает счетчик и запускает функцию.
#define RUN_TEST(test) do { \
    tests_run++; \
    printf("."); \
    fflush(stdout); \
    test(); \
} while (0)

#endif // TESTER_H