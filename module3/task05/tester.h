
#ifndef TESTER_H
#define TESTER_H

#include <stdio.h>

extern int tests_run;
extern int tests_failed;

#define ASSERT_TRUE(msg, cond) do { \
    tests_run++; \
    if (!(cond)) { \
        printf("\n[FAIL] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define RUN_TEST(test_func) do { \
    printf("."); \
    fflush(stdout); \
    test_func(); \
} while(0)

#endif /* TESTER_H */
