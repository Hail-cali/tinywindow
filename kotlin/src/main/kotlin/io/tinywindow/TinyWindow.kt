package io.tinywindow

import kotlin.time.Duration

object TinyWindow {

    fun frequencyCap(
        expectedPairs: Long,
        windows: List<Duration>,
        errorRate: Double = 0.01
    ): FrequencyCap {
        return FrequencyCap(expectedPairs, windows, errorRate)
    }

    fun rateLimiter(
        expectedKeys: Long,
        windows: List<Duration>,
        errorRate: Double = 0.01
    ): RateLimiter {
        return RateLimiter(expectedKeys, windows, errorRate)
    }

    fun dedup(
        expectedItems: Long,
        ttl: Duration,
        errorRate: Double = 0.001
    ): Dedup {
        return Dedup(expectedItems, ttl, errorRate)
    }
}
