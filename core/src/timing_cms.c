#include "tinywindow.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/*
 * Timing Count-Min Sketch (thread-safe)
 *
 * Concurrency model:
 *   - Counter read/write: atomic operations (lock-free)
 *   - Window advance + slot clear: pthread_mutex (rare, every slot_duration)
 *   - Lifecycle: atomic destroyed flag + CAS in destroy (lock-free)
 *   - Hash computation: outside any lock
 *
 * destroy() uses CAS to prevent double-free. record()/count() check the
 * destroyed flag on entry (~1ns overhead). Caller must ensure no in-flight
 * operations when calling destroy — standard C lifecycle contract (MEM30-C).
 */

struct tw_timing_cms {
    uint32_t   depth;
    uint32_t   width;
    uint32_t   num_slots;
    uint64_t   slot_duration_ms;
    uint16_t  *counters;         /* d x w x t array, cache-line aligned */
    tw_window_t *window;
    uint32_t   seed;
    pthread_mutex_t advance_lock; /* protects window advance + slot clear */
    bool       destroyed;        /* atomic: set by destroy(), checked by record()/count() */
};

static inline size_t counter_index(const tw_timing_cms_t *cms,
                                    uint32_t row, uint32_t col,
                                    uint32_t slot) {
    return ((size_t)row * cms->width + col) * cms->num_slots + slot;
}

static void clear_slot(tw_timing_cms_t *cms, uint32_t slot) {
    for (uint32_t r = 0; r < cms->depth; r++) {
        for (uint32_t c = 0; c < cms->width; c++) {
            size_t idx = counter_index(cms, r, c, slot);
            __atomic_store_n(&cms->counters[idx], 0, __ATOMIC_RELAXED);
        }
    }
}

static void advance_and_clear(tw_timing_cms_t *cms, uint64_t now_ms) {
    /* Fast path: skip lock when no rotation is needed (vast majority of calls) */
    if (!tw_window_needs_advance(cms->window, now_ms)) return;

    pthread_mutex_lock(&cms->advance_lock);

    uint32_t old_active = tw_window_active_slot(cms->window);
    uint32_t rotated = tw_window_advance(cms->window, now_ms);

    for (uint32_t i = 1; i <= rotated; i++) {
        uint32_t slot_to_clear = (old_active + i) % cms->num_slots;
        clear_slot(cms, slot_to_clear);
    }

    pthread_mutex_unlock(&cms->advance_lock);
}

tw_timing_cms_t *tw_tcms_create(uint32_t depth, uint32_t width,
                                 uint32_t num_slots,
                                 uint64_t slot_duration_ms) {
    if (depth == 0 || width == 0 || num_slots == 0 || slot_duration_ms == 0) {
        return NULL;
    }

    tw_timing_cms_t *cms = (tw_timing_cms_t *)malloc(sizeof(tw_timing_cms_t));
    if (!cms) return NULL;

    cms->depth = depth;
    cms->width = width;
    cms->num_slots = num_slots;
    cms->slot_duration_ms = slot_duration_ms;
    cms->seed = 0x9747b28c;
    cms->destroyed = false;

    if (pthread_mutex_init(&cms->advance_lock, NULL) != 0) {
        free(cms);
        return NULL;
    }

    /* Overflow check: depth * width * num_slots * sizeof(uint16_t) */
    size_t total_counters = (size_t)depth * width;
    if (total_counters / depth != width ||
        total_counters > SIZE_MAX / num_slots) {
        pthread_mutex_destroy(&cms->advance_lock);
        free(cms);
        return NULL;
    }
    total_counters *= num_slots;
    if (total_counters > SIZE_MAX / sizeof(uint16_t)) {
        pthread_mutex_destroy(&cms->advance_lock);
        free(cms);
        return NULL;
    }
    cms->counters = (uint16_t *)tw_aligned_alloc(total_counters * sizeof(uint16_t));
    if (!cms->counters) {
        pthread_mutex_destroy(&cms->advance_lock);
        free(cms);
        return NULL;
    }

    cms->window = tw_window_create(num_slots, slot_duration_ms);
    if (!cms->window) {
        tw_aligned_free(cms->counters);
        pthread_mutex_destroy(&cms->advance_lock);
        free(cms);
        return NULL;
    }

    return cms;
}

void tw_tcms_destroy(tw_timing_cms_t *cms) {
    if (!cms) return;

    /* Prevent double-free: only the first caller proceeds */
    bool expected = false;
    if (!__atomic_compare_exchange_n(&cms->destroyed, &expected, true,
                                      false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return;

    pthread_mutex_destroy(&cms->advance_lock);
    tw_window_destroy(cms->window);
    tw_aligned_free(cms->counters);
    free(cms);
}

tw_status_t tw_tcms_record(tw_timing_cms_t *cms, const char *key,
                            uint64_t now_ms) {
    if (!cms || !key) return TW_ERR_PARAM;
    if (__atomic_load_n(&cms->destroyed, __ATOMIC_ACQUIRE)) return TW_ERR_PARAM;

    /* Hash outside the lock */
    uint64_t h1, h2;
    tw_hash128(key, cms->seed, &h1, &h2);

    /* Advance window (lock only if rotation needed, very rare) */
    advance_and_clear(cms, now_ms);

    /*
     * Atomic counter increment (lock-free).
     * Note: the saturation check (old < UINT16_MAX) is not strictly atomic
     * with the fetch_add — under extreme contention on the same cell, a
     * counter could briefly wrap past UINT16_MAX. This is benign in practice:
     * hitting 65535 in a single slot means capacity is far exceeded.
     */
    uint32_t active = tw_window_active_slot(cms->window);
    for (uint32_t r = 0; r < cms->depth; r++) {
        uint64_t h = h1 + (uint64_t)r * h2;
        uint32_t col = (uint32_t)(h % cms->width);
        size_t idx = counter_index(cms, r, col, active);

        uint16_t old = __atomic_load_n(&cms->counters[idx], __ATOMIC_RELAXED);
        if (old < UINT16_MAX) {
            __atomic_fetch_add(&cms->counters[idx], 1, __ATOMIC_RELAXED);
        }
    }
    return TW_OK;
}

uint64_t tw_tcms_count(tw_timing_cms_t *cms, const char *key,
                        uint64_t window_ms, uint64_t now_ms) {
    if (!cms || !key) return 0;
    if (__atomic_load_n(&cms->destroyed, __ATOMIC_ACQUIRE)) return 0;

    /* Hash outside the lock */
    uint64_t h1, h2;
    tw_hash128(key, cms->seed, &h1, &h2);

    advance_and_clear(cms, now_ms);

    /* Atomic reads (lock-free) */
    uint32_t active = tw_window_active_slot(cms->window);
    uint32_t query_slots = tw_window_slots_for(cms->window, window_ms);

    uint64_t min_sum = UINT64_MAX;

    for (uint32_t r = 0; r < cms->depth; r++) {
        uint64_t h = h1 + (uint64_t)r * h2;
        uint32_t col = (uint32_t)(h % cms->width);

        uint64_t row_sum = 0;
        for (uint32_t s = 0; s < query_slots; s++) {
            uint32_t slot = (active + cms->num_slots - s) % cms->num_slots;
            size_t idx = counter_index(cms, r, col, slot);
            row_sum += __atomic_load_n(&cms->counters[idx], __ATOMIC_RELAXED);
        }

        if (row_sum < min_sum) {
            min_sum = row_sum;
        }
    }

    return (min_sum == UINT64_MAX) ? 0 : min_sum;
}

void tw_tcms_reset(tw_timing_cms_t *cms) {
    if (!cms) return;
    if (__atomic_load_n(&cms->destroyed, __ATOMIC_ACQUIRE)) return;
    pthread_mutex_lock(&cms->advance_lock);
    size_t total = (size_t)cms->depth * cms->width * cms->num_slots;
    memset(cms->counters, 0, total * sizeof(uint16_t));
    pthread_mutex_unlock(&cms->advance_lock);
}

size_t tw_tcms_memory_usage(const tw_timing_cms_t *cms) {
    if (!cms) return 0;
    size_t counters_size = (size_t)cms->depth * cms->width
                           * cms->num_slots * sizeof(uint16_t);
    return sizeof(tw_timing_cms_t) + counters_size + tw_window_sizeof();
}
