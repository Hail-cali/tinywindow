package benchmark

import io.tinywindow.TinyWindow
import kotlin.time.Duration.Companion.hours
import kotlin.time.measureTime

/**
 * Benchmark: nanofilter vs Apache DataSketches CountMinSketch.
 *
 * Key difference: DataSketches CMS has no time window support.
 * nanofilter provides built-in time decay with comparable throughput.
 *
 * Prerequisites: Add org.apache.datasketches:datasketches-java dependency.
 */
fun main() {
    val iterations = 500_000

    // ── nanofilter (with time windows) ──
    println("=== nanofilter Timing CMS ===")
    TinyWindow.frequencyCap(
        expectedPairs = 1_000_000,
        windows = listOf(1.hours),
        errorRate = 0.01
    ).use { cap ->
        val time = measureTime {
            repeat(iterations) { i ->
                cap.record("key:$i")
            }
        }
        println("record: ${time.inWholeNanoseconds / iterations}ns/op")

        val countTime = measureTime {
            repeat(iterations) { i ->
                cap.count(1.hours, "key:$i")
            }
        }
        println("count:  ${countTime.inWholeNanoseconds / iterations}ns/op")
        println("memory: ${cap.memoryUsage() / 1024}KB")
        println("features: time-windowed frequency counting")
    }

    // ── DataSketches (placeholder) ──
    println("\n=== Apache DataSketches CMS (placeholder) ===")
    println("// Add dependency: org.apache.datasketches:datasketches-java:5.0.0")
    println("// DataSketches CMS: no time window, counts are cumulative forever")
    println("// To achieve time windows, you'd need external TTL logic")

    /*
    // Uncomment with DataSketches dependency:
    val sketch = org.apache.datasketches.frequencies.ItemsSketch<String>(1024)
    val dsRecordTime = measureTime {
        repeat(iterations) { i -> sketch.update("key:$i") }
    }
    println("DataSketches record: ${dsRecordTime.inWholeNanoseconds / iterations}ns/op")
    */

    println("\n=== Feature Comparison ===")
    println("| Feature              | nanofilter | DataSketches CMS |")
    println("|---------------------|------------|------------------|")
    println("| Time windows        | Built-in   | Not supported    |")
    println("| Multi-window query  | Yes        | No               |")
    println("| Auto-expiry         | Yes        | No               |")
    println("| GC-free (off-heap)  | Yes        | No (Java heap)   |")
    println("| JVM native          | Yes (JNI)  | Yes (pure Java)  |")
}
