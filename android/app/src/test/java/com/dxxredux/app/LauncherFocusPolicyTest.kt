package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LauncherFocusPolicyTest {
    @Test
    fun phoneTouchModeDoesNotSeedControllerFocus() {
        assertFalse(
            shouldSeedLauncherControllerFocus(
                isAndroidTv = false,
                controllerNavigationActive = false,
            ),
        )
    }

    @Test
    fun phoneControllerNavigationSeedsFocus() {
        assertTrue(
            shouldSeedLauncherControllerFocus(
                isAndroidTv = false,
                controllerNavigationActive = true,
            ),
        )
    }

    @Test
    fun androidTvAlwaysSeedsFocus() {
        assertTrue(
            shouldSeedLauncherControllerFocus(
                isAndroidTv = true,
                controllerNavigationActive = false,
            ),
        )
    }
}
