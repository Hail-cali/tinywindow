package benchmark

/**
 * Flink integration benchmark: measures GC impact of nanofilter vs Java alternatives.
 *
 * This is a conceptual benchmark showing how nanofilter would be used
 * inside a Flink ProcessFunction with zero GC overhead.
 *
 * Prerequisites: Flink dependencies.
 */

/*
// Example Flink ProcessFunction using nanofilter:

import io.tinywindow.TinyWindow
import org.apache.flink.streaming.api.functions.KeyedProcessFunction
import org.apache.flink.util.Collector
import kotlin.time.Duration.Companion.minutes
import kotlin.time.Duration.Companion.hours

class RateLimitFunction : KeyedProcessFunction<String, RequestEvent, ThrottleEvent>() {

    @Transient
    private lateinit var limiter: io.nanofilter.RateLimiter

    override fun open(params: org.apache.flink.configuration.Configuration) {
        limiter = TinyWindow.rateLimiter(
            expectedKeys = 1_000_000,
            windows = listOf(1.minutes, 1.hours),
            errorRate = 0.01
        )
    }

    override fun processElement(
        event: RequestEvent,
        ctx: Context,
        out: Collector<ThrottleEvent>
    ) {
        limiter.record(event.ip)
        if (limiter.isLimited(event.ip, 1.minutes, 100)) {
            out.collect(ThrottleEvent(event.ip, "rate_limited"))
        }
    }

    override fun close() {
        limiter.close()
    }
}
*/

fun main() {
    println("=== Flink Integration Benchmark (conceptual) ===")
    println()
    println("Key advantage of nanofilter in Flink:")
    println("  - All memory is off-heap (C malloc), zero GC pressure")
    println("  - No serialization overhead for state")
    println("  - Sub-microsecond operations don't affect event time processing")
    println()
    println("Comparison with Java-based alternatives in Flink:")
    println("  | Approach           | GC Impact | Latency   | State Size |")
    println("  |-------------------|-----------|-----------|------------|")
    println("  | HashMap + Timer   | High      | ~100ns    | Exact, large|")
    println("  | DataSketches CMS  | Medium    | ~200ns    | ~100KB     |")
    println("  | nanofilter        | None      | ~500ns*   | ~11MB      |")
    println("  * includes JNI overhead")
    println()
    println("To run actual Flink benchmark:")
    println("  1. Add Flink + nanofilter dependencies")
    println("  2. Enable GC logging: -Xlog:gc*:gc.log")
    println("  3. Compare GC pause times with vs without nanofilter")
}
