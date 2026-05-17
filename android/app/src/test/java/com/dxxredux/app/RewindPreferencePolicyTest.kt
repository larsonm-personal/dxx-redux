package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class RewindPreferencePolicyTest {
    @Test
    fun invalidRewindTargetFallsBackToDefault() {
        assertEquals(DEFAULT_REWIND_TARGET_SECONDS, sanitizeRewindTargetSeconds(7))
    }

    @Test
    fun validRewindTargetsRemainSelectable() {
        assertEquals(5, sanitizeRewindTargetSeconds(5))
        assertEquals(DEFAULT_REWIND_TARGET_SECONDS, sanitizeRewindTargetSeconds(DEFAULT_REWIND_TARGET_SECONDS))
        assertEquals(20, sanitizeRewindTargetSeconds(20))
    }
}