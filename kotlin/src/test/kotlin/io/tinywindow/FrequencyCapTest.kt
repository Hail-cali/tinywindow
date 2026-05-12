package io.tinywindow

import org.junit.jupiter.api.Test
import org.junit.jupiter.api.assertThrows
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import kotlin.time.Duration.Companion.hours
import kotlin.time.Duration.Companion.days

class FrequencyCapTest {

    @Test
    fun `create and close without error`() {
        val cap = TinyWindow.frequencyCap(
            expectedPairs = 100_000,
            windows = listOf(1.hours, 24.hours, 7.days),
            errorRate = 0.01
        )
        assertTrue(cap.memoryUsage() > 0)
        cap.close()
    }

    @Test
    fun `record and count basic`() {
        TinyWindow.frequencyCap(
            expectedPairs = 100_000,
            windows = listOf(1.hours),
            errorRate = 0.01
        ).use { cap ->
            repeat(5) { cap.record("user:abc", "campaign:xyz") }
            val count = cap.count(1.hours, "user:abc", "campaign:xyz")
            assertTrue(count >= 5, "CMS count should be >= true count, got $count")
        }
    }

    @Test
    fun `countAll returns all windows`() {
        TinyWindow.frequencyCap(
            expectedPairs = 100_000,
            windows = listOf(1.hours, 24.hours),
            errorRate = 0.01
        ).use { cap ->
            repeat(3) { cap.record("user:abc", "campaign:xyz") }
            val counts = cap.countAll("user:abc", "campaign:xyz")
            assertEquals(2, counts.size)
            assertTrue(counts[1.hours]!! >= 3)
            assertTrue(counts[24.hours]!! >= 3)
        }
    }

    @Test
    fun `unknown key returns zero`() {
        TinyWindow.frequencyCap(
            expectedPairs = 100_000,
            windows = listOf(1.hours),
            errorRate = 0.01
        ).use { cap ->
            cap.record("user:abc", "campaign:xyz")
            assertEquals(0L, cap.count(1.hours, "user:other", "campaign:other"))
        }
    }

    @Test
    fun `query unconfigured window throws`() {
        TinyWindow.frequencyCap(
            expectedPairs = 100_000,
            windows = listOf(1.hours),
            errorRate = 0.01
        ).use { cap ->
            assertThrows<IllegalArgumentException> { cap.count(24.hours, "user:abc") }
        }
    }

    @Test
    fun `use after close throws`() {
        val cap = TinyWindow.frequencyCap(
            expectedPairs = 100_000,
            windows = listOf(1.hours),
            errorRate = 0.01
        )
        cap.close()
        assertThrows<IllegalStateException> { cap.record("user:abc") }
    }
}
