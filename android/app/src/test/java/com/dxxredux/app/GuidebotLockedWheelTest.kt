package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class GuidebotLockedWheelTest {
    @Test
    fun lockedGuideSpawnRingUsesOuterThirdOfWheel() {
        val radius = 90f

        assertFalse(lockedGuideSpawnRingSelected(59.9f, radius))
        assertTrue(lockedGuideSpawnRingSelected(60f, radius))
        assertTrue(lockedGuideSpawnRingSelected(90f, radius))
        assertTrue(lockedGuideSpawnRingSelected(108f, radius))
        assertFalse(lockedGuideSpawnRingSelected(108.1f, radius))
    }

    @Test
    fun lockedGuideSpawnRingRejectsInvalidRadius() {
        assertFalse(lockedGuideSpawnRingSelected(1f, 0f))
        assertFalse(lockedGuideSpawnRingSelected(1f, -1f))
    }
}
