package com.dxxredux.app

import androidx.compose.ui.geometry.Offset
import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
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
    fun absoluteSliderUpdatesCannotCrossOppositeEdges() {
        val zone = FloatingZone(leftPct = 10f, topPct = 20f, rightPct = 30f, bottomPct = 50f)

        assertEquals(28f, setFloatingZoneEdge(zone, FloatingZoneEdge.LEFT, 100f).leftPct, 0.001f)
        assertEquals(12f, setFloatingZoneEdge(zone, FloatingZoneEdge.RIGHT, 0f).rightPct, 0.001f)
        assertEquals(48f, setFloatingZoneEdge(zone, FloatingZoneEdge.TOP, 100f).topPct, 0.001f)
        assertEquals(22f, setFloatingZoneEdge(zone, FloatingZoneEdge.BOTTOM, 0f).bottomPct, 0.001f)
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

    @Test
    fun scrollStripTakesPriorityOverOverlappingStickRegion() {
        val layout =
            TouchLayout(
                sticks =
                    listOf(
                        AnalogStickControl(
                            id = "stick0",
                            xPct = 50f,
                            yPct = 50f,
                            axisX = 0,
                            axisY = 1,
                            mouseMode = true,
                            floatingZone = FloatingZone(leftPct = 20f, topPct = 20f, rightPct = 80f, bottomPct = 80f),
                        ),
                    ),
                radialMenus =
                    listOf(
                        RadialMenuControl(
                            id = "guidebot",
                            xPct = 50f,
                            yPct = 50f,
                            segments = listOf(RadialSegment("Guidebot", 0)),
                            presentation = SelectorPresentation.SCROLL_STRIP,
                        ),
                    ),
            )

        val hits = hitTestAll(layout, Offset(500f, 500f), canvasWidth = 1000f, canvasHeight = 1000f)

        assertEquals(listOf("radial" to 0), hits)
    }

    @Test
    fun buttonTakesPriorityOverOverlappingAxisRegion() {
        val layout =
            TouchLayout(
                buttons =
                    listOf(
                        ButtonControl(
                            id = "button0",
                            xPct = 50f,
                            yPct = 50f,
                            binding = TouchBindings.BTN_FIRE_PRIMARY,
                        ),
                    ),
                axisRegions =
                    listOf(
                        AxisRegionControl(
                            id = "region0",
                            zone = FloatingZone(leftPct = 20f, topPct = 20f, rightPct = 80f, bottomPct = 80f),
                        ),
                    ),
            )

        val hits = hitTestAll(layout, Offset(500f, 500f), canvasWidth = 1000f, canvasHeight = 1000f)

        assertEquals(listOf("button" to 0), hits)
    }

    private fun minimalHumanLayout(): JSONObject =
        JSONObject()
            .put("type", "touch_layout")
            .put("version", 2)
            .put("globalOpacity", 0.7)
            .put("sticks", JSONArray())
            .put("buttons", JSONArray())
            .put("sliders", JSONArray())
            .put("radialMenus", JSONArray())
            .put("dpads", JSONArray())

    @Test
    fun touchLayoutRejectsInvalidNumericDomainsWithoutPartialResults() {
        for (value in listOf("NaN", "Infinity", "-Infinity", "0.7", 0.19, 1.01, 1e100)) {
            val result = HumanReadableConfig.humanJsonToTouchLayout(minimalHumanLayout().put("globalOpacity", value))
            assertNull(result.value)
        }
        val button =
            JSONObject()
                .put("id", "fire")
                .put("x", 50.0)
                .put("y", "NaN")
                .put("binding", "Fire Primary")
        val result =
            HumanReadableConfig.humanJsonToTouchLayout(
                minimalHumanLayout().put("buttons", JSONArray().put(button)),
            )
        assertNull(result.value)
        assertTrue(result.warnings.joinToString().contains("buttons[0].y"))
    }

    @Test
    fun rawTouchCodecAndResponseMathRejectNonfiniteValues() {
        assertTrue(runCatching { TouchLayout.fromJson(TouchLayout().toJson().put("globalOpacity", "NaN")) }.isFailure)
        assertEquals(0f, applyResponseCurve(0.5f, ResponseCurve.S_CURVE, Float.NaN), 0f)
        assertEquals(0f, applyResponseCurve(Float.POSITIVE_INFINITY, ResponseCurve.EXPONENTIAL, 2f), 0f)
        assertTrue(applyResponseCurve(1f, ResponseCurve.S_CURVE, 2f).isFinite())
    }

    @Test
    fun humanTouchLayoutRequiresCompleteKnownSchema() {
        assertNull(HumanReadableConfig.humanJsonToTouchLayout(JSONObject()).value)
        assertNull(
            HumanReadableConfig
                .humanJsonToTouchLayout(
                    minimalHumanLayout().put("version", CURRENT_TOUCH_LAYOUT_VERSION + 1),
                ).value,
        )
        assertNull(HumanReadableConfig.humanJsonToTouchLayout(minimalHumanLayout().apply { remove("buttons") }).value)

        val button =
            JSONObject()
                .put("id", "fire")
                .put("x", 50.0)
                .put("y", 50.0)
                .put("binding", 256)
        assertNull(
            HumanReadableConfig
                .humanJsonToTouchLayout(
                    minimalHumanLayout().put("buttons", JSONArray().put(button)),
                ).value,
        )
        button.put("binding", "binding_256")
        assertNull(
            HumanReadableConfig
                .humanJsonToTouchLayout(
                    minimalHumanLayout().put("buttons", JSONArray().put(button)),
                ).value,
        )
    }
}
