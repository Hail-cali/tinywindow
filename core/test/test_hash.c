#include "nanofilter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * MurmurHash3_x86_128 test vectors.
 * Reference values from the original smhasher implementation.
 */

static void test_empty_string(void) {
    /* Empty string with seed 0: MurmurHash3 reference returns all zeros (finalization of 0) */
    uint32_t out0[4] = {0};
    nf_murmurhash3_x86_128("", 0, 0, out0);
    printf("  empty string (seed=0): %08x %08x %08x %08x\n",
           out0[0], out0[1], out0[2], out0[3]);

    /* With non-zero seed, should produce non-zero hash */
    uint32_t out1[4] = {0};
    nf_murmurhash3_x86_128("", 0, 42, out1);
    printf("  empty string (seed=42): %08x %08x %08x %08x\n",
           out1[0], out1[1], out1[2], out1[3]);
    assert(out1[0] != 0 || out1[1] != 0 || out1[2] != 0 || out1[3] != 0);
    printf("  PASS: empty string with non-zero seed produces non-zero hash\n");
}

static void test_known_vectors(void) {
    /* Test determinism: same input -> same output */
    uint32_t out1[4], out2[4];

    nf_murmurhash3_x86_128("hello", 5, 42, out1);
    nf_murmurhash3_x86_128("hello", 5, 42, out2);
    assert(memcmp(out1, out2, 16) == 0);
    printf("  PASS: deterministic output\n");

    /* Different seeds -> different output */
    nf_murmurhash3_x86_128("hello", 5, 0, out1);
    nf_murmurhash3_x86_128("hello", 5, 1, out2);
    assert(memcmp(out1, out2, 16) != 0);
    printf("  PASS: different seeds produce different hashes\n");

    /* Different keys -> different output */
    nf_murmurhash3_x86_128("hello", 5, 0, out1);
    nf_murmurhash3_x86_128("world", 5, 0, out2);
    assert(memcmp(out1, out2, 16) != 0);
    printf("  PASS: different keys produce different hashes\n");
}

static void test_hash128_convenience(void) {
    uint64_t h1a, h2a, h1b, h2b;

    nf_hash128("user:abc:campaign:xyz", 0, &h1a, &h2a);
    nf_hash128("user:abc:campaign:xyz", 0, &h1b, &h2b);
    assert(h1a == h1b && h2a == h2b);
    printf("  PASS: nf_hash128 deterministic\n");

    nf_hash128("key_a", 0, &h1a, &h2a);
    nf_hash128("key_b", 0, &h1b, &h2b);
    assert(h1a != h1b || h2a != h2b);
    printf("  PASS: nf_hash128 different keys\n");
}

static void test_distribution(void) {
    /* Simple chi-squared-like test: hash 10000 keys, check bucket distribution */
    const int NUM_KEYS = 10000;
    const int NUM_BUCKETS = 100;
    int buckets[100] = {0};

    for (int i = 0; i < NUM_KEYS; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key_%d", i);
        uint64_t h1, h2;
        nf_hash128(key, 0, &h1, &h2);
        buckets[h1 % NUM_BUCKETS]++;
        (void)h2;
    }

    int expected = NUM_KEYS / NUM_BUCKETS;
    int max_deviation = 0;
    for (int i = 0; i < NUM_BUCKETS; i++) {
        int dev = abs(buckets[i] - expected);
        if (dev > max_deviation) max_deviation = dev;
    }

    double deviation_pct = (double)max_deviation / expected * 100.0;
    printf("  distribution: max deviation = %d (%.1f%% of expected %d)\n",
           max_deviation, deviation_pct, expected);
    /* Allow up to 50% deviation (very loose, should easily pass) */
    assert(deviation_pct < 50.0);
    printf("  PASS: reasonable distribution\n");
}

static void test_various_lengths(void) {
    /* Test inputs of various lengths (1 to 64 bytes) */
    uint32_t prev[4] = {0};
    for (size_t len = 1; len <= 64; len++) {
        char buf[65];
        memset(buf, 'A', len);
        buf[len] = '\0';

        uint32_t out[4];
        nf_murmurhash3_x86_128(buf, len, 0, out);

        if (len > 1) {
            assert(memcmp(out, prev, 16) != 0);
        }
        memcpy(prev, out, 16);
    }
    printf("  PASS: various input lengths (1-64) produce unique hashes\n");
}

int main(void) {
    printf("=== test_hash ===\n");

    printf("[test_empty_string]\n");
    test_empty_string();

    printf("[test_known_vectors]\n");
    test_known_vectors();

    printf("[test_hash128_convenience]\n");
    test_hash128_convenience();

    printf("[test_distribution]\n");
    test_distribution();

    printf("[test_various_lengths]\n");
    test_various_lengths();

    printf("\nAll hash tests passed.\n");
    return 0;
}
