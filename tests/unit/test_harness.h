#ifndef NETREK_TEST_HARNESS_H
#define NETREK_TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run = 0;
static int g_assertions = 0;
static int g_failures = 0;

#define ASSERT_TRUE(cond) do { \
    g_assertions++; \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: expected true: %s\n", __FILE__, __LINE__, #cond); \
        g_failures++; \
        return; \
    } \
} while (0)

#define ASSERT_EQ(a, b) do { \
    long long _a = (long long)(a); \
    long long _b = (long long)(b); \
    g_assertions++; \
    if (_a != _b) { \
        fprintf(stderr, "FAIL %s:%d: %s (%lld) != %s (%lld)\n", \
                __FILE__, __LINE__, #a, _a, #b, _b); \
        g_failures++; \
        return; \
    } \
} while (0)

#define ASSERT_STREQ(a, b) do { \
    const char *_a = (a); \
    const char *_b = (b); \
    g_assertions++; \
    if (!_a || !_b || strcmp(_a, _b) != 0) { \
        fprintf(stderr, "FAIL %s:%d: %s (\"%s\") != %s (\"%s\")\n", \
                __FILE__, __LINE__, #a, _a ? _a : "(null)", #b, _b ? _b : "(null)"); \
        g_failures++; \
        return; \
    } \
} while (0)

#define RUN_TEST(fn) do { \
    int _before = g_failures; \
    g_tests_run++; \
    fn(); \
    if (g_failures == _before) { \
        printf("  ok   %s\n", #fn); \
    } else { \
        printf("  FAIL %s\n", #fn); \
    } \
} while (0)

#define TEST_SUMMARY(suite) do { \
    if (g_failures > 0) { \
        fprintf(stderr, "FAIL: %s (%d/%d failed, %d assertions)\n", \
                (suite), g_failures, g_tests_run, g_assertions); \
        return 1; \
    } \
    printf("PASS: %s (%d tests, %d assertions)\n", \
           (suite), g_tests_run, g_assertions); \
    return 0; \
} while (0)

#endif /* NETREK_TEST_HARNESS_H */
