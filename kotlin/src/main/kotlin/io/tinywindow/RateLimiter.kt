package io.tinywindow

import kotlin.time.Duration

class RateLimiter internal constructor(
    expectedKeys: Long,
    val windows: List<Duration>,
    errorRate: Double
) : AutoCloseable {

    private data class WindowHandle(val duration: Duration, val handle: Long)

    private val handles: List<WindowHandle>

    init {
        val params = CmsParams.compute(expectedKeys, errorRate)
        val allocated = mutableListOf<Long>()
        try {
            handles = windows.map { window ->
                val slotConfig = SlotConfig.forWindow(window.inWholeMilliseconds)
                val handle = Native.tcmsCreate(
                    params.depth, params.width,
                    slotConfig.numSlots, slotConfig.slotDurationMs
                )
                check(handle != 0L) { "Failed to allocate Timing CMS for window $window" }
                allocated += handle
                WindowHandle(window, handle)
            }
        } catch (e: Exception) {
            allocated.forEach(Native::tcmsDestroy)
            throw e
        }
    }

    private val guard = handles.map { it.handle }.let { ptrs ->
        NativeGuard("RateLimiter") { ptrs.forEach(Native::tcmsDestroy) }
    }

    fun record(key: String) = guard.withRef {
        val now = System.currentTimeMillis()
        for (wh in handles) { Native.tcmsRecord(wh.handle, key, now) }
    }

    fun count(key: String, window: Duration): Long = guard.withRef {
        val wh = handles.find { it.duration == window }
            ?: throw IllegalArgumentException("Window $window not configured")
        Native.tcmsCount(wh.handle, key, window.inWholeMilliseconds, System.currentTimeMillis())
    }

    fun isLimited(key: String, window: Duration, limit: Long): Boolean {
        return count(key, window) >= limit
    }

    override fun close() = guard.close()
}
