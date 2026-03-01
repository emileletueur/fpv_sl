#pragma once
/* Framework de test minimaliste pour cible RP2040.
   Résultats reportés via USB CDC (LOGI/LOGE depuis debug_log.h).

   Usage dans un fichier de test :
     void test_foo(void) {
         TEST_EXPECT_EQ(42, my_function());
     }

   Dans test_runner_main.c :
     RUN_TEST(test_foo); */

#include "debug_log.h"

/* Variables globales définies dans test_runner_main.c */
extern int _test_failed;
extern int _pass_count;
extern int _fail_count;

/* ── Assertions ─────────────────────────────────────────────────── */

#define TEST_EXPECT(cond) \
    do { \
        if (!(cond)) { \
            LOGE("    EXPECT failed: " #cond "  [%s:%d]", __FILE__, __LINE__); \
            _test_failed = 1; \
        } \
    } while (0)

#define TEST_EXPECT_TRUE(v)      TEST_EXPECT((v) != 0)
#define TEST_EXPECT_FALSE(v)     TEST_EXPECT((v) == 0)
#define TEST_EXPECT_EQ(a, b)     TEST_EXPECT((a) == (b))
#define TEST_EXPECT_NEQ(a, b)    TEST_EXPECT((a) != (b))
#define TEST_EXPECT_NULL(v)      TEST_EXPECT((v) == NULL)
#define TEST_EXPECT_NOT_NULL(v)  TEST_EXPECT((v) != NULL)

/* ── Runner ─────────────────────────────────────────────────────── */

#define RUN_TEST(fn) \
    do { \
        _test_failed = 0; \
        (fn)(); \
        if (_test_failed) { LOGE("[FAIL] " #fn); _fail_count++; } \
        else              { LOGI("[PASS] " #fn); _pass_count++; } \
    } while (0)
