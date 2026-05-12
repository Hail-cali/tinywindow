# Algorithm Design

## Timing Count-Min Sketch

### Standard Count-Min Sketch (CMS)

A CMS is a probabilistic data structure for frequency counting:
- `d` independent hash functions mapping items to `w` columns
- Each `record(item)` increments `d` counters
- `count(item)` returns the minimum across `d` rows
- Guarantee: estimate >= true count (never underestimates)
- Error bound: with probability `1-delta`, error < `epsilon * N`
  - `w = ceil(e/epsilon)`, `d = ceil(ln(1/delta))`

### Time Extension

Timing CMS adds a third dimension: **time slots**.

```
Standard CMS:     d x w          (2D counter array)
Timing CMS:       d x w x t     (3D counter array)
```

Each time slot covers a fixed duration (e.g., 5 minutes). When time advances:
1. Compute how many slots have elapsed
2. Zero-fill the newly active slot(s)
3. Advance the circular pointer

Querying over a window of duration `W`:
1. Determine how many slots `s` cover `W`
2. For each row: sum counters across `s` slots, starting from active
3. Return the minimum row sum (standard CMS min-of-rows)

### Kirsch-Mitzenmacher Optimization

Instead of `d` independent hash functions, we use 2 hashes and derive `d`:

```
h_i(x) = h1(x) + i * h2(x)
```

This gives the same theoretical guarantees with only one hash computation.

### Memory Layout

Counters are stored in a single contiguous, cache-line-aligned block:

```
counters[row * width * num_slots + col * num_slots + slot]
```

This layout is slot-friendly: clearing a slot requires strided access,
but recording/counting (which are hot paths) access contiguous slot data.

## Sliding Bloom Filter

For deduplication, we use a time-slotted Bloom filter:
- Each slot has its own bit array
- Insert: set bits in the active slot
- Query: check if ALL hash bits are set in ANY slot within the window (union)
- Expiry: zero-fill stale slots on time advance

Optimal parameters:
- `m = -n * ln(fp) / (ln2)^2` bits per slot
- `k = (m/n) * ln2` hash functions

## Window Manager

A shared component that tracks time progression:
- Maintains a circular slot pointer and last-advance timestamp
- `advance(now)` returns the count of elapsed slots
- Callers are responsible for clearing stale data
- Idempotent: multiple calls with the same timestamp are no-ops
