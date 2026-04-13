package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class OverlayVisibilityPolicyTest {
    @Test
    fun settingsTrayKeepsOverlayVisibleWhilePauseWindowIsFront() {
        assertTrue(
            shouldShowTouchOverlay(
                inGame = false,
                overlayEnabled = true,
                playerDead = false,
                endlevel = false,
                automap = false,
                settingsTrayVisible = true,
            ),
        )
    }

    @Test
    fun pausedWithoutTrayStillHidesGameplayOverlay() {
        assertFalse(
            shouldShowTouchOverlay(
                inGame = false,
                overlayEnabled = true,
                playerDead = false,
                endlevel = false,
                automap = false,
                settingsTrayVisible = false,
            ),
        )
    }
}