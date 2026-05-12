#include "nanofilter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_create_destroy(void) {
    nf_sliding_bf_t *bf = nf_sbf_create(10000, 0.01, 6, 10000);
    assert(bf != NULL);
    printf("  memory usage: %zu bytes\n", nf_sbf_memory_usage(bf));
    nf_sbf_destroy(bf);

    /* Invalid params */
    assert(nf_sbf_create(0, 0.01, 6, 10000) == NULL);
    assert(nf_sbf_create(10000, 0.0, 6, 10000) == NULL);
    assert(nf_sbf_create(10000, 1.0, 6, 10000) == NULL);
    assert(nf_sbf_create(10000, 0.01, 0, 10000) == NULL);
    assert(nf_sbf_create(10000, 0.01, 6, 0) == NULL);

    printf("  PASS: create/destroy\n");
}

static void test_insert_and_query(void) {
    nf_sliding_bf_t *bf = nf_sbf_create(10000, 0.01, 6, 10000);
    assert(bf != NULL);

    uint64_t t = 1000000;

    nf_sbf_insert(bf, "click:abc123", t);
    assert(nf_sbf_might_contain(bf, "click:abc123", 60000, t) == true);
    assert(nf_sbf_might_contain(bf, "click:never_inserted", 60000, t) == false);

    nf_sbf_destroy(bf);
    printf("  PASS: insert and query\n");
}

static void test_time_expiry(void) {
    /* 6 slots of 10 seconds = 60s total window */
    nf_sliding_bf_t *bf = nf_sbf_create(10000, 0.01, 6, 10000);
    assert(bf != NULL);

    uint64_t t = 1000000;
    nf_sbf_insert(bf, "click:abc123", t);

    /* Still within window at t + 50s */
    assert(nf_sbf_might_contain(bf, "click:abc123", 60000, t + 50000) == true);

    /* After full window expiry at t + 60s */
    assert(nf_sbf_might_contain(bf, "click:abc123", 60000, t + 60000) == false);

    nf_sbf_destroy(bf);
    printf("  PASS: time expiry\n");
}

static void test_partial_window_query(void) {
    /* 6 slots of 10s; insert at t, query with 20s window at t+40s */
    nf_sliding_bf_t *bf = nf_sbf_create(10000, 0.01, 6, 10000);
    assert(bf != NULL);

    uint64_t t = 1000000;
    nf_sbf_insert(bf, "key_a", t);

    /* At t+40s, query 20s window (covers slots at t+40s and t+30s, not t) */
    assert(nf_sbf_might_contain(bf, "key_a", 20000, t + 40000) == false);

    /* At t+40s, query full 60s window (covers all slots including t) */
    assert(nf_sbf_might_contain(bf, "key_a", 60000, t + 40000) == true);

    nf_sbf_destroy(bf);
    printf("  PASS: partial window query\n");
}

static void test_false_positive_rate(void) {
    nf_sliding_bf_t *bf = nf_sbf_create(10000, 0.01, 1, 3600000);
    assert(bf != NULL);

    uint64_t now = 1000000;

    /* Insert 5000 items */
    for (int i = 0; i < 5000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "inserted_%d", i);
        nf_sbf_insert(bf, key, now);
    }

    /* Check 10000 items that were NOT inserted */
    int false_positives = 0;
    for (int i = 0; i < 10000; i++) {
        char key[32];
        snprintf(key, sizeof(key), "not_inserted_%d", i);
        if (nf_sbf_might_contain(bf, key, 3600000, now)) {
            false_positives++;
        }
    }

    double fp_rate = (double)false_positives / 10000.0;
    printf("  FP rate: %.4f (target: 0.01)\n", fp_rate);
    assert(fp_rate < 0.03);

    nf_sbf_destroy(bf);
    printf("  PASS: false positive rate\n");
}

static void test_multiple_inserts_same_key(void) {
    nf_sliding_bf_t *bf = nf_sbf_create(10000, 0.01, 6, 10000);
    assert(bf != NULL);

    uint64_t t = 1000000;

    /* Insert same key multiple times across different slots */
    nf_sbf_insert(bf, "dup_key", t);
    nf_sbf_insert(bf, "dup_key", t + 10000);
    nf_sbf_insert(bf, "dup_key", t + 20000);

    /* Should still be found */
    assert(nf_sbf_might_contain(bf, "dup_key", 60000, t + 20000) == true);

    nf_sbf_destroy(bf);
    printf("  PASS: multiple inserts same key\n");
}

int main(void) {
    printf("=== test_sliding_bf ===\n");

    printf("[test_create_destroy]\n");
    test_create_destroy();

    printf("[test_insert_and_query]\n");
    test_insert_and_query();

    printf("[test_time_expiry]\n");
    test_time_expiry();

    printf("[test_partial_window_query]\n");
    test_partial_window_query();

    printf("[test_false_positive_rate]\n");
    test_false_positive_rate();

    printf("[test_multiple_inserts_same_key]\n");
    test_multiple_inserts_same_key();

    printf("\nAll sliding BF tests passed.\n");
    return 0;
}
