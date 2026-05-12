package io.tinywindow

import org.junit.jupiter.api.Test
import kotlin.time.Duration.Companion.hours
import kotlin.time.Duration.Companion.days
import kotlin.time.measureTime

class BenchmarkTest {

    @Test
    fun `frequency cap throughput`() {
        TinyWindow.frequencyCap(
            expectedPairs = 1_000_000,
            windows = listOf(1.hours, 24.hours, 7.days),
            errorRate = 0.01
        ).use { cap ->
            val iterations = 100_000
            val recordTime = measureTime {
                repeat(iterations) { i -> cap.record("user:${i % 10000}", "campaign:${i % 1000}") }
            }
            val countTime = measureTime {
                repeat(iterations) { i -> cap.countAll("user:${i % 10000}", "campaign:${i % 1000}") }
            }
            println("FrequencyCap benchmark ($iterations iterations, 3 windows):")
            println("  record: ${recordTime.inWholeMilliseconds}ms total, ${recordTime.inWholeNanoseconds / iterations}ns/op")
            println("  countAll: ${countTime.inWholeMilliseconds}ms total, ${countTime.inWholeNanoseconds / iterations}ns/op")
            println("  memory: ${cap.memoryUsage() / 1024}KB")
        }
    }
}
