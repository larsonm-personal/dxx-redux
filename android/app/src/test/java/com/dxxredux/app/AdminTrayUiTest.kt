package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AdminTrayUiTest {
    @Test
    fun touchModeKeepsAutomapOutWhenTouchButtonExists() {
        assertFalse(adminTrayVisibleActions(gamepadOnlyMode = false, hasTouchAutomapButton = true).contains(TouchOverlayView.ADMIN_AUTOMAP))
    }

    @Test
    fun touchModeKeepsAutomapOutWhenTouchButtonIsMissing() {
        val actions = adminTrayVisibleActions(gamepadOnlyMode = false, hasTouchAutomapButton = false)

        assertEquals(8, actions.size)
        assertFalse(actions.contains(TouchOverlayView.ADMIN_AUTOMAP))
        assertTrue(actions.contains(TouchOverlayView.ADMIN_BRIGHTNESS))
    }

    @Test
    fun singlePlayerGamepadOnlyModeHidesNetworkAndHeadlightActions() {
        val actions = adminTrayVisibleActions(gamepadOnlyMode = true, hasTouchAutomapButton = true)

        assertFalse(actions.contains(TouchOverlayView.ADMIN_NET_STATS))
        assertFalse(actions.contains(TouchOverlayView.ADMIN_NET_EVENTS))
        assertFalse(actions.contains(TouchOverlayView.ADMIN_HEADLIGHT))
        assertFalse(actions.contains(TouchOverlayView.ADMIN_WARP))
        assertFalse(actions.contains(TouchOverlayView.ADMIN_ACCEPT_JOIN))
        assertFalse(actions.contains(TouchOverlayView.ADMIN_AUTOMAP))
        assertTrue(actions.contains(TouchOverlayView.ADMIN_BRIGHTNESS))
        assertTrue(actions.contains(TouchOverlayView.ADMIN_MUSIC))
    }

    @Test
    fun multiplayerGamepadOnlyModeIncludesNetworkControllerActions() {
        val actions =
            adminTrayVisibleActions(
                gamepadOnlyMode = true,
                hasTouchAutomapButton = true,
                isMultiplayerGame = true,
            )

        assertEquals(
            listOf(
                TouchOverlayView.ADMIN_INCREASE_VIEW,
                TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL,
                TouchOverlayView.ADMIN_NET_STATS,
                TouchOverlayView.ADMIN_QUICK_LOAD,
                TouchOverlayView.ADMIN_OPEN_MENU,
                TouchOverlayView.ADMIN_NET_EVENTS,
                TouchOverlayView.ADMIN_EXIT_LAUNCHER,
                TouchOverlayView.ADMIN_QUICK_SAVE,
                TouchOverlayView.ADMIN_VIDEO_INFO,
                TouchOverlayView.ADMIN_BRIGHTNESS,
                TouchOverlayView.ADMIN_WARP,
                TouchOverlayView.ADMIN_MUSIC,
                TouchOverlayView.ADMIN_ACCEPT_JOIN,
            ),
            actions,
        )
    }

    @Test
    fun pendingLaunchKeepsNetworkActionsVisibleWithoutWarp() {
        val actions =
            adminTrayVisibleActions(
                gamepadOnlyMode = true,
                hasTouchAutomapButton = true,
                hasPendingLaunchInfo = true,
            )

        assertTrue(actions.contains(TouchOverlayView.ADMIN_NET_STATS))
        assertTrue(actions.contains(TouchOverlayView.ADMIN_NET_EVENTS))
        assertTrue(actions.contains(TouchOverlayView.ADMIN_ACCEPT_JOIN))
        assertTrue(actions.contains(TouchOverlayView.ADMIN_BRIGHTNESS))
        assertFalse(actions.contains(TouchOverlayView.ADMIN_WARP))
    }

    @Test
    fun adminTrayActionsDefaultToEnabled() {
        assertTrue(adminTrayActionEnabled(TouchOverlayView.ADMIN_NET_EVENTS, null))
    }

    @Test
    fun adminTrayEnabledProviderCanDisableSpecificAction() {
        assertFalse(
            adminTrayActionEnabled(TouchOverlayView.ADMIN_NET_EVENTS) { action ->
                action != TouchOverlayView.ADMIN_NET_EVENTS
            },
        )
        assertTrue(
            adminTrayActionEnabled(TouchOverlayView.ADMIN_NET_STATS) { action ->
                action != TouchOverlayView.ADMIN_NET_EVENTS
            },
        )
    }

    @Test
    fun overlayTogglesUseCheckboxesAndStayOpen() {
        assertTrue(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_NET_STATS))
        assertTrue(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_NET_EVENTS))
        assertTrue(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_VIDEO_INFO))

        assertFalse(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_NET_STATS))
        assertFalse(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_NET_EVENTS))
        assertFalse(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_VIDEO_INFO))
    }

    @Test
    fun oneShotActionsStillCloseTheTray() {
        assertFalse(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL))
        assertFalse(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_QUICK_SAVE))
        assertFalse(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_OPEN_MENU))

        assertTrue(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_QUICK_SAVE))
        assertTrue(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_OPEN_MENU))
    }

    @Test
    fun musicSubmenuStaysOpenWithoutCheckboxStyling() {
        assertFalse(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_MUSIC))
        assertFalse(adminTrayUsesSlider(TouchOverlayView.ADMIN_MUSIC))
        assertFalse(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_MUSIC))
    }

    @Test
    fun brightnessUsesSliderSemanticsAndStaysOpen() {
        assertTrue(adminTrayUsesSlider(TouchOverlayView.ADMIN_BRIGHTNESS))
        assertFalse(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_BRIGHTNESS))
        assertFalse(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_BRIGHTNESS))
    }

    @Test
    fun brightnessHelpersClampAndStepWithinEngineRange() {
        assertEquals(0, clampAdminTrayBrightness(-4))
        assertEquals(16, clampAdminTrayBrightness(20))
        assertEquals(10, stepAdminTrayBrightness(9, 1))
        assertEquals(0, stepAdminTrayBrightness(0, -1))
        assertEquals(16, stepAdminTrayBrightness(16, 1))
    }

    @Test
    fun brightnessFractionMapsAcrossFullSliderWidth() {
        assertEquals(0, adminTrayBrightnessFromFraction(0f))
        assertEquals(8, adminTrayBrightnessFromFraction(0.5f))
        assertEquals(16, adminTrayBrightnessFromFraction(1f))
    }

    @Test
    fun autoLevelToggleStaysOpen() {
        assertFalse(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL))
    }

    @Test
    fun cycleViewStaysOpen() {
        assertFalse(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_INCREASE_VIEW))
        assertFalse(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_INCREASE_VIEW))
    }

    @Test
    fun overlayTogglesOccupyRightColumnSlots() {
        assertEquals(2, TouchOverlayView.ADMIN_NET_STATS)
        assertEquals(5, TouchOverlayView.ADMIN_NET_EVENTS)
        assertEquals(8, TouchOverlayView.ADMIN_VIDEO_INFO)
    }
}