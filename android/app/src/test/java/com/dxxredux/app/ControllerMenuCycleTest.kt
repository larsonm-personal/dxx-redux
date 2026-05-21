package com.dxxredux.app

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
}
