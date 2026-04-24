package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class GamepadButtonDispatchPolicyTest {
    @Test
    fun gameplayDownStillRequiresEdgeTracker() {
        assertFalse(shouldDispatchGamepadButtonDown(isInGame = true, repeatCount = 0, edgeDispatchAllowed = false))
        assertTrue(shouldDispatchGamepadButtonDown(isInGame = true, repeatCount = 0, edgeDispatchAllowed = true))
    }

    @Test
    fun menuDownAllowsFreshTapWithoutGameplayLatch() {
        assertTrue(shouldDispatchGamepadButtonDown(isInGame = false, repeatCount = 0, edgeDispatchAllowed = false))
        assertFalse(shouldDispatchGamepadButtonDown(isInGame = false, repeatCount = 1, edgeDispatchAllowed = true))
    }

    @Test
    fun gameplayUpStillRequiresEdgeTracker() {
        assertFalse(shouldDispatchGamepadButtonUp(isInGame = true, edgeDispatchAllowed = false))
        assertTrue(shouldDispatchGamepadButtonUp(isInGame = true, edgeDispatchAllowed = true))
    }

    @Test
    fun menuUpAlwaysReleases() {
        assertTrue(shouldDispatchGamepadButtonUp(isInGame = false, edgeDispatchAllowed = false))
        assertTrue(shouldDispatchGamepadButtonUp(isInGame = false, edgeDispatchAllowed = true))
    }
}