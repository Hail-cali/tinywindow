#include "tinywindow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_create_destroy(void) {
    tw_timing_cms_t *cms = tw_tcms_create(5, 1000, 12, 300000);
    assert(cms != NULL);
    printf("  memory usage: %zu bytes\n", tw_tcms_memory_usage(cms));
    tw_tcms_destroy(cms);

    /* Invalid params */
    assert(tw_tcms_create(0, 1000, 12, 300000) == NULL);
    assert(tw_tcms_create(5, 0, 12, 300000) == NULL);
    assert(tw_tcms_create(5, 1000, 0, 300000) == NULL);
    assert(tw_tcms_create(5, 1000, 12, 0) == NULL);

    printf("  PASS: create/destroy\n");
}

static void test_basic_count(void) {
    /* 5 rows, 10000 cols, 12 slots of 5 minutes */
    tw_timing_cms_t *cms = tw_tcms_create(5, 10000, 12, 300000);
    assert(cms != NULL);

    uint64_t now = 1000000;  /* arbitrary start time */

    /* Record "user:abc" 5 times */
    for (int i = 0; i < 5; i++) {
        assert(tw_tcms_record(cms, "user:abc", now) == TW_OK);
    }

    /* Count within full window (12 slots * 300s = 3600s = 1 hour) */
    uint64_t count = tw_tcms_count(cms, "user:abc", 3600000, now);
    printf("  count(user:abc, 1h) = %llu (expected >= 5)\n",
           (unsigned long long)count);
    assert(count >= 5);  /* CMS overestimates, never underestimates */

    /* Count for unknown key should be 0 or very small */
    uint64_t count_unknown = tw_tcms_count(cms, "user:unknown", 3600000, now);
    printf("  count(user:unknown, 1h) = %llu (expected 0)\n",
           (unsigned long long)count_unknown);
    assert(count_unknown == 0);

    tw_tcms_destroy(cms);
    printf("  PASS: basic count\n");
}

static void test_time_window_expiry(void) {
    /* 4 rows, 5000 cols, 6 slots of 10 seconds */
    uint32_t slot_ms = 10000;
    tw_timing_cms_t *cms = tw_tcms_create(4, 5000, 6, slot_ms);
    assert(cms != NULL);

    uint64_t t = 1000000;

    /* Record at t */
    tw_tcms_record(cms, "key_a", t);
    tw_tcms_record(cms, "key_a", t);
    tw_tcms_record(cms, "key_a", t);

    uint64_t c1 = tw_tcms_count(cms, "key_a", 60000, t);
    printf("  at t: count = %llu\n", (unsigned long long)c1);
    assert(c1 >= 3);

    /* Advance by 3 slots (30 seconds): still within 60s window */
    uint64_t t2 = t + 3 * slot_ms;
    uint64_t c2 = tw_tcms_count(cms, "key_a", 60000, t2);
    printf("  at t+30s: count(60s) = %llu\n", (unsigned long long)c2);
    assert(c2 >= 3);

    /* Advance by 6 slots (60 seconds): original slot should be expired */
    uint64_t t3 = t + 6 * slot_ms;
    uint64_t c3 = tw_tcms_count(cms, "key_a", 60000, t3);
    printf("  at t+60s: count(60s) = %llu\n", (unsigned long long)c3);
    /* The original slot was cleared, so count should be 0 */
    assert(c3 == 0);

    tw_tcms_destroy(cms);
    printf("  PASS: time window expiry\n");
}

static void test_multi_key_accuracy(void) {
    /* Test CMS accuracy with many keys */
    tw_timing_cms_t *cms = tw_tcms_create(5, 50000, 1, 3600000);
    assert(cms != NULL);

    uint64_t now = 1000000;
    const int NUM_KEYS = 1000;

    /* Record key_0 100 times, key_1 50 times, rest 1 time each */
    for (int i = 0; i < 100; i++) {
        tw_tcms_record(cms, "hot_key_0", now);
    }
    for (int i = 0; i < 50; i++) {
        tw_tcms_record(cms, "hot_key_1", now);
    }
    for (int i = 2; i < NUM_KEYS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        tw_tcms_record(cms, key, now);
    }

    uint64_t c0 = tw_tcms_count(cms, "hot_key_0", 3600000, now);
    uint64_t c1 = tw_tcms_count(cms, "hot_key_1", 3600000, now);
    printf("  hot_key_0: count = %llu (true = 100)\n", (unsigned long long)c0);
    printf("  hot_key_1: count = %llu (true = 50)\n", (unsigned long long)c1);

    /* CMS guarantee: estimate >= true count */
    assert(c0 >= 100);
    assert(c1 >= 50);

    /* With width=50000 and only ~1200 total inserts, overestimation should be minimal */
    assert(c0 <= 110);  /* allow ~10% overestimation */
    assert(c1 <= 60);

    tw_tcms_destroy(cms);
    printf("  PASS: multi-key accuracy\n");
}

static void test_reset(void) {
    tw_timing_cms_t *cms = tw_tcms_create(4, 1000, 6, 10000);
    assert(cms != NULL);

    uint64_t now = 1000000;
    for (int i = 0; i < 10; i++) {
        tw_tcms_record(cms, "key", now);
    }
    assert(tw_tcms_count(cms, "key", 60000, now) >= 10);

    tw_tcms_reset(cms);
    assert(tw_tcms_count(cms, "key", 60000, now) == 0);

    tw_tcms_destroy(cms);
    printf("  PASS: reset\n");
}

int main(void) {
    printf("=== test_timing_cms ===\n");

    printf("[test_create_destroy]\n");
    test_create_destroy();

    printf("[test_basic_count]\n");
    test_basic_count();

    printf("[test_time_window_expiry]\n");
    test_time_window_expiry();

    printf("[test_multi_key_accuracy]\n");
    test_multi_key_accuracy();

    printf("[test_reset]\n");
    test_reset();

    printf("\nAll tests passed.\n");
    return 0;
}
