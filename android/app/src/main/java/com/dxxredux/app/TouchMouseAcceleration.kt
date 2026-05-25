package com.dxxredux.app

import kotlin.math.pow

private const val MOUSE_HISTORY_WINDOW_MS = 16f
private const val MOUSE_HISTORY_DECAY_PER_WINDOW = 0.75f
private const val MOUSE_NO_ACCEL_DISTANCE_PER_WINDOW = 8f
private const val MOUSE_EXCESS_DISTANCE_REFERENCE = 18f
private const val MOUSE_DISTANCE_FALLBACK_START = 24f
private const val MOUSE_DISTANCE_FALLBACK_WEIGHT = 0.2f

internal data class MouseAccelerationHistory(
    val recentDistancePx: Float,
    val recentGracePx: Float,
)

internal fun updateMouseAccelerationHistory(
    previousDistancePx: Float,
    previousGracePx: Float,
    stepDistancePx: Float,
    dtMs: Long,
): MouseAccelerationHistory {
    val windowCount = (dtMs.coerceAtLeast(1L).toFloat() / MOUSE_HISTORY_WINDOW_MS).coerceIn(1f, 6f)
    val decay = MOUSE_HISTORY_DECAY_PER_WINDOW.pow(windowCount)
    return MouseAccelerationHistory(
        recentDistancePx = previousDistancePx * decay + stepDistancePx,
        recentGracePx = previousGracePx * decay + MOUSE_NO_ACCEL_DISTANCE_PER_WINDOW * windowCount,
    )
}

internal fun mouseAccelerationMultiplier(
    enabled: Boolean,
    maxMultiplier: Float,
    recentDistancePx: Float,
    recentGracePx: Float,
    distancePx: Float,
    viewportHeightPx: Float,
): Float {
    if (!enabled) return 1f
    val clampedMax = maxMultiplier.coerceAtLeast(1f)
    val historyRatio =
        ((recentDistancePx - recentGracePx).coerceAtLeast(0f) / MOUSE_EXCESS_DISTANCE_REFERENCE).coerceIn(0f, 1f)
    val distanceRatio =
        if (viewportHeightPx > 1f) {
            ((distancePx - MOUSE_DISTANCE_FALLBACK_START).coerceAtLeast(0f) / (viewportHeightPx * 0.5f)).coerceIn(
                0f,
                1f,
            ) * MOUSE_DISTANCE_FALLBACK_WEIGHT
        } else {
            0f
        }
    val ratio = maxOf(historyRatio, distanceRatio)
    return 1f + (clampedMax - 1f) * ratio
}
