# Benchmark Results

> Results will be populated after Phase 1 and Phase 3 completion.

## C Core Benchmarks

Run with: `cd core && mkdir build && cd build && cmake .. && make && ./bench_throughput`

### Target Performance

| Operation | Target | Actual |
|-----------|--------|--------|
| MurmurHash3 (per key) | < 100ns | TBD |
| TCMS record | < 1us | TBD |
| TCMS count (single window) | < 1us | TBD |
| SBF insert | < 1us | TBD |
| SBF query | < 1us | TBD |

## JVM Benchmarks (via JNI)

### Target Performance

| Operation | Target | Actual |
|-----------|--------|--------|
| record() | < 5us (p99) | TBD |
| count() | < 10us (p99) | TBD |
| countAll() (3 windows) | < 20us (p99) | TBD |

## vs Redis

| Metric | nanofilter | Redis (localhost) | Improvement |
|--------|-----------|-------------------|-------------|
| Single count latency | TBD | ~500us | TBD |
| countAll (3 windows) | TBD | ~1500us | TBD |
| Memory (5M pairs, 3 windows) | ~11MB | ~200MB+ | TBD |

## Memory Usage

| Configuration | Memory |
|--------------|--------|
| 500K pairs, 1 window (1h) | TBD |
| 5M pairs, 3 windows (1h/24h/7d) | ~11MB (estimated) |
| 10M pairs, 3 windows | TBD |

## Methodology

- C benchmarks: `clock_gettime(CLOCK_MONOTONIC)`, 1M iterations
- JVM benchmarks: JMH or `measureTime` with warmup
- Redis benchmarks: Jedis client, localhost, single-threaded
- All benchmarks on: Apple M-series / Linux x86_64
