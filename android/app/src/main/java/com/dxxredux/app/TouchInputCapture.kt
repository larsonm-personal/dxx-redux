package com.dxxredux.app

import java.util.concurrent.Executors
import java.util.concurrent.ScheduledExecutorService
import java.util.concurrent.TimeUnit

/** One bounded capture per game Activity, written through the normal exportable debug log. */
class TouchInputCapture(
    private val nowMs: () -> Long,
    private val writeBatch: (String) -> Unit,
    private val setNativeEnabled: (Boolean) -> Unit,
    private val durationMs: Long = 30_000L,
    private val maxRecords: Int = 120_000,
    private val maxBatchChars: Int = 1024 * 1024,
) {
    private val lock = Any()
    private val flushLock = Any()
    private val pending = StringBuilder()
    private var worker: ScheduledExecutorService? = null

    @Volatile private var started = false
    private var startMs = 0L
    private var sequence = 0L
    private var accepted = 0
    private var dropped = 0
    private var batchDropped = 0
    private var firstDropMs = 0L
    private var lastDropMs = 0L

    @Volatile var active = false
        private set

    fun start(metadata: () -> String) {
        if (started) return
        synchronized(flushLock) {
            synchronized(lock) {
                if (started) return
                started = true
                startMs = nowMs()
                append("start version=1 duration_ms=$durationMs clock=uptime_ms ${metadata()}")
                active = true
            }
            setNativeEnabled(true)
            worker =
                Executors
                    .newSingleThreadScheduledExecutor { task ->
                        Thread(task, "touch-capture-writer").apply { isDaemon = true }
                    }.also { it.scheduleAtFixedRate({ flush() }, 1, 1, TimeUnit.SECONDS) }
        }
    }

    fun record(message: () -> String) {
        if (!active) return
        val timestamp = nowMs()
        val text = message()
        synchronized(lock) {
            if (!active || timestamp - startMs >= durationMs) return
            if (accepted >= maxRecords || pending.length + text.length + 128 > maxBatchChars) {
                dropped++
                if (batchDropped++ == 0) firstDropMs = timestamp
                lastDropMs = timestamp
                return
            }
            append(text, timestamp)
            accepted++
        }
    }

    /** Called by the writer; also permits deterministic lifecycle tests without sleeping. */
    internal fun flush() {
        synchronized(flushLock) {
            val reason =
                synchronized(lock) {
                    when {
                        !active -> return
                        nowMs() - startMs >= durationMs -> "duration"
                        accepted >= maxRecords -> "record_limit"
                        else -> null
                    }
                }
            if (reason != null) {
                finish(reason)
                return
            }
            val batch = synchronized(lock) { takePending() }
            if (batch.isNotEmpty()) writeBatch(batch)
        }
    }

    /** Synchronous final flush makes the tail exportable before the game process exits. */
    fun finish(reason: String) {
        synchronized(flushLock) {
            val batch =
                synchronized(lock) {
                    if (!active) return
                    active = false
                    appendGap()
                    append("end reason=$reason records=$accepted dropped=$dropped elapsed_ms=${nowMs() - startMs}")
                    takePending()
                }
            setNativeEnabled(false)
            worker?.shutdown()
            worker = null
            writeBatch(batch)
        }
    }

    private fun append(
        message: String,
        timestamp: Long = nowMs(),
    ) {
        pending
            .append("[touch-capture] capture=$startMs seq=${sequence++} t_ms=$timestamp ")
            .append(message)
            .append('\n')
    }

    private fun takePending(): String {
        appendGap()
        val batch = pending.toString()
        pending.setLength(0)
        return batch
    }

    private fun appendGap() {
        if (batchDropped == 0) return
        append("gap dropped=$batchDropped first_t_ms=$firstDropMs last_t_ms=$lastDropMs")
        batchDropped = 0
    }
}
