# Flink Rate Limiter Example

Demonstrates nanofilter as a rate limiter inside a Flink ProcessFunction.

## Setup (Phase 4)

```bash
./gradlew run
```

## Key Points

- One `RateLimiter` instance per Flink subtask (no sharing)
- Zero GC overhead — all state is off-heap
- No external state store dependency (Redis, RocksDB)
