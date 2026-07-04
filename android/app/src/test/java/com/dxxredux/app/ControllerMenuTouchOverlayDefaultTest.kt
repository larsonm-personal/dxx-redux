package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ControllerMenuTouchOverlayDefaultTest {
    @Test
    fun touchDevicesDefaultToAdvancedPreset() {
        assertEquals(DEFAULT_TOUCH_PRESET_NAME, defaultTouchPresetName(hasTouchscreen = true))
    }

    @Test
    fun touchlessDevicesDefaultToControllerMenusPreset() {
        assertEquals(CONTROLLER_MENU_TOUCH_PRESET_NAME, defaultTouchPresetName(hasTouchscreen = false))
    }

    @Test
    fun touchOverlayPreferenceDefaultsOnForTouchlessDevices() {
        assertTrue(defaultTouchOverlayEnabled(hasTouchscreen = false, hasController = true))
        assertTrue(defaultTouchOverlayEnabled(hasTouchscreen = false, hasController = false))
    }

    @Test
    fun touchOverlayPreferenceKeepsPhoneControllerDefaultOff() {
        assertFalse(defaultTouchOverlayEnabled(hasTouchscreen = true, hasController = true))
        assertTrue(defaultTouchOverlayEnabled(hasTouchscreen = true, hasController = false))
    }

    @Test
    fun controllerMenuOnlyControlsAreHiddenUntilTheirMenuIsOpen() {
        assertFalse(
            controllerMenuControlVisible(
                controllerMenuOnlyLayout = true,
                menuOpen = false,
            ),
        )
        assertTrue(
            controllerMenuControlVisible(
                controllerMenuOnlyLayout = true,
                menuOpen = true,
            ),
        )
        assertTrue(
            controllerMenuControlVisible(
                controllerMenuOnlyLayout = false,
                menuOpen = false,
            ),
        )
    }

    @Test
    fun adminTrayTabVisibilityMatchesDrawAndTouchPolicy() {
        assertFalse(adminTrayTabVisible(gamepadOnlyMode = true, hasSettingsDiagnostic = false))
        assertFalse(adminTrayTabVisible(gamepadOnlyMode = false, hasSettingsDiagnostic = true))
        assertTrue(adminTrayTabVisible(gamepadOnlyMode = false, hasSettingsDiagnostic = false))
    }

    @Test
    fun controllerMenuPresetIsRecognizedByLayoutName() {
        assertTrue(isControllerMenuOnlyTouchLayout(TouchLayout(name = CONTROLLER_MENU_TOUCH_PRESET_NAME)))
        assertFalse(isControllerMenuOnlyTouchLayout(TouchLayout(name = DEFAULT_TOUCH_PRESET_NAME)))
    }
}
