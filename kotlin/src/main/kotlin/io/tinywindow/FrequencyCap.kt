package io.tinywindow

import kotlin.time.Duration

class FrequencyCap internal constructor(
    expectedPairs: Long,
    val windows: List<Duration>,
    errorRate: Double
) : AutoCloseable {

    private data class WindowHandle(
        val duration: Duration,
        val handle: Long,
        val slotDurationMs: Long
    )

    private val handles: List<WindowHandle>
    @Volatile private var closed = false

    init {
        val cmsParams = CmsParams.compute(expectedPairs, errorRate)
        handles = windows.map { window ->
            val windowMs = window.inWholeMilliseconds
            val slotConfig = SlotConfig.forWindow(windowMs)
            val handle = Native.tcmsCreate(
                cmsParams.depth, cmsParams.width,
                slotConfig.numSlots, slotConfig.slotDurationMs
            )
            check(handle != 0L) { "Failed to allocate Timing CMS for window $window" }
            WindowHandle(window, handle, slotConfig.slotDurationMs)
        }
    }

    fun record(vararg keys: String) {
        check(!closed) { "FrequencyCap is closed" }
        val compositeKey = keys.joinToString(":")
        val now = System.currentTimeMillis()
        for (wh in handles) {
            Native.tcmsRecord(wh.handle, compositeKey, now)
        }
    }

    fun count(window: Duration, vararg keys: String): Long {
        check(!closed) { "FrequencyCap is closed" }
        val compositeKey = keys.joinToString(":")
        val now = System.currentTimeMillis()
        val wh = handles.find { it.duration == window }
            ?: throw IllegalArgumentException("Window $window not configured")
        return Native.tcmsCount(wh.handle, compositeKey, window.inWholeMilliseconds, now)
    }

    fun countAll(vararg keys: String): Map<Duration, Long> {
        check(!closed) { "FrequencyCap is closed" }
        val compositeKey = keys.joinToString(":")
        val now = System.currentTimeMillis()
        return handles.associate { wh ->
            wh.duration to Native.tcmsCount(
                wh.handle, compositeKey, wh.duration.inWholeMilliseconds, now
            )
        }
    }

    fun memoryUsage(): Long = handles.sumOf { Native.tcmsMemoryUsage(it.handle) }

    override fun close() {
        if (closed) return
        closed = true
        handles.forEach { Native.tcmsDestroy(it.handle) }
    }
}

internal data class CmsParams(val depth: Int, val width: Int) {
    companion object {
        fun compute(expectedItems: Long, errorRate: Double): CmsParams {
            val d = Math.ceil(Math.log(1.0 / errorRate)).toInt().coerceIn(3, 10)
            val wTheory = (Math.E / errorRate).toLong()
            val wPractical = expectedItems / 10
            val w = maxOf(wTheory, wPractical, 1000L).toInt()
            return CmsParams(depth = d, width = w)
        }
    }
}

internal data class SlotConfig(val numSlots: Int, val slotDurationMs: Long) {
    companion object {
        fun forWindow(windowMs: Long): SlotConfig {
            val targetSlots = when {
                windowMs <= 3_600_000L -> 12
                windowMs <= 86_400_000L -> 24
                else -> 28
            }
            val slotMs = windowMs / targetSlots
            return SlotConfig(numSlots = targetSlots, slotDurationMs = slotMs.coerceAtLeast(1000))
        }
    }
}
