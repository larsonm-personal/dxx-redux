package com.dxxredux.app

import org.junit.Assert.*
import org.junit.Test

class NavigationDirectionRepeatTest {
    @Test
    fun releaseAndReversalRestartDelayAndLatePollingDoesNotBurst() {
        val repeat = NavigationDirectionRepeat(NavigationRepeatTiming(500, 125))
        assertTrue(repeat.update(1, 0))
        assertFalse(repeat.update(1, 499))
        assertTrue(repeat.update(1, 500))
        assertFalse(repeat.update(1, 624))
        assertTrue(repeat.update(1, 1000))
        assertFalse(repeat.update(1, 1000))
        assertTrue(repeat.update(-1, 1001))
        assertFalse(repeat.update(-1, 1500))
        assertFalse(repeat.update(0, 1501))
        assertTrue(repeat.update(-1, 1502))
        assertFalse(repeat.update(-1, 2001))
    }

    @Test
    fun navigationCanRepeatWhileTheSameHoldStillOpensItsBinding() {
        val repeat = NavigationDirectionRepeat(NavigationRepeatTiming(500, 125))
        val detector = ControllerLongPressDetector()
        var moves = 0
        var opened: ControllerLongPressDetector.Trigger? = null
        for (now in 0L..2000L step 50L) {
            val trigger = detector.update(now, FloatArray(6), pressedButtons = listOf("D-Down"), gated = false)
            if (trigger != null) {
                opened = trigger
                repeat.update(0, now)
            } else if (repeat.update(1, now)) {
                moves++
            }
        }
        assertTrue(moves > 2)
        assertEquals(ControllerLongPressDetector.Trigger.Button("D-Down"), opened)
    }
}
