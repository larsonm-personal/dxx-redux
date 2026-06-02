package com.dxxredux.app

import androidx.compose.ui.focus.FocusRequester
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

    @Test
    fun phoneTouchModeDoesNotShowControllerHighlight() {
        assertFalse(
            shouldShowControllerFocusHighlight(
                hasTouchscreen = true,
                controllerNavigationActive = false,
            ),
        )
    }

    @Test
    fun phoneControllerNavigationShowsControllerHighlight() {
        assertTrue(
            shouldShowControllerFocusHighlight(
                hasTouchscreen = true,
                controllerNavigationActive = true,
            ),
        )
    }

    @Test
    fun touchlessDeviceShowsControllerHighlightImmediately() {
        assertTrue(
            shouldShowControllerFocusHighlight(
                hasTouchscreen = false,
                controllerNavigationActive = false,
            ),
        )
    }

    @Test
    fun unattachedFocusRequesterCanBeRequestedSafely() {
        FocusRequester().requestFocusSafely()
    }
}
