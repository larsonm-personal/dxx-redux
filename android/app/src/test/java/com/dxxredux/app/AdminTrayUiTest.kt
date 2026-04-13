package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AdminTrayUiTest {
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
        assertFalse(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_INCREASE_VIEW))
        assertFalse(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL))
        assertFalse(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_QUICK_SAVE))
        assertFalse(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_OPEN_MENU))
        assertFalse(adminTrayUsesCheckbox(TouchOverlayView.ADMIN_MUSIC))

        assertTrue(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_INCREASE_VIEW))
        assertTrue(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_QUICK_SAVE))
        assertTrue(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_OPEN_MENU))
        assertTrue(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_MUSIC))
    }

    @Test
    fun autoLevelToggleStaysOpen() {
        assertFalse(adminTrayClosesAfterActivate(TouchOverlayView.ADMIN_TOGGLE_AUTOLEVEL))
    }
}