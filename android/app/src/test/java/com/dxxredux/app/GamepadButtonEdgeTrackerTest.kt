package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class GamepadButtonEdgeTrackerTest {
    @Test
    fun repeatedDownIsSuppressedUntilMatchingUp() {
        val tracker = GamepadButtonEdgeTracker()

        assertTrue(tracker.shouldDispatchDown(keyCode = 97, repeatCount = 0))
        assertFalse(tracker.shouldDispatchDown(keyCode = 97, repeatCount = 1))
        assertFalse(tracker.shouldDispatchDown(keyCode = 97, repeatCount = 0))
        assertTrue(tracker.shouldDispatchUp(97))
        assertTrue(tracker.shouldDispatchDown(keyCode = 97, repeatCount = 0))
    }

    @Test
    fun upWithoutDownIsIgnored() {
        val tracker = GamepadButtonEdgeTracker()

        assertFalse(tracker.shouldDispatchUp(98))
    }

    @Test
    fun clearReleasesLatchedButtons() {
        val tracker = GamepadButtonEdgeTracker()

        assertTrue(tracker.shouldDispatchDown(keyCode = 99, repeatCount = 0))
        tracker.clear()
        assertTrue(tracker.shouldDispatchDown(keyCode = 99, repeatCount = 0))
    }
}