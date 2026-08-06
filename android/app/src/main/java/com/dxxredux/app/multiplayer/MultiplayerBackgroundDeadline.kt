package com.dxxredux.app.multiplayer

internal class MultiplayerBackgroundDeadline(
    private val timeoutMs: Long,
) {
    private var generation = 0L
    private var backgroundSinceMs: Long? = null

    fun background(nowMs: Long): Pair<Long, Long>? {
        if (backgroundSinceMs != null) return null
        backgroundSinceMs = nowMs
        generation++
        return generation to nowMs + timeoutMs
    }

    fun foreground(): Long {
        backgroundSinceMs = null
        return ++generation
    }

    fun expire(
        token: Long,
        nowMs: Long,
    ): Boolean {
        val started = backgroundSinceMs ?: return false
        if (token != generation || nowMs - started < timeoutMs) return false
        backgroundSinceMs = null
        generation++
        return true
    }
}
