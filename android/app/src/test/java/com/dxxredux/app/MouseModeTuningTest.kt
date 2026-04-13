package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class MouseModeTuningTest {
    @Test
    fun tinyShortIntervalMoveStaysInsideGraceBand() {
        val history =
            updateMouseAccelerationHistory(
                previousDistancePx = 0f,
                previousGracePx = 0f,
                stepDistancePx = 2f,
                dtMs = 1L,
            )
        val multiplier =
            mouseAccelerationMultiplier(
                enabled = true,
                maxMultiplier = 3f,
                recentDistancePx = history.recentDistancePx,
                recentGracePx = history.recentGracePx,
                distancePx = 2f,
                viewportHeightPx = 1200f,
            )

        assertEquals(1f, multiplier, 0.0001f)
    }

    @Test
    fun repeatedFastSwipeBreaksOutOfGraceBand() {
        var recentDistancePx = 0f
        var recentGracePx = 0f

        repeat(4) {
            val history =
                updateMouseAccelerationHistory(
                    previousDistancePx = recentDistancePx,
                    previousGracePx = recentGracePx,
                    stepDistancePx = 15f,
                    dtMs = 16L,
                )
            recentDistancePx = history.recentDistancePx
            recentGracePx = history.recentGracePx
        }

        val multiplier =
            mouseAccelerationMultiplier(
                enabled = true,
                maxMultiplier = 3f,
                recentDistancePx = recentDistancePx,
                recentGracePx = recentGracePx,
                distancePx = 60f,
                viewportHeightPx = 1200f,
            )

        assertTrue(multiplier > 1.5f)
    }

    @Test
    fun longSlowDriftGetsOnlySmallFallbackBoost() {
        val multiplier =
            mouseAccelerationMultiplier(
                enabled = true,
                maxMultiplier = 3f,
                recentDistancePx = 4f,
                recentGracePx = 10f,
                distancePx = 600f,
                viewportHeightPx = 1200f,
            )

        assertTrue(multiplier > 1f)
        assertTrue(multiplier < 1.5f)
    }

    @Test
    fun disabledExponentialLeavesMultiplierFlat() {
        assertEquals(
            1f,
            mouseAccelerationMultiplier(
                enabled = false,
                maxMultiplier = 3f,
                recentDistancePx = 24f,
                recentGracePx = 0f,
                distancePx = 600f,
                viewportHeightPx = 1200f,
            ),
            0.0001f,
        )
    }
}