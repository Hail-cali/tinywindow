#include "tinywindow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void bench_hash(void) {
    printf("--- Hash Benchmark ---\n");

    const int ITERS = 1000000;
    char key[64];
    uint64_t h1, h2;

    uint64_t start = now_ns();
    for (int i = 0; i < ITERS; i++) {
        snprintf(key, sizeof(key), "user:%d:campaign:%d", i % 10000, i % 1000);
        tw_hash128(key, 0, &h1, &h2);
    }
    uint64_t elapsed = now_ns() - start;

    printf("  %d hashes in %.3f ms\n", ITERS, (double)elapsed / 1e6);
    printf("  %.1f ns/hash\n", (double)elapsed / ITERS);
    printf("  %.1f M hashes/sec\n", (double)ITERS / ((double)elapsed / 1e9) / 1e6);
    (void)h1; (void)h2;
}

static void bench_tcms_record(void) {
    printf("\n--- Timing CMS Record Benchmark ---\n");

    /* Realistic config: 5 rows, 50000 cols, 12 slots of 5 min */
    tw_timing_cms_t *cms = tw_tcms_create(5, 50000, 12, 300000);
    if (!cms) { printf("  FAILED to create CMS\n"); return; }

    const int ITERS = 1000000;
    uint64_t now = 1000000;
    char key[64];

    uint64_t start = now_ns();
    for (int i = 0; i < ITERS; i++) {
        snprintf(key, sizeof(key), "user:%d:campaign:%d", i % 100000, i % 10000);
        tw_tcms_record(cms, key, now);
    }
    uint64_t elapsed = now_ns() - start;

    printf("  %d records in %.3f ms\n", ITERS, (double)elapsed / 1e6);
    printf("  %.1f ns/record (%.2f us)\n",
           (double)elapsed / ITERS, (double)elapsed / ITERS / 1000.0);
    printf("  memory: %zu bytes (%.2f MB)\n",
           tw_tcms_memory_usage(cms),
           (double)tw_tcms_memory_usage(cms) / (1024.0 * 1024.0));

    tw_tcms_destroy(cms);
}

static void bench_tcms_count(void) {
    printf("\n--- Timing CMS Count Benchmark ---\n");

    tw_timing_cms_t *cms = tw_tcms_create(5, 50000, 12, 300000);
    if (!cms) { printf("  FAILED to create CMS\n"); return; }

    uint64_t now = 1000000;
    char key[64];

    /* Pre-populate */
    for (int i = 0; i < 100000; i++) {
        snprintf(key, sizeof(key), "user:%d:campaign:%d", i % 100000, i % 10000);
        tw_tcms_record(cms, key, now);
    }

    const int ITERS = 1000000;
    uint64_t start = now_ns();
    volatile uint64_t sum = 0;
    for (int i = 0; i < ITERS; i++) {
        snprintf(key, sizeof(key), "user:%d:campaign:%d", i % 100000, i % 10000);
        sum += tw_tcms_count(cms, key, 3600000, now);
    }
    uint64_t elapsed = now_ns() - start;

    printf("  %d counts in %.3f ms\n", ITERS, (double)elapsed / 1e6);
    printf("  %.1f ns/count (%.2f us)\n",
           (double)elapsed / ITERS, (double)elapsed / ITERS / 1000.0);
    (void)sum;

    tw_tcms_destroy(cms);
}

static void bench_sbf(void) {
    printf("\n--- Sliding Bloom Filter Benchmark ---\n");

    tw_sliding_bf_t *bf = tw_sbf_create(1000000, 0.01, 12, 300000);
    if (!bf) { printf("  FAILED to create SBF\n"); return; }

    uint64_t now = 1000000;
    char key[64];

    const int ITERS = 1000000;

    /* Insert benchmark */
    uint64_t start = now_ns();
    for (int i = 0; i < ITERS; i++) {
        snprintf(key, sizeof(key), "click:%d", i);
        tw_sbf_insert(bf, key, now);
    }
    uint64_t elapsed = now_ns() - start;
    printf("  insert: %.1f ns/op (%.2f us)\n",
           (double)elapsed / ITERS, (double)elapsed / ITERS / 1000.0);

    /* Query benchmark */
    start = now_ns();
    volatile int found = 0;
    for (int i = 0; i < ITERS; i++) {
        snprintf(key, sizeof(key), "click:%d", i);
        if (tw_sbf_might_contain(bf, key, 3600000, now)) found++;
    }
    elapsed = now_ns() - start;
    printf("  query:  %.1f ns/op (%.2f us)\n",
           (double)elapsed / ITERS, (double)elapsed / ITERS / 1000.0);
    printf("  memory: %zu bytes (%.2f MB)\n",
           tw_sbf_memory_usage(bf),
           (double)tw_sbf_memory_usage(bf) / (1024.0 * 1024.0));
    (void)found;

    tw_sbf_destroy(bf);
}

int main(void) {
    printf("=== tinywindow C benchmark ===\n\n");
    bench_hash();
    bench_tcms_record();
    bench_tcms_count();
    bench_sbf();
    printf("\nDone.\n");
    return 0;
}
