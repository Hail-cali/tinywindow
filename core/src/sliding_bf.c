#include "tinywindow.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

/*
 * Sliding Bloom Filter (thread-safe)
 *
 * Concurrency model:
 *   - Bit set: atomic OR (lock-free)
 *   - Bit read: atomic load (lock-free)
 *   - Window advance + slot clear: pthread_mutex
 *   - Lifecycle: atomic destroyed flag + CAS in destroy (lock-free)
 *
 * Caller must ensure no in-flight operations when calling destroy (MEM30-C).
 */

struct tw_sliding_bf {
    uint64_t    bits_per_slot;
    uint32_t    num_hashes;
    uint32_t    num_slots;
    uint64_t    slot_duration_ms;
    uint8_t    *bits;
    tw_window_t *window;
    uint32_t    seed;
    pthread_mutex_t advance_lock;
    bool       destroyed;
};

static inline size_t byte_index(const tw_sliding_bf_t *bf,
                                 uint32_t slot, uint64_t bit) {
    return (size_t)slot * ((bf->bits_per_slot + 7) / 8) + (size_t)(bit / 8);
}

static inline void set_bit(tw_sliding_bf_t *bf, uint32_t slot, uint64_t bit) {
    size_t idx = byte_index(bf, slot, bit);
    __atomic_fetch_or(&bf->bits[idx], (uint8_t)(1u << (bit % 8)), __ATOMIC_RELAXED);
}

static inline bool get_bit(const tw_sliding_bf_t *bf, uint32_t slot,
                            uint64_t bit) {
    size_t idx = byte_index(bf, slot, bit);
    uint8_t val = __atomic_load_n(&bf->bits[idx], __ATOMIC_RELAXED);
    return (val & (uint8_t)(1u << (bit % 8))) != 0;
}

static void clear_slot_bits(tw_sliding_bf_t *bf, uint32_t slot) {
    size_t bytes_per_slot = (bf->bits_per_slot + 7) / 8;
    memset(bf->bits + (size_t)slot * bytes_per_slot, 0, bytes_per_slot);
}

static void advance_and_clear(tw_sliding_bf_t *bf, uint64_t now_ms) {
    /* Fast path: skip lock when no rotation is needed (vast majority of calls) */
    if (!tw_window_needs_advance(bf->window, now_ms)) return;

    pthread_mutex_lock(&bf->advance_lock);

    uint32_t old_active = tw_window_active_slot(bf->window);
    uint32_t rotated = tw_window_advance(bf->window, now_ms);

    for (uint32_t i = 1; i <= rotated; i++) {
        uint32_t slot_to_clear = (old_active + i) % bf->num_slots;
        clear_slot_bits(bf, slot_to_clear);
    }

    pthread_mutex_unlock(&bf->advance_lock);
}

tw_sliding_bf_t *tw_sbf_create(uint64_t expected_items, double fp_rate,
                                uint32_t num_slots,
                                uint64_t slot_duration_ms) {
    if (expected_items == 0 || fp_rate <= 0.0 || fp_rate >= 1.0 ||
        num_slots == 0 || slot_duration_ms == 0) {
        return NULL;
    }

    tw_sliding_bf_t *bf = (tw_sliding_bf_t *)malloc(sizeof(tw_sliding_bf_t));
    if (!bf) return NULL;

    double n = (double)expected_items / (double)num_slots;
    double m = -(n * log(fp_rate)) / (log(2.0) * log(2.0));
    double k = (m / n) * log(2.0);

    bf->bits_per_slot = (uint64_t)ceil(m);
    if (bf->bits_per_slot < 64) bf->bits_per_slot = 64;
    bf->num_hashes = (uint32_t)ceil(k);
    if (bf->num_hashes < 1) bf->num_hashes = 1;
    if (bf->num_hashes > 30) bf->num_hashes = 30;
    bf->num_slots = num_slots;
    bf->slot_duration_ms = slot_duration_ms;
    bf->seed = 0xa5b9c3d7;
    bf->destroyed = false;

    if (pthread_mutex_init(&bf->advance_lock, NULL) != 0) {
        free(bf);
        return NULL;
    }

    size_t bytes_per_slot = (bf->bits_per_slot + 7) / 8;
    /* Overflow check */
    if (bytes_per_slot > SIZE_MAX / num_slots) {
        pthread_mutex_destroy(&bf->advance_lock);
        free(bf);
        return NULL;
    }
    size_t total_bytes = (size_t)num_slots * bytes_per_slot;
    bf->bits = (uint8_t *)tw_aligned_alloc(total_bytes);
    if (!bf->bits) {
        pthread_mutex_destroy(&bf->advance_lock);
        free(bf);
        return NULL;
    }

    bf->window = tw_window_create(num_slots, slot_duration_ms);
    if (!bf->window) {
        tw_aligned_free(bf->bits);
        pthread_mutex_destroy(&bf->advance_lock);
        free(bf);
        return NULL;
    }

    return bf;
}

void tw_sbf_destroy(tw_sliding_bf_t *bf) {
    if (!bf) return;

    bool expected = false;
    if (!__atomic_compare_exchange_n(&bf->destroyed, &expected, true,
                                      false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return;

    pthread_mutex_destroy(&bf->advance_lock);
    tw_window_destroy(bf->window);
    tw_aligned_free(bf->bits);
    free(bf);
}

tw_status_t tw_sbf_insert(tw_sliding_bf_t *bf, const char *key,
                           uint64_t now_ms) {
    if (!bf || !key) return TW_ERR_PARAM;
    if (__atomic_load_n(&bf->destroyed, __ATOMIC_ACQUIRE)) return TW_ERR_PARAM;

    uint64_t h1, h2;
    tw_hash128(key, bf->seed, &h1, &h2);

    advance_and_clear(bf, now_ms);

    uint32_t active = tw_window_active_slot(bf->window);
    for (uint32_t i = 0; i < bf->num_hashes; i++) {
        uint64_t h = h1 + (uint64_t)i * h2;
        uint64_t bit = h % bf->bits_per_slot;
        set_bit(bf, active, bit);
    }
    return TW_OK;
}

bool tw_sbf_might_contain(tw_sliding_bf_t *bf, const char *key,
                           uint64_t window_ms, uint64_t now_ms) {
    if (!bf || !key) return false;
    if (__atomic_load_n(&bf->destroyed, __ATOMIC_ACQUIRE)) return false;

    uint64_t h1, h2;
    tw_hash128(key, bf->seed, &h1, &h2);

    advance_and_clear(bf, now_ms);

    uint32_t active = tw_window_active_slot(bf->window);
    uint32_t query_slots = tw_window_slots_for(bf->window, window_ms);

    for (uint32_t i = 0; i < bf->num_hashes; i++) {
        uint64_t h = h1 + (uint64_t)i * h2;
        uint64_t bit = h % bf->bits_per_slot;

        bool found = false;
        for (uint32_t s = 0; s < query_slots && !found; s++) {
            uint32_t slot = (active + bf->num_slots - s) % bf->num_slots;
            if (get_bit(bf, slot, bit)) {
                found = true;
            }
        }
        if (!found) return false;
    }
    return true;
}

size_t tw_sbf_memory_usage(const tw_sliding_bf_t *bf) {
    if (!bf) return 0;
    size_t bytes_per_slot = (bf->bits_per_slot + 7) / 8;
    size_t bits_size = (size_t)bf->num_slots * bytes_per_slot;
    return sizeof(tw_sliding_bf_t) + bits_size + tw_window_sizeof();
}
