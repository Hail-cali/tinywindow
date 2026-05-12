package io.tinywindow

import kotlin.time.Duration

class RateLimiter internal constructor(
    expectedKeys: Long,
    val windows: List<Duration>,
    errorRate: Double
) : AutoCloseable {

    private data class WindowHandle(val duration: Duration, val handle: Long)

    private val handles: List<WindowHandle>
    @Volatile private var closed = false

    init {
        val params = CmsParams.compute(expectedKeys, errorRate)
        handles = windows.map { window ->
            val slotConfig = SlotConfig.forWindow(window.inWholeMilliseconds)
            val handle = Native.tcmsCreate(
                params.depth, params.width,
                slotConfig.numSlots, slotConfig.slotDurationMs
            )
            check(handle != 0L) { "Failed to allocate Timing CMS for window $window" }
            WindowHandle(window, handle)
        }
    }

    fun record(key: String) {
        check(!closed) { "RateLimiter is closed" }
        val now = System.currentTimeMillis()
        for (wh in handles) { Native.tcmsRecord(wh.handle, key, now) }
    }

    fun count(key: String, window: Duration): Long {
        check(!closed) { "RateLimiter is closed" }
        val wh = handles.find { it.duration == window }
            ?: throw IllegalArgumentException("Window $window not configured")
        return Native.tcmsCount(wh.handle, key, window.inWholeMilliseconds, System.currentTimeMillis())
    }

    fun isLimited(key: String, window: Duration, limit: Long): Boolean {
        return count(key, window) >= limit
    }

    override fun close() {
        if (closed) return
        closed = true
        handles.forEach { Native.tcmsDestroy(it.handle) }
    }
}
