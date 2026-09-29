package com.dxxredux.app

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.async
import kotlinx.coroutines.runInterruptible
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withTimeout

/** A deadline releases the UI, but file ownership lasts until the actual writer retires */
internal class LaunchPreparationWork {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private val writer = Mutex()

    suspend fun <T> run(
        timeoutMs: Long,
        prepare: () -> T,
    ): T {
        val work =
            scope.async {
                writer.withLock { runInterruptible { prepare() } }
            }
        return try {
            withTimeout(timeoutMs) { work.await() }
        } finally {
            // Interrupt blocking Java I/O; a non-cooperative native call retains the lock until it returns
            work.cancel()
        }
    }
}

internal val launcherFilePreparation = LaunchPreparationWork()
internal val launcherLaunchOwner =
    java.util.concurrent.atomic
        .AtomicReference<Any?>()
