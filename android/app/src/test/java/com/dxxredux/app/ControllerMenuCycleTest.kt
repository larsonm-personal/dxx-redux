package com.dxxredux.app

import android.view.KeyEvent
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ControllerMenuCycleTest {
    @Test
    fun nextControllerMenuSurface_cyclesThroughRemainingAndAdminTray() {
        assertEquals(
            ControllerMenuSurface.REMAINING_ACTIONS,
            nextControllerMenuSurface(ControllerMenuSurface.NONE, hasRemainingActions = true),
        )
        assertEquals(
            ControllerMenuSurface.ADMIN_TRAY,
            nextControllerMenuSurface(ControllerMenuSurface.REMAINING_ACTIONS, hasRemainingActions = true),
        )
        assertEquals(
            ControllerMenuSurface.NONE,
            nextControllerMenuSurface(ControllerMenuSurface.ADMIN_TRAY, hasRemainingActions = true),
        )
    }

    @Test
    fun nextControllerMenuSurface_skipsRemainingWhenNoActionsExist() {
        assertEquals(
            ControllerMenuSurface.ADMIN_TRAY,
            nextControllerMenuSurface(ControllerMenuSurface.NONE, hasRemainingActions = false),
        )
    }

    @Test
    fun hasControllerMenuBinding_detectsMenuLabel() {
        val bindings = mapOf("Select" to "Menu", "Start" to "Game Menu (ESC)")

        assertTrue(hasControllerMenuBinding(bindings))
    }

    @Test
    fun hasControllerMenuBinding_rejectsConfigsWithoutMenu() {
        val bindings = mapOf("Select" to "Automap", "Start" to "Game Menu (ESC)")

        assertFalse(hasControllerMenuBinding(bindings))
    }

    @Test
    fun moveRemainingActionSelection_movesWithinColumn() {
        assertEquals(
            1,
            moveRemainingActionSelection(
                currentIndex = 0,
                actionCount = 4,
                rowCount = 3,
                keyCode = KeyEvent.KEYCODE_DPAD_DOWN,
            ),
        )
        assertEquals(
            2,
            moveRemainingActionSelection(
                currentIndex = 2,
                actionCount = 4,
                rowCount = 3,
                keyCode = KeyEvent.KEYCODE_DPAD_DOWN,
            ),
        )
        assertEquals(
            0,
            moveRemainingActionSelection(
                currentIndex = 1,
                actionCount = 4,
                rowCount = 3,
                keyCode = KeyEvent.KEYCODE_DPAD_UP,
            ),
        )
    }

    @Test
    fun moveRemainingActionSelection_usesClosestRowAcrossPartialColumns() {
        assertEquals(
            3,
            moveRemainingActionSelection(
                currentIndex = 1,
                actionCount = 4,
                rowCount = 3,
                keyCode = KeyEvent.KEYCODE_DPAD_RIGHT,
            ),
        )
        assertEquals(
            0,
            moveRemainingActionSelection(
                currentIndex = 3,
                actionCount = 4,
                rowCount = 3,
                keyCode = KeyEvent.KEYCODE_DPAD_LEFT,
            ),
        )
    }

    @Test
    fun onlyEnergyShieldUsesHeldRemainingActionActivation() {
        assertTrue(remainingActionUsesHeldActivation(TouchBindings.BTN_ENERGY_SHIELD))
        assertFalse(remainingActionUsesHeldActivation(TouchBindings.BTN_HEADLIGHT))
        assertFalse(remainingActionUsesHeldActivation(TouchBindings.BTN_AUTOMAP))
    }
}
