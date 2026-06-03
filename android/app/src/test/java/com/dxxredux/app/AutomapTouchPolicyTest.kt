package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AutomapTouchPolicyTest {
    @Test
    fun automapOverlayKeepsOnlyMapAndMovementButtons() {
        assertTrue(automapTouchButtonVisible(TouchBindings.BTN_AUTOMAP))
        assertTrue(automapTouchButtonVisible(TouchBindings.BTN_ACCELERATE))
        assertTrue(automapTouchButtonVisible(TouchBindings.BTN_REVERSE))
        assertTrue(automapTouchButtonVisible(TouchBindings.BTN_SLIDE_ON))
        assertTrue(automapTouchButtonVisible(TouchBindings.BTN_BANK_RIGHT))

        assertFalse(automapTouchButtonVisible(TouchBindings.BTN_FIRE_PRIMARY))
        assertFalse(automapTouchButtonVisible(TouchBindings.BTN_FIRE_SECONDARY))
        assertFalse(automapTouchButtonVisible(TouchBindings.BTN_FIRE_FLARE))
        assertFalse(automapTouchButtonVisible(TouchBindings.BTN_GYRO_RECENTER))
    }

    @Test
    fun automapActionsExposeRecenterAndMarkerSubmenus() {
        val actions = automapTouchActions(includeMarkers = true)

        assertEquals(4, actions.size)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP, actions.first().adminAction)
        assertEquals("Close Map", actions.first().label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_RECENTER, actions[1].adminAction)
        assertEquals("Recenter Map", actions[1].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_SET_MARKER_MENU, actions[2].adminAction)
        assertEquals("Set Marker", actions[2].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_JUMP_MARKER_MENU, actions[3].adminAction)
        assertEquals("Jump to Marker", actions[3].label)
    }

    @Test
    fun automapSetMarkerSubmenuExposesOnlySetMarkerSlots() {
        val actions =
            automapTouchActions(
                includeMarkers = true,
                markerMenuMode = AutomapMarkerMenuMode.SET,
            )

        assertEquals(11, actions.size)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP, actions.first().adminAction)
        assertEquals("Close Map", actions.first().label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_MARKER_MENU_ROOT, actions[1].adminAction)
        assertEquals("Back", actions[1].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_SET_MARKER_BASE, actions[2].adminAction)
        assertEquals("Set Marker 1", actions[2].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_SET_MARKER_BASE + 8, actions.last().adminAction)
        assertEquals("Set Marker 9", actions.last().label)
    }

    @Test
    fun automapJumpMarkerSubmenuExposesOnlyJumpMarkerSlots() {
        val actions =
            automapTouchActions(
                includeMarkers = true,
                markerMenuMode = AutomapMarkerMenuMode.JUMP,
            )

        assertEquals(11, actions.size)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP, actions.first().adminAction)
        assertEquals("Close Map", actions.first().label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_MARKER_MENU_ROOT, actions[1].adminAction)
        assertEquals("Back", actions[1].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE, actions[2].adminAction)
        assertEquals("Jump to Marker 1", actions[2].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE + 8, actions.last().adminAction)
        assertEquals("Jump to Marker 9", actions.last().label)
    }

    @Test
    fun automapActionsHideMarkersWhenUnsupported() {
        val actions = automapTouchActions(includeMarkers = false)

        assertEquals(2, actions.size)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP, actions.first().adminAction)
        assertEquals("Close Map", actions.first().label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_RECENTER, actions[1].adminAction)
        assertEquals("Recenter Map", actions[1].label)
    }

    @Test
    fun automapMarkerAdminActionMapsOnlyMarkerRange() {
        assertEquals(0, automapMarkerAdminActionIndex(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE))
        assertEquals(8, automapMarkerAdminActionIndex(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE + 8))
        assertEquals(null, automapMarkerAdminActionIndex(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE - 1))
        assertEquals(null, automapMarkerAdminActionIndex(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE + 9))
    }

    @Test
    fun automapSetMarkerAdminActionMapsOnlyMarkerRange() {
        assertEquals(0, automapSetMarkerAdminActionIndex(TouchOverlayView.ADMIN_AUTOMAP_SET_MARKER_BASE))
        assertEquals(8, automapSetMarkerAdminActionIndex(TouchOverlayView.ADMIN_AUTOMAP_SET_MARKER_BASE + 8))
        assertEquals(null, automapSetMarkerAdminActionIndex(TouchOverlayView.ADMIN_AUTOMAP_SET_MARKER_BASE - 1))
        assertEquals(null, automapSetMarkerAdminActionIndex(TouchOverlayView.ADMIN_AUTOMAP_SET_MARKER_BASE + 9))
    }
}
