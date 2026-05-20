package io.tinywindow

import kotlin.time.Duration

class Dedup internal constructor(
    expectedItems: Long,
    val ttl: Duration,
    errorRate: Double
) : AutoCloseable {

    private val handle: Long

    init {
        val ttlMs = ttl.inWholeMilliseconds
        val slotConfig = SlotConfig.forWindow(ttlMs)
        handle = Native.sbfCreate(
            expectedItems, errorRate,
            slotConfig.numSlots, slotConfig.slotDurationMs
        )
        check(handle != 0L) { "Failed to allocate Sliding Bloom Filter" }
    }

    private val guard = handle.let { ptr ->
        NativeGuard("Dedup") { Native.sbfDestroy(ptr) }
    }

    fun mightContain(vararg keys: String): Boolean = guard.withRef {
        val key = compositeKey(keys)
        Native.sbfMightContain(handle, key, ttl.inWholeMilliseconds, System.currentTimeMillis())
    }

    fun record(vararg keys: String) = guard.withRef {
        val key = compositeKey(keys)
        Native.sbfInsert(handle, key, System.currentTimeMillis())
    }

    fun isDuplicate(vararg keys: String): Boolean = guard.withRef {
        val key = compositeKey(keys)
        val now = System.currentTimeMillis()
        val seen = Native.sbfMightContain(handle, key, ttl.inWholeMilliseconds, now)
        Native.sbfInsert(handle, key, now)
        seen
    }

    fun memoryUsage(): Long = guard.withRef {
        Native.sbfMemoryUsage(handle)
    }

    override fun close() = guard.close()

    private fun compositeKey(keys: Array<out String>): String {
        return keys.joinToString("") { "${it.length}:$it" }
    }
}
