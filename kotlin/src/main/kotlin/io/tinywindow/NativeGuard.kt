package io.tinywindow

import java.lang.ref.Cleaner
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger

/**
 * Lock-free lifecycle guard for native (off-heap) resources.
 *
 * Concurrency model:
 *   - [withRef]: atomically increments a ref-count, executes [block], then decrements.
 *     Multiple callers run concurrently with zero blocking (no mutex, no ReadWriteLock).
 *   - [close]: sets a closed flag (CAS). No new [withRef] calls can succeed afterwards.
 *     The last in-flight operation to finish triggers destroy via CAS on [destroyed].
 *
 * GC safety net:
 *   If [close] is never called, [Cleaner] releases native memory when the guard
 *   becomes phantom-reachable. The destroy action is held in a static-like
 *   [CleanerAction] to avoid preventing GC of the owning object.
 *
 * Safe with coroutines — never blocks the calling thread.
 */
internal class NativeGuard(
    private val name: String,
    onDestroy: () -> Unit
) {
    private val state = CleanerAction(onDestroy)
    private val cleanable = cleaner.register(this, state)

    inline fun <T> withRef(block: () -> T): T {
        acquireRef()
        try {
            return block()
        } finally {
            releaseRef()
        }
    }

    fun close() {
        if (!state.closed.compareAndSet(false, true)) return
        if (state.refCount.get() == 0) cleanable.clean()
    }

    @PublishedApi
    internal fun acquireRef() {
        state.refCount.incrementAndGet()
        if (state.closed.get()) {
            releaseRef()
            throw IllegalStateException("$name is closed")
        }
    }

    @PublishedApi
    internal fun releaseRef() {
        if (state.refCount.decrementAndGet() == 0 && state.closed.get()) {
            cleanable.clean()
        }
    }

    /**
     * Destroy action held separately from [NativeGuard] so the Cleaner
     * reference does not prevent the guard from becoming phantom-reachable.
     */
    private class CleanerAction(
        private val onDestroy: () -> Unit
    ) : Runnable {
        val closed = AtomicBoolean(false)
        val refCount = AtomicInteger(0)
        private val destroyed = AtomicBoolean(false)

        override fun run() {
            if (destroyed.compareAndSet(false, true)) {
                onDestroy()
            }
        }
    }

    companion object {
        private val cleaner: Cleaner = Cleaner.create()
    }
}
