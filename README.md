# tinywindow

> Time-windowed probabilistic frequency counting for JVM
> C core + Kotlin/Python bindings | Apache 2.0

**"How many times did user ABC see campaign XYZ in the last 1h / 24h / 7d?"**
Answer this in **<0.1us**, in-process, with zero GC overhead.

## Why

Ad servers check per-user frequency on every bid request. The standard approach — Redis `INCR` + `EXPIRE` — adds **0.5-2ms** network latency per lookup. Multiply by 100 candidate ads at 500K QPS, and Redis becomes the bottleneck.

tinywindow eliminates the network hop entirely:

```
Redis GET per ad:  500-2000us  (network round-trip)
tinywindow:           ~100ns  (in-process, off-heap)
```

## Quick Start

```kotlin
val cap = TinyWindow.frequencyCap(
    expectedPairs = 5_000_000,
    windows = listOf(1.hours, 24.hours, 7.days),
    errorRate = 0.01
)

// Record an impression
cap.record("user:abc", "campaign:xyz")

// Query all windows at once
val counts = cap.countAll("user:abc", "campaign:xyz")
// -> {1h: 2, 24h: 5, 7d: 12}

if (counts[1.hours]!! >= 3) skip()
```

## How It Works

Standard Count-Min Sketch is a 2D counter array (`d` rows x `w` columns). tinywindow extends it into 3D by adding **circular time slots**:

```
Standard CMS:     d x w           (2D)
Timing CMS:       d x w x t      (3D, with time slots)

record("key")    -> increment counters in the active slot
count("key", 1h) -> sum counters across slots covering 1h, take min across rows
time passes      -> zero-fill expired slots, rotate circular pointer
```

Multiple windows (1h / 24h / 7d) run in parallel, each with its own slot granularity:
- **1h window**: 5-min slots x 12
- **24h window**: 1-hr slots x 24
- **7d window**: 6-hr slots x 28

See [docs/ALGORITHM.md](docs/ALGORITHM.md) for the full design.

## Use Cases

| Use Case | API | Description |
|----------|-----|-------------|
| Ad Frequency Capping | `FrequencyCap` | Multi-window impression counting per user x campaign |
| Stream Rate Limiting | `RateLimiter` | IP/user rate limiting inside Flink/Kafka pipelines |
| Click Deduplication | `Dedup` | Time-bounded duplicate detection via Sliding Bloom Filter |

## Comparison

| Solution | Time Decay | Freq Count | Multi-Window | JVM Native | Off-Heap |
|----------|:---------:|:----------:|:------------:|:----------:|:--------:|
| Redis INCR+EXPIRE | O | O | O (multi-key) | X (network) | - |
| Apache DataSketches CMS | X | O | X | O | X |
| Guava BloomFilter | X | X | X | O | X |
| **tinywindow** | **O** | **O** | **O** | **O (C+JNI)** | **O** |

## Architecture

```
User Code (Kotlin / Java / Python)
        |
        | JNI / cffi
        v
  tinywindow C core
  +-- TimingCMS    : time-slotted Count-Min Sketch (frequency counting)
  +-- SlidingBF    : time-slotted Bloom Filter (dedup / membership)
  +-- WindowManager: circular slot rotation and expiry
  +-- Memory       : cache-line aligned, single malloc, off-heap
```

## Spring Boot Integration

```kotlin
// build.gradle.kts
dependencies {
    implementation("io.tinywindow:tinywindow:0.1.0")
}
```

```kotlin
@Service
class FrequencyCapService {

    private val cap = TinyWindow.frequencyCap(
        expectedPairs = 5_000_000,
        windows = listOf(1.hours, 24.hours, 7.days),
        errorRate = 0.01
    )

    @Synchronized
    fun record(userId: String, campaignId: String) {
        cap.record(userId, campaignId)
    }

    @Synchronized
    fun isCapped(userId: String, campaignId: String): Boolean {
        val counts = cap.countAll(userId, campaignId)
        return (counts[1.hours] ?: 0) >= 3
            || (counts[24.hours] ?: 0) >= 10
    }

    @PreDestroy
    fun close() = cap.close()
}
```

```kotlin
@RestController
class AdController(private val freqCap: FrequencyCapService) {

    @PostMapping("/bid")
    fun bid(@RequestBody req: BidRequest): BidResponse {
        val eligible = req.campaigns.filter { !freqCap.isCapped(req.userId, it) }
        return BidResponse(eligible)
    }
}
```

- `@Service` singleton = native memory lifecycle managed by Spring
- `@Synchronized` = C core is not thread-safe
- `@PreDestroy` = free off-heap memory on shutdown

See [docs/USAGE.md](docs/USAGE.md) for rate limiter, dedup, sharding, Python examples.

## Build

```bash
# C core
cd core && mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && make && ctest

# Kotlin (builds C core automatically)
cd kotlin && ./gradlew test

# Python (requires C core installed)
cd python && pip install -e ".[dev]" && pytest
```

## Benchmark (Apple M-series, C core)

| Operation | Latency | Throughput |
|-----------|---------|------------|
| MurmurHash3_x86_128 | 89 ns | 11.2 M/s |
| TimingCMS `record` | 95 ns | 10.5 M/s |
| TimingCMS `count` | 112 ns | 8.9 M/s |
| SlidingBF `insert` | 58 ns | 17.1 M/s |
| SlidingBF `query` | 60 ns | 16.7 M/s |

Memory: **5.72 MB** for 5M user-campaign pairs (1h window, d=5, w=50000, 12 slots).

See [docs/BENCHMARK.md](docs/BENCHMARK.md) for full results and methodology.

## Project Structure

```
tinywindow/
├── core/               C core library
│   ├── include/        public API (nanofilter.h)
│   ├── src/            hash, timing_cms, sliding_bf, window, memory
│   └── test/           unit tests + throughput benchmark
├── kotlin/             Kotlin/JVM bindings (JNI)
│   ├── src/main/       TinyWindow, FrequencyCap, RateLimiter, Dedup
│   └── src/test/       Kotlin tests + benchmark
├── python/             Python bindings (cffi)
├── benchmark/          vs Redis, vs DataSketches, Flink integration
├── docs/               ALGORITHM.md, BENCHMARK.md, USAGE.md
└── examples/           Spring Boot ad server, Flink rate limiter
```

## Roadmap

- [x] **Phase 1** - C core: MurmurHash3, Timing CMS, Sliding BF, Window Manager
- [ ] **Phase 2** - Kotlin/JVM bindings: JNI bridge, Builder API, native packaging
- [ ] **Phase 3** - Benchmarks & showcase: vs Redis, vs DataSketches, Spring Boot example
- [ ] **Phase 4** - Ecosystem: Python bindings, Flink UDF example, CI/CD, Maven Central

## License

[Apache 2.0](LICENSE)
