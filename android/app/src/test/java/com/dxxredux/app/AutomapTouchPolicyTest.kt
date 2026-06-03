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
    fun automapActionsExposeRecenterAndClampedMarkers() {
        val actions = automapTouchActions(markerCount = 12, includeMarkers = true)

        assertEquals(11, actions.size)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_RECENTER, actions.first().adminAction)
        assertEquals("Recenter Map", actions.first().label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE, actions[1].adminAction)
        assertEquals("Jump to Marker 1", actions[1].label)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE + 9, actions.last().adminAction)
        assertEquals("Jump to Marker 10", actions.last().label)
    }

    @Test
    fun automapActionsHideMarkersWhenUnsupported() {
        val actions = automapTouchActions(markerCount = 4, includeMarkers = false)

        assertEquals(1, actions.size)
        assertEquals(TouchOverlayView.ADMIN_AUTOMAP_RECENTER, actions.single().adminAction)
    }

    @Test
    fun automapMarkerAdminActionMapsOnlyMarkerRange() {
        assertEquals(0, automapMarkerAdminActionIndex(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE))
        assertEquals(9, automapMarkerAdminActionIndex(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE + 9))
        assertEquals(null, automapMarkerAdminActionIndex(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE - 1))
        assertEquals(null, automapMarkerAdminActionIndex(TouchOverlayView.ADMIN_AUTOMAP_MARKER_BASE + 10))
    }
}
