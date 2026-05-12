package io.tinywindow

import org.junit.jupiter.api.Test
import org.junit.jupiter.api.assertThrows
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import kotlin.time.Duration.Companion.hours

class DedupTest {

    @Test
    fun `first occurrence is not duplicate`() {
        TinyWindow.dedup(
            expectedItems = 100_000,
            ttl = 1.hours,
            errorRate = 0.001
        ).use { dedup ->
            assertFalse(dedup.isDuplicate("click:abc123"))
        }
    }

    @Test
    fun `second occurrence is duplicate`() {
        TinyWindow.dedup(
            expectedItems = 100_000,
            ttl = 1.hours,
            errorRate = 0.001
        ).use { dedup ->
            assertFalse(dedup.isDuplicate("click:abc123"))
            assertTrue(dedup.isDuplicate("click:abc123"))
        }
    }

    @Test
    fun `different keys are independent`() {
        TinyWindow.dedup(
            expectedItems = 100_000,
            ttl = 1.hours,
            errorRate = 0.001
        ).use { dedup ->
            assertFalse(dedup.isDuplicate("click:abc"))
            assertFalse(dedup.isDuplicate("click:xyz"))
            assertTrue(dedup.isDuplicate("click:abc"))
        }
    }

    @Test
    fun `mightContain after record`() {
        TinyWindow.dedup(
            expectedItems = 100_000,
            ttl = 1.hours,
            errorRate = 0.001
        ).use { dedup ->
            assertFalse(dedup.mightContain("click:abc"))
            dedup.record("click:abc")
            assertTrue(dedup.mightContain("click:abc"))
        }
    }

    @Test
    fun `memory usage is positive`() {
        TinyWindow.dedup(
            expectedItems = 100_000,
            ttl = 1.hours,
            errorRate = 0.001
        ).use { dedup ->
            assertTrue(dedup.memoryUsage() > 0)
        }
    }

    @Test
    fun `use after close throws`() {
        val dedup = TinyWindow.dedup(
            expectedItems = 100_000,
            ttl = 1.hours,
            errorRate = 0.001
        )
        dedup.close()
        assertThrows<IllegalStateException> { dedup.isDuplicate("click:abc") }
    }
}
