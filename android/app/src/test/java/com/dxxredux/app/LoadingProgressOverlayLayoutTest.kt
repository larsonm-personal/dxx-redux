package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class LoadingProgressOverlayLayoutTest {
    @Test
    fun layoutPlacesBarNearBottomAndKeepsRequestedWidth() {
        val layout =
            computeLoadingProgressOverlayLayout(
                width = 1000,
                height = 500,
                density = 2f,
                percent = 35,
            )

        assertEquals(100f, layout.left)
        assertEquals(900f, layout.right)
        assertEquals(400f, layout.top)
        assertEquals(448f, layout.bottom)
        assertEquals(35, layout.clampedPercent)
    }

    @Test
    fun layoutClampsOutOfRangeProgress() {
        val low = computeLoadingProgressOverlayLayout(800, 600, 1.5f, -10)
        val high = computeLoadingProgressOverlayLayout(800, 600, 1.5f, 140)

        assertEquals(0, low.clampedPercent)
        assertEquals(100, high.clampedPercent)
    }
}