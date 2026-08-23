package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class MenuInteractionOverlayViewTest {
    @Test
    fun pinchRequiresBothPointsOutsideNativeOwnedRegions() {
        assertTrue(menuPinchAllowed(0, 0))
        assertFalse(menuPinchAllowed(MENU_POINT_TAPPABLE, 0))
        assertFalse(menuPinchAllowed(0, MENU_POINT_SCROLL_OWNED))
        assertFalse(menuPinchAllowed(MENU_POINT_TAPPABLE, MENU_POINT_SCROLL_OWNED))
    }

    @Test
    fun pinchKeepsTheOldFocalPointStable() {
        val result = updateMenuPinchIntent(1f, 0f, 0.25f, 0.25f, 2f)
        assertEquals(2f, result.zoom, 0.0001f)
        assertEquals(0.25f, result.panFraction, 0.0001f)
    }

    @Test
    fun pinchIncludesVerticalCentroidMovement() {
        val result = updateMenuPinchIntent(1f, 0f, 0.5f, 0.6f, 1f)
        assertEquals(1f, result.zoom, 0.0001f)
        assertEquals(0.1f, result.panFraction, 0.0001f)
    }

    @Test
    fun pinchClampsZoomAndPanIntent() {
        val result = updateMenuPinchIntent(2.9f, 0.9f, 0f, 1f, 3f)
        assertEquals(3f, result.zoom, 0.0001f)
        assertTrue(result.panFraction in -1f..1f)
    }
}
