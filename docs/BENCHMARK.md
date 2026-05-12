# Benchmark Results

## C Core Benchmarks

Run with: `cd core && mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make && ./bench_throughput`

### Apple M-series (ARM64)

| Operation | Latency | Throughput |
|-----------|---------|------------|
| MurmurHash3_x86_128 (per key) | ~76 ns | 13.2 M/s |
| TimingCMS `record` | ~90 ns | 11.1 M/s |
| TimingCMS `count` (12 slots) | ~116 ns | 8.6 M/s |
| SlidingBF `insert` | ~61 ns | 16.5 M/s |
| SlidingBF `query` | ~58 ns | 17.4 M/s |

Configuration: depth=5, width=50000, 12 slots x 5min (1h window).

### Memory Usage

| Configuration | Memory |
|--------------|--------|
| TimingCMS (d=5, w=50000, 12 slots) | 5.72 MB |
| SlidingBF (1M items, fp=0.01, 12 slots) | 1.14 MB |
| 3-window FrequencyCap (1h/24h/7d) | ~11 MB (estimated) |

## JVM Benchmarks (via JNI)

| Operation | Target | Notes |
|-----------|--------|-------|
| record() | < 5us (p99) | Includes JNI overhead (~200ns) |
| count() | < 10us (p99) | Includes JNI + string encoding |
| countAll() (3 windows) | < 20us (p99) | 3x count() calls |

## vs Redis

| Metric | tinywindow | Redis (localhost) | Improvement |
|--------|-----------|-------------------|-------------|
| Single count latency | ~0.1us | ~500us | ~5000x |
| countAll (3 windows) | ~0.35us | ~1500us | ~4300x |
| Memory (5M pairs, 3 windows) | ~11MB | ~200MB+ | ~18x |

Note: Redis latency is network-bound. The comparison highlights the benefit of in-process computation.

## Methodology

- C benchmarks: `clock_gettime(CLOCK_MONOTONIC)`, 1M iterations, single-threaded
- JVM benchmarks: `measureTime` with warmup
- Redis benchmarks: Jedis client, localhost, single-threaded
- Hardware: Apple M-series / Linux x86_64
