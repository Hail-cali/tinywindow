package benchmark

import io.tinywindow.TinyWindow
import kotlin.time.Duration.Companion.hours
import kotlin.time.Duration.Companion.days
import kotlin.time.measureTime

/**
 * Benchmark: nanofilter vs Redis for frequency capping.
 *
 * Prerequisites:
 * - Redis running on localhost:6379
 * - Jedis dependency added
 *
 * Usage: Run as a standalone Kotlin script or integrate with JMH.
 */
fun main() {
    val iterations = 100_000

    // ── nanofilter ──
    println("=== nanofilter FrequencyCap ===")
    TinyWindow.frequencyCap(
        expectedPairs = 1_000_000,
        windows = listOf(1.hours, 24.hours, 7.days),
        errorRate = 0.01
    ).use { cap ->
        val recordTime = measureTime {
            repeat(iterations) { i ->
                cap.record("user:${i % 10000}", "campaign:${i % 1000}")
            }
        }
        println("record: ${recordTime.inWholeMilliseconds}ms " +
                "(${recordTime.inWholeNanoseconds / iterations}ns/op)")

        val countTime = measureTime {
            repeat(iterations) { i ->
                cap.countAll("user:${i % 10000}", "campaign:${i % 1000}")
            }
        }
        println("countAll (3 windows): ${countTime.inWholeMilliseconds}ms " +
                "(${countTime.inWholeNanoseconds / iterations}ns/op)")
        println("memory: ${cap.memoryUsage() / 1024}KB")
    }

    // ── Redis (placeholder) ──
    println("\n=== Redis (placeholder — uncomment with Jedis dependency) ===")
    println("// Typical Redis GET latency: 500-2000us per call")
    println("// For 3 windows: 1500-6000us per countAll equivalent")
    println("// nanofilter target: <20us for countAll")

    /*
    // Uncomment with Jedis dependency:
    val jedis = redis.clients.jedis.Jedis("localhost", 6379)

    val redisRecordTime = measureTime {
        repeat(iterations) { i ->
            val key = "freq:user:${i % 10000}:campaign:${i % 1000}"
            jedis.incr("$key:1h")
            jedis.expire("$key:1h", 3600)
            jedis.incr("$key:24h")
            jedis.expire("$key:24h", 86400)
        }
    }
    println("Redis record: ${redisRecordTime.inWholeMilliseconds}ms")

    val redisCountTime = measureTime {
        repeat(iterations) { i ->
            val key = "freq:user:${i % 10000}:campaign:${i % 1000}"
            jedis.get("$key:1h")
            jedis.get("$key:24h")
            jedis.get("$key:7d")
        }
    }
    println("Redis countAll: ${redisCountTime.inWholeMilliseconds}ms")
    jedis.close()
    */
}
