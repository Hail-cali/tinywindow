#ifndef NANOFILTER_H
#define NANOFILTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Error codes ────────────────────────────────────────────────── */

typedef enum {
    NF_OK             =  0,
    NF_ERR_ALLOC      = -1,
    NF_ERR_PARAM      = -2,
    NF_ERR_OVERFLOW    = -3,
} nf_status_t;

/* ── Hash ───────────────────────────────────────────────────────── */

/**
 * MurmurHash3_x86_128: produces 128-bit hash.
 * @param key   pointer to input data
 * @param len   length of input data in bytes
 * @param seed  hash seed
 * @param out   pointer to 128-bit (4 x uint32_t) output buffer
 */
void nf_murmurhash3_x86_128(const void *key, size_t len,
                             uint32_t seed, void *out);

/**
 * Convenience: hash a null-terminated string and return two 64-bit values.
 * Useful for generating d independent hash indices for CMS.
 */
void nf_hash128(const char *key, uint32_t seed,
                uint64_t *h1_out, uint64_t *h2_out);

/* ── Timing Count-Min Sketch ────────────────────────────────────── */

typedef struct nf_timing_cms nf_timing_cms_t;

/**
 * Create a Timing CMS instance.
 * @param depth         number of hash rows (d)
 * @param width         number of counters per row (w)
 * @param num_slots     number of time slots (t)
 * @param slot_duration_ms  duration of each slot in milliseconds
 * @return pointer to new instance, or NULL on failure
 */
nf_timing_cms_t *nf_tcms_create(uint32_t depth, uint32_t width,
                                 uint32_t num_slots,
                                 uint64_t slot_duration_ms);

/** Destroy a Timing CMS instance and free memory. */
void nf_tcms_destroy(nf_timing_cms_t *cms);

/**
 * Record an event (increment counters in the active slot).
 * @param cms       instance
 * @param key       event key (null-terminated)
 * @param now_ms    current timestamp in milliseconds
 */
nf_status_t nf_tcms_record(nf_timing_cms_t *cms, const char *key,
                            uint64_t now_ms);

/**
 * Query count within a time window.
 * @param cms       instance
 * @param key       event key (null-terminated)
 * @param window_ms window duration in milliseconds (up to num_slots * slot_duration_ms)
 * @param now_ms    current timestamp in milliseconds
 * @return estimated count (always >= true count)
 */
uint64_t nf_tcms_count(nf_timing_cms_t *cms, const char *key,
                        uint64_t window_ms, uint64_t now_ms);

/** Reset all counters to zero. */
void nf_tcms_reset(nf_timing_cms_t *cms);

/** Get total memory usage in bytes. */
size_t nf_tcms_memory_usage(const nf_timing_cms_t *cms);

/* ── Sliding Bloom Filter ───────────────────────────────────────── */

typedef struct nf_sliding_bf nf_sliding_bf_t;

/**
 * Create a Sliding Bloom Filter instance.
 * @param expected_items  expected number of distinct items
 * @param fp_rate         desired false positive rate (e.g. 0.01)
 * @param num_slots       number of time slots
 * @param slot_duration_ms  duration of each slot in milliseconds
 * @return pointer to new instance, or NULL on failure
 */
nf_sliding_bf_t *nf_sbf_create(uint64_t expected_items, double fp_rate,
                                uint32_t num_slots,
                                uint64_t slot_duration_ms);

/** Destroy a Sliding Bloom Filter instance. */
void nf_sbf_destroy(nf_sliding_bf_t *bf);

/**
 * Insert an item into the active slot.
 * @param bf        instance
 * @param key       item key (null-terminated)
 * @param now_ms    current timestamp in milliseconds
 */
nf_status_t nf_sbf_insert(nf_sliding_bf_t *bf, const char *key,
                           uint64_t now_ms);

/**
 * Check if an item might exist within the time window.
 * @param bf        instance
 * @param key       item key (null-terminated)
 * @param window_ms window duration in milliseconds
 * @param now_ms    current timestamp in milliseconds
 * @return true if item might exist, false if definitely not
 */
bool nf_sbf_might_contain(nf_sliding_bf_t *bf, const char *key,
                           uint64_t window_ms, uint64_t now_ms);

/** Get total memory usage in bytes. */
size_t nf_sbf_memory_usage(const nf_sliding_bf_t *bf);

/* ── Window Manager ─────────────────────────────────────────────── */

typedef struct nf_window nf_window_t;

/**
 * Create a window manager.
 * @param num_slots         number of time slots
 * @param slot_duration_ms  duration of each slot in milliseconds
 * @return pointer to new instance, or NULL on failure
 */
nf_window_t *nf_window_create(uint32_t num_slots, uint64_t slot_duration_ms);

/** Destroy a window manager. */
void nf_window_destroy(nf_window_t *win);

/**
 * Advance the window to the given timestamp, returning the number of
 * slots that were rotated (and should be cleared).
 */
uint32_t nf_window_advance(nf_window_t *win, uint64_t now_ms);

/** Get the current active slot index. */
uint32_t nf_window_active_slot(const nf_window_t *win);

/** Get the size of the window struct in bytes. */
size_t nf_window_sizeof(void);

/**
 * Lockless check: does the window need to advance?
 * Used as a fast-path to avoid unnecessary mutex acquisition.
 */
bool nf_window_needs_advance(const nf_window_t *win, uint64_t now_ms);

/**
 * Get the number of slots that cover the given window duration.
 * Clamped to num_slots.
 */
uint32_t nf_window_slots_for(const nf_window_t *win, uint64_t window_ms);

/* ── Memory utilities ───────────────────────────────────────────── */

/**
 * Allocate cache-line aligned memory block.
 * @param size  number of bytes
 * @return pointer to aligned memory, or NULL on failure
 */
void *nf_aligned_alloc(size_t size);

/** Free memory allocated by nf_aligned_alloc. */
void nf_aligned_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* NANOFILTER_H */
