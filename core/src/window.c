#include "nanofilter.h"
#include <stdlib.h>

struct nf_window {
    uint32_t num_slots;
    uint64_t slot_duration_ms;
    uint32_t active_slot;       /* index of the current active slot (circular) */
    uint64_t last_advance_ms;   /* timestamp of last advance */
};

nf_window_t *nf_window_create(uint32_t num_slots, uint64_t slot_duration_ms) {
    if (num_slots == 0 || slot_duration_ms == 0) return NULL;

    nf_window_t *win = (nf_window_t *)malloc(sizeof(nf_window_t));
    if (!win) return NULL;

    win->num_slots = num_slots;
    win->slot_duration_ms = slot_duration_ms;
    win->active_slot = 0;
    win->last_advance_ms = 0;
    return win;
}

void nf_window_destroy(nf_window_t *win) {
    free(win);
}

uint32_t nf_window_advance(nf_window_t *win, uint64_t now_ms) {
    if (!win) return 0;

    if (win->last_advance_ms == 0) {
        win->last_advance_ms = now_ms;
        return 0;
    }

    if (now_ms <= win->last_advance_ms) {
        return 0;
    }

    uint64_t elapsed = now_ms - win->last_advance_ms;
    uint32_t slots_elapsed = (uint32_t)(elapsed / win->slot_duration_ms);

    if (slots_elapsed == 0) return 0;

    /* Clamp: if more slots elapsed than total, everything is stale */
    if (slots_elapsed > win->num_slots) {
        slots_elapsed = win->num_slots;
    }

    /* Advance the active slot pointer */
    win->active_slot = (win->active_slot + slots_elapsed) % win->num_slots;
    win->last_advance_ms += (uint64_t)slots_elapsed * win->slot_duration_ms;

    return slots_elapsed;
}

uint32_t nf_window_active_slot(const nf_window_t *win) {
    if (!win) return 0;
    return win->active_slot;
}

size_t nf_window_sizeof(void) {
    return sizeof(nf_window_t);
}

bool nf_window_needs_advance(const nf_window_t *win, uint64_t now_ms) {
    if (!win) return false;
    uint64_t last = win->last_advance_ms;
    if (last == 0) return true;           /* first call: needs initialization */
    if (now_ms <= last) return false;
    return (now_ms - last) >= win->slot_duration_ms;
}

uint32_t nf_window_slots_for(const nf_window_t *win, uint64_t window_ms) {
    if (!win) return 0;
    uint32_t slots = (uint32_t)((window_ms + win->slot_duration_ms - 1)
                                / win->slot_duration_ms);
    if (slots > win->num_slots) {
        slots = win->num_slots;
    }
    return slots;
}
