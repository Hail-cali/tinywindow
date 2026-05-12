package io.tinywindow

import kotlin.time.Duration

class Dedup internal constructor(
    expectedItems: Long,
    val ttl: Duration,
    errorRate: Double
) : AutoCloseable {

    private val handle: Long
    @Volatile private var closed = false

    init {
        val ttlMs = ttl.inWholeMilliseconds
        val slotConfig = SlotConfig.forWindow(ttlMs)
        handle = Native.sbfCreate(
            expectedItems, errorRate,
            slotConfig.numSlots, slotConfig.slotDurationMs
        )
        check(handle != 0L) { "Failed to allocate Sliding Bloom Filter" }
    }

    fun mightContain(key: String): Boolean {
        check(!closed) { "Dedup is closed" }
        return Native.sbfMightContain(handle, key, ttl.inWholeMilliseconds, System.currentTimeMillis())
    }

    fun record(key: String) {
        check(!closed) { "Dedup is closed" }
        Native.sbfInsert(handle, key, System.currentTimeMillis())
    }

    fun isDuplicate(key: String): Boolean {
        check(!closed) { "Dedup is closed" }
        val now = System.currentTimeMillis()
        val seen = Native.sbfMightContain(handle, key, ttl.inWholeMilliseconds, now)
        Native.sbfInsert(handle, key, now)
        return seen
    }

    fun memoryUsage(): Long = Native.sbfMemoryUsage(handle)

    override fun close() {
        if (closed) return
        closed = true
        Native.sbfDestroy(handle)
    }
}
