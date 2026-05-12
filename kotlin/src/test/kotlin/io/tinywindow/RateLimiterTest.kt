package io.tinywindow

import org.junit.jupiter.api.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import kotlin.time.Duration.Companion.minutes
import kotlin.time.Duration.Companion.hours

class RateLimiterTest {

    @Test
    fun `basic rate limiting`() {
        TinyWindow.rateLimiter(
            expectedKeys = 100_000,
            windows = listOf(1.minutes, 1.hours),
            errorRate = 0.01
        ).use { limiter ->
            val ip = "192.168.1.100"
            repeat(50) { limiter.record(ip) }
            assertFalse(limiter.isLimited(ip, 1.minutes, 100))
            repeat(60) { limiter.record(ip) }
            assertTrue(limiter.isLimited(ip, 1.minutes, 100))
        }
    }

    @Test
    fun `different keys are independent`() {
        TinyWindow.rateLimiter(
            expectedKeys = 100_000,
            windows = listOf(1.minutes),
            errorRate = 0.01
        ).use { limiter ->
            repeat(10) { limiter.record("ip_a") }
            repeat(5) { limiter.record("ip_b") }
            assertTrue(limiter.count("ip_a", 1.minutes) >= 10)
            assertTrue(limiter.count("ip_b", 1.minutes) >= 5)
            assertEquals(0L, limiter.count("ip_c", 1.minutes))
        }
    }
}
