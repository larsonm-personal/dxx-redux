package com.dxxredux.app

import java.util.concurrent.atomic.AtomicBoolean

/** One-shot marker distinguishing a read-only preview return from other launcher resumes. */
internal object LevelPreviewReturnRefreshGate {
    private val previewLaunchPending = AtomicBoolean(false)

    fun markLaunch() {
        previewLaunchPending.set(true)
    }

    fun consumeReturn(): Boolean = previewLaunchPending.compareAndSet(true, false)

    internal fun resetForTest() {
        previewLaunchPending.set(false)
    }
}
