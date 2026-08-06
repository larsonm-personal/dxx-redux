package com.dxxredux.app

import java.util.concurrent.atomic.AtomicLong

/** Process-lifetime UI work counters sampled by the native introspection API. */
object DormancyDiagnostics {
    private val centralOverlayPolls = AtomicLong()
    private val independentOverlayPolls = AtomicLong()

    fun recordCentralOverlayPoll() {
        centralOverlayPolls.incrementAndGet()
    }

    fun recordIndependentOverlayPoll() {
        independentOverlayPolls.incrementAndGet()
    }

    fun snapshot(): LongArray =
        longArrayOf(
            centralOverlayPolls.get(),
            independentOverlayPolls.get(),
        )
}
