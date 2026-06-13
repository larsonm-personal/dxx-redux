package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class TouchAxisRegionTravelTest {
    @Test
    fun dragValueUsesScreenBoundAboveOrigin() {
        assertEquals(-0.25f, axisRegionDragValue(position = 300f, touchOrigin = 400f, minBound = 0f, maxBound = 1000f), 0.0001f)
        assertEquals(-1f, axisRegionDragValue(position = 0f, touchOrigin = 400f, minBound = 0f, maxBound = 1000f), 0.0001f)
    }

    @Test
    fun dragValueUsesScreenBoundBelowOrigin() {
        assertEquals(0.5f, axisRegionDragValue(position = 700f, touchOrigin = 400f, minBound = 0f, maxBound = 1000f), 0.0001f)
        assertEquals(1f, axisRegionDragValue(position = 1000f, touchOrigin = 400f, minBound = 0f, maxBound = 1000f), 0.0001f)
    }

    @Test
    fun dragValueClampsOutsideScreenBounds() {
        assertEquals(-1f, axisRegionDragValue(position = -100f, touchOrigin = 400f, minBound = 0f, maxBound = 1000f), 0.0001f)
        assertEquals(1f, axisRegionDragValue(position = 1200f, touchOrigin = 400f, minBound = 0f, maxBound = 1000f), 0.0001f)
    }

    @Test
    fun dragValueReturnsZeroWhenOriginIsAtRequestedEdge() {
        assertEquals(0f, axisRegionDragValue(position = -100f, touchOrigin = 0f, minBound = 0f, maxBound = 1000f), 0.0001f)
        assertEquals(0f, axisRegionDragValue(position = 1200f, touchOrigin = 1000f, minBound = 0f, maxBound = 1000f), 0.0001f)
    }
}
