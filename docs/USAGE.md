# Usage Guide

## Installation

### Kotlin/JVM (Gradle)

```kotlin
dependencies {
    implementation("io.nanofilter:nanofilter:0.1.0")
}
```

### Python

```bash
pip install nanofilter
```

### C

```bash
cd core
mkdir build && cd build
cmake ..
make
sudo make install
```

## Quick Start

### Frequency Capping (Kotlin)

```kotlin
import io.nanofilter.NanoFilter
import kotlin.time.Duration.Companion.hours
import kotlin.time.Duration.Companion.days

// Create a frequency cap with multiple windows
val cap = NanoFilter.frequencyCap(
    expectedPairs = 5_000_000,       // expected user x campaign pairs
    windows = listOf(1.hours, 24.hours, 7.days),
    errorRate = 0.01                  // 1% error tolerance
)

// Record an ad impression
cap.record("user:abc", "campaign:xyz")

// Check frequency before serving an ad
val counts = cap.countAll("user:abc", "campaign:xyz")
// → {1h: 2, 24h: 5, 7d: 12}

if (counts[1.hours]!! >= 3) {
    // Skip this ad — frequency cap reached
}

// Always close to free native memory
cap.close()
// Or use .use { } block for automatic cleanup
```

### Rate Limiting (Kotlin)

```kotlin
import io.nanofilter.NanoFilter
import kotlin.time.Duration.Companion.minutes
import kotlin.time.Duration.Companion.hours

NanoFilter.rateLimiter(
    expectedKeys = 1_000_000,
    windows = listOf(1.minutes, 1.hours),
    errorRate = 0.01
).use { limiter ->
    limiter.record(ipAddress)
    if (limiter.isLimited(ipAddress, 1.minutes, limit = 100)) {
        throttle()
    }
}
```

### Deduplication (Kotlin)

```kotlin
import io.nanofilter.NanoFilter
import kotlin.time.Duration.Companion.hours

NanoFilter.dedup(
    expectedItems = 10_000_000,
    ttl = 24.hours,
    errorRate = 0.001
).use { dedup ->
    if (dedup.isDuplicate("click:abc123")) {
        flagAsFraud()
    } else {
        processClick()
    }
}
```

### Python

```python
from nanofilter import TimingCMS

with TimingCMS(depth=5, width=50000, num_slots=12, slot_duration_ms=300000) as cms:
    cms.record("user:abc")
    count = cms.count("user:abc", window_ms=3600000)
    print(f"Count in last hour: {count}")
```

## Configuration Guide

### Choosing Parameters

**Error rate (`errorRate`):**
- 0.01 (1%): Good for most use cases
- 0.001 (0.1%): High accuracy, more memory
- 0.05 (5%): Low memory, rough estimates

**Expected items:**
- Frequency capping: number of unique (user, campaign) pairs
- Rate limiting: number of unique keys (IPs, user IDs)
- Deduplication: number of unique items within the TTL

### Memory Estimation

```
CMS memory ≈ depth × width × num_slots × 2 bytes

For errorRate=0.01:
  depth ≈ 5, width ≈ 272

Per window:
  1h  (12 slots): 5 × 272 × 12 × 2 = ~32KB
  24h (24 slots): 5 × 272 × 24 × 2 = ~65KB
  7d  (28 slots): 5 × 272 × 28 × 2 = ~76KB
```

Width scales with `expectedPairs` — the formula uses `ceil(e/errorRate)` but
is adjusted based on the expected collision rate.

## Spring Boot Integration

### Gradle

```kotlin
dependencies {
    implementation("io.tinywindow:tinywindow:0.1.0")
}
```

### Service Bean

```kotlin
@Service
class FrequencyCapService {

    private val cap = TinyWindow.frequencyCap(
        expectedPairs = 5_000_000,
        windows = listOf(1.hours, 24.hours, 7.days),
        errorRate = 0.01
    )

    fun record(userId: String, campaignId: String) {
        cap.record(userId, campaignId)
    }

    fun countAll(userId: String, campaignId: String): Map<Duration, Long> {
        return cap.countAll(userId, campaignId)
    }

    fun isCapped(userId: String, campaignId: String): Boolean {
        val counts = cap.countAll(userId, campaignId)
        return (counts[1.hours] ?: 0) >= 3
            || (counts[24.hours] ?: 0) >= 10
            || (counts[7.days] ?: 0) >= 30
    }

    @PreDestroy
    fun close() = cap.close()
}
```

Key points:
- `@Service` singleton = native 메모리 한 번만 할당
- `@PreDestroy` = shutdown 시 `close()` 호출 → 메모리 해제
- Thread-safe: 별도 동기화 불필요 (C 코어 내부에서 처리)

### Controller

```kotlin
@RestController
class AdController(private val freqCap: FrequencyCapService) {

    @PostMapping("/impression")
    fun record(@RequestParam userId: String, @RequestParam campaignId: String) {
        freqCap.record(userId, campaignId)
    }

    @PostMapping("/bid")
    fun bid(@RequestBody req: BidRequest): BidResponse {
        val eligible = req.campaigns.filter { !freqCap.isCapped(req.userId, it) }
        return BidResponse(eligible)
    }
}
```

## Thread Safety

All public API functions (`record`, `count`, `countAll`, `insert`, `mightContain`) are **thread-safe**.

Concurrency model (C core):
- Counter read/write: **atomic operations** (lock-free, concurrent access OK)
- Window slot rotation: **mutex** (rare event — once per `slot_duration`, e.g. every 5 minutes)
- Hash computation: outside any lock

No external synchronization needed in Kotlin/Java/Python.

## Resource Management

Always call `close()` or use `.use { }` to release native memory.
The JVM finalizer is **not** relied upon — explicit cleanup is required.
