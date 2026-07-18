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

        assertEquals(5, actions.size)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP, actions.first().adminAction)
        assertEquals("Close Map", actions.first().label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_RECENTER, actions[1].adminAction)
        assertEquals("Recenter Map", actions[1].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_SET_MARKER_MENU, actions[2].adminAction)
        assertEquals("Drop Marker", actions[2].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_NAME_MARKER, actions[3].adminAction)
        assertEquals("Name Marker", actions[3].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_JUMP_MARKER_MENU, actions[4].adminAction)
        assertEquals("Jump to Marker", actions[4].label)
    }

    @Test
    fun automapSetMarkerSubmenuExposesOnlyFreeMarkerSlots() {
        val actions =
            automapTouchActions(
                includeMarkers = true,
                markerMenuMode = AutomapMarkerMenuMode.SET,
                markerSlots = intArrayOf(0, 1, 0, -1, 1, 0, 0, 1, 0),
            )

        assertEquals(7, actions.size)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP, actions.first().adminAction)
        assertEquals("Close Map", actions.first().label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_MARKER_MENU_ROOT, actions[1].adminAction)
        assertEquals("Back", actions[1].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_SET_MARKER_BASE, actions[2].adminAction)
        assertEquals("Drop Marker 1", actions[2].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_SET_MARKER_BASE + 8, actions.last().adminAction)
        assertEquals("Drop Marker 9", actions.last().label)
    }

    @Test
    fun automapJumpMarkerSubmenuExposesOnlyPlacedMarkerSlots() {
        val actions =
            automapTouchActions(
                includeMarkers = true,
                markerMenuMode = AutomapMarkerMenuMode.JUMP,
                markerSlots = intArrayOf(0, 1, 0, -1, 1, 0, 0, 1, 0),
            )

        assertEquals(5, actions.size)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP, actions.first().adminAction)
        assertEquals("Close Map", actions.first().label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_MARKER_MENU_ROOT, actions[1].adminAction)
        assertEquals("Back", actions[1].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE + 1, actions[2].adminAction)
        assertEquals("Jump to Marker 2", actions[2].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE + 7, actions.last().adminAction)
        assertEquals("Jump to Marker 8", actions.last().label)
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
    fun previewActionsKeepNavigationWithoutPersistentMarkers() {
        val actions = automapTouchActions(includeMarkers = true, previewMode = true)

        assertEquals(
            listOf("Close Preview", "Recenter Map"),
            actions.map { it.label },
        )
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
