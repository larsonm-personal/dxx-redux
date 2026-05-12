package com.dxxredux.app

import androidx.compose.ui.geometry.Offset
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TouchEditorZoneEdgeTest {
    @Test
    fun edgeSelectionEncodingRoundTrips() {
        val encoded = encodeFloatingZoneEdgeSelection(3, FloatingZoneEdge.RIGHT)
        val (index, edge) = decodeFloatingZoneEdgeSelection(encoded)

        assertEquals(3, index)
        assertEquals(FloatingZoneEdge.RIGHT, edge)
    }

    @Test
    fun edgeHitSlopUsesButtonOversizeWithMinimum() {
        assertEquals(12f, defaultTouchEditorEdgeHitSlopPx(8f), 0.001f)
        assertEquals(18f, defaultTouchEditorEdgeHitSlopPx(60f), 0.001f)
    }

    @Test
    fun resizingFloatingZoneClampsToMinimumWidthAndBounds() {
        val zone = FloatingZone(leftPct = 10f, topPct = 20f, rightPct = 30f, bottomPct = 50f)

        val resizedLeft = resizeFloatingZone(zone, FloatingZoneEdge.LEFT, dxPct = 40f, dyPct = 0f)
        val resizedRight = resizeFloatingZone(zone, FloatingZoneEdge.RIGHT, dxPct = -40f, dyPct = 0f)

        assertEquals(28f, resizedLeft.leftPct, 0.001f)
        assertEquals(12f, resizedRight.rightPct, 0.001f)
    }

    @Test
    fun resizingFloatingZoneClampsToMinimumHeightAndBounds() {
        val zone = FloatingZone(leftPct = 10f, topPct = 20f, rightPct = 30f, bottomPct = 50f)

        val resizedTop = resizeFloatingZone(zone, FloatingZoneEdge.TOP, dxPct = 0f, dyPct = 40f)
        val resizedBottom = resizeFloatingZone(zone, FloatingZoneEdge.BOTTOM, dxPct = 0f, dyPct = -40f)

        assertEquals(48f, resizedTop.topPct, 0.001f)
        assertEquals(22f, resizedBottom.bottomPct, 0.001f)
    }

    @Test
    fun resizingFloatingZonePreservesUntouchedEdges() {
        val zone = FloatingZone(leftPct = 10f, topPct = 20f, rightPct = 30f, bottomPct = 50f)
        val resized = resizeFloatingZone(zone, FloatingZoneEdge.BOTTOM, dxPct = 0f, dyPct = 5f)

        assertEquals(10f, resized.leftPct, 0.001f)
        assertEquals(20f, resized.topPct, 0.001f)
        assertEquals(30f, resized.rightPct, 0.001f)
        assertTrue(resized.bottomPct > zone.bottomPct)
    }

    @Test
    fun floatingStickEdgesAreHitTestedOutsideMouseMode() {
        val layout =
            TouchLayout(
                sticks =
                    listOf(
                        AnalogStickControl(
                            id = "stick0",
                            xPct = 80f,
                            yPct = 80f,
                            axisX = 0,
                            axisY = 1,
                            floating = true,
                            floatingZone = FloatingZone(leftPct = 10f, topPct = 20f, rightPct = 30f, bottomPct = 50f),
                        ),
                    ),
            )

        val hits = hitTestAll(layout, Offset(100f, 350f), canvasWidth = 1000f, canvasHeight = 1000f)

        assertTrue(hits.contains("stickZoneEdge" to encodeFloatingZoneEdgeSelection(0, FloatingZoneEdge.LEFT)))
    }

    @Test
    fun nonFloatingNonMouseStickDoesNotExposeZoneEdges() {
        val layout =
            TouchLayout(
                sticks =
                    listOf(
                        AnalogStickControl(
                            id = "stick0",
                            xPct = 80f,
                            yPct = 80f,
                            axisX = 0,
                            axisY = 1,
                            floating = false,
                            mouseMode = false,
                            floatingZone = FloatingZone(leftPct = 10f, topPct = 20f, rightPct = 30f, bottomPct = 50f),
                        ),
                    ),
            )

        val hits = hitTestAll(layout, Offset(100f, 350f), canvasWidth = 1000f, canvasHeight = 1000f)

        assertFalse(hits.any { it.first == "stickZoneEdge" })
    }

    @Test
    fun moreActionsButtonIsHitTestedInEditor() {
        val layout = TouchLayout(moreActions = MoreActionsControl(xPct = 50f, yPct = 50f))

        val hits = hitTestAll(layout, Offset(500f, 500f), canvasWidth = 1000f, canvasHeight = 1000f)

        assertTrue(hits.contains("moreActions" to 0))
    }
}
