#include "tinywindow.h"
#include <stdio.h>
#include <assert.h>

static void test_create_destroy(void) {
    tw_window_t *win = tw_window_create(6, 10000);
    assert(win != NULL);
    assert(tw_window_active_slot(win) == 0);
    tw_window_destroy(win);

    /* Invalid params */
    assert(tw_window_create(0, 10000) == NULL);
    assert(tw_window_create(6, 0) == NULL);

    printf("  PASS: create/destroy\n");
}

static void test_first_advance_initializes(void) {
    tw_window_t *win = tw_window_create(6, 10000);

    /* First call initializes last_advance_ms, returns 0 rotated */
    assert(tw_window_advance(win, 1000000) == 0);
    assert(tw_window_active_slot(win) == 0);

    tw_window_destroy(win);
    printf("  PASS: first advance initializes\n");
}

static void test_no_advance_within_slot(void) {
    tw_window_t *win = tw_window_create(6, 10000);
    uint64_t t = 1000000;

    tw_window_advance(win, t);

    /* Time passes but less than one slot duration */
    assert(tw_window_advance(win, t + 5000) == 0);
    assert(tw_window_active_slot(win) == 0);

    tw_window_destroy(win);
    printf("  PASS: no advance within slot\n");
}

static void test_single_slot_advance(void) {
    tw_window_t *win = tw_window_create(6, 10000);
    uint64_t t = 1000000;

    tw_window_advance(win, t);

    assert(tw_window_advance(win, t + 10000) == 1);
    assert(tw_window_active_slot(win) == 1);

    tw_window_destroy(win);
    printf("  PASS: single slot advance\n");
}

static void test_multi_slot_advance(void) {
    tw_window_t *win = tw_window_create(6, 10000);
    uint64_t t = 1000000;

    tw_window_advance(win, t);

    assert(tw_window_advance(win, t + 35000) == 3);
    assert(tw_window_active_slot(win) == 3);

    tw_window_destroy(win);
    printf("  PASS: multi slot advance\n");
}

static void test_circular_wrap(void) {
    tw_window_t *win = tw_window_create(4, 10000);
    uint64_t t = 1000000;

    tw_window_advance(win, t);

    /* Advance 3 slots: 0 -> 3 */
    tw_window_advance(win, t + 30000);
    assert(tw_window_active_slot(win) == 3);

    /* Advance 2 more: 3 -> 5 % 4 = 1 */
    assert(tw_window_advance(win, t + 50000) == 2);
    assert(tw_window_active_slot(win) == 1);

    tw_window_destroy(win);
    printf("  PASS: circular wrap\n");
}

static void test_clamp_to_num_slots(void) {
    tw_window_t *win = tw_window_create(4, 10000);
    uint64_t t = 1000000;

    tw_window_advance(win, t);

    /* Advance by 100 slots worth of time — should clamp to num_slots (4) */
    assert(tw_window_advance(win, t + 1000000) == 4);

    tw_window_destroy(win);
    printf("  PASS: clamp to num_slots\n");
}

static void test_needs_advance(void) {
    tw_window_t *win = tw_window_create(6, 10000);
    uint64_t t = 1000000;

    /* Before first advance: needs init */
    assert(tw_window_needs_advance(win, t) == true);

    tw_window_advance(win, t);

    /* Same timestamp: no advance needed */
    assert(tw_window_needs_advance(win, t) == false);

    /* Within same slot: no advance needed */
    assert(tw_window_needs_advance(win, t + 5000) == false);

    /* At slot boundary: advance needed */
    assert(tw_window_needs_advance(win, t + 10000) == true);

    /* Past time: no advance needed */
    assert(tw_window_needs_advance(win, t - 1000) == false);

    tw_window_destroy(win);
    printf("  PASS: needs_advance\n");
}

static void test_slots_for(void) {
    tw_window_t *win = tw_window_create(12, 300000); /* 12 slots x 5min */

    /* 1 hour = 3600000ms / 300000ms = 12 slots (exact) */
    assert(tw_window_slots_for(win, 3600000) == 12);

    /* 30 min = 1800000ms / 300000ms = 6 slots */
    assert(tw_window_slots_for(win, 1800000) == 6);

    /* 1 ms = ceil(1/300000) = 1 slot */
    assert(tw_window_slots_for(win, 1) == 1);

    /* 0 ms = 0 slots */
    assert(tw_window_slots_for(win, 0) == 0);

    /* Larger than total window: clamped to 12 */
    assert(tw_window_slots_for(win, 7200000) == 12);

    tw_window_destroy(win);
    printf("  PASS: slots_for\n");
}

static void test_idempotent_advance(void) {
    tw_window_t *win = tw_window_create(6, 10000);
    uint64_t t = 1000000;

    tw_window_advance(win, t);
    tw_window_advance(win, t + 20000);
    assert(tw_window_active_slot(win) == 2);

    /* Same timestamp again: no-op */
    assert(tw_window_advance(win, t + 20000) == 0);
    assert(tw_window_active_slot(win) == 2);

    tw_window_destroy(win);
    printf("  PASS: idempotent advance\n");
}

int main(void) {
    printf("=== test_window ===\n");

    printf("[test_create_destroy]\n");
    test_create_destroy();

    printf("[test_first_advance_initializes]\n");
    test_first_advance_initializes();

    printf("[test_no_advance_within_slot]\n");
    test_no_advance_within_slot();

    printf("[test_single_slot_advance]\n");
    test_single_slot_advance();

    printf("[test_multi_slot_advance]\n");
    test_multi_slot_advance();

    printf("[test_circular_wrap]\n");
    test_circular_wrap();

    printf("[test_clamp_to_num_slots]\n");
    test_clamp_to_num_slots();

    printf("[test_needs_advance]\n");
    test_needs_advance();

    printf("[test_slots_for]\n");
    test_slots_for();

    printf("[test_idempotent_advance]\n");
    test_idempotent_advance();

    printf("\nAll window tests passed.\n");
    return 0;
}
