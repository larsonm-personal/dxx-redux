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
    fun edgeHitSlopUsesThreePercentOfScreenWidthWithMinimum() {
        assertEquals(12f, defaultTouchEditorEdgeHitSlopPx(100f), 0.001f)
        assertEquals(30f, defaultTouchEditorEdgeHitSlopPx(1000f), 0.001f)
    }

    @Test
    fun defaultReticlePreviewStaysCenteredAndUsesShortScreenDimension() {
        val landscape = defaultReticlePreviewGeometry(width = 1920f, height = 1080f)
        val portrait = defaultReticlePreviewGeometry(width = 1080f, height = 1920f)

        assertEquals(Offset(960f, 540f), landscape.center)
        assertEquals(Offset(540f, 960f), portrait.center)
        assertEquals(4.5f, landscape.unit, 0.001f)
        assertEquals(landscape.unit, portrait.unit, 0.001f)
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
    fun regionEdgesUseExpandedHitTargetOnBothAxes() {
        val layout =
            TouchLayout(
                axisRegions =
                    listOf(
                        AxisRegionControl(
                            id = "region0",
                            zone = FloatingZone(leftPct = 20f, topPct = 20f, rightPct = 80f, bottomPct = 80f),
                        ),
                    ),
            )
        val leftEdge = "axisRegionEdge" to encodeFloatingZoneEdgeSelection(0, FloatingZoneEdge.LEFT)
        val topEdge = "axisRegionEdge" to encodeFloatingZoneEdgeSelection(0, FloatingZoneEdge.TOP)

        assertTrue(hitTestAll(layout, Offset(171f, 500f), 1000f, 1000f).contains(leftEdge))
        assertFalse(hitTestAll(layout, Offset(169f, 500f), 1000f, 1000f).contains(leftEdge))
        assertTrue(hitTestAll(layout, Offset(500f, 171f), 1000f, 1000f).contains(topEdge))
        assertFalse(hitTestAll(layout, Offset(500f, 169f), 1000f, 1000f).contains(topEdge))
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

    @Test
    fun overlappingRegionEdgeJoinsTapCycleAfterDiscreteControl() {
        val layout =
            TouchLayout(
                buttons =
                    listOf(
                        ButtonControl(
                            id = "button0",
                            xPct = 20f,
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

        val hits = hitTestAll(layout, Offset(200f, 500f), canvasWidth = 1000f, canvasHeight = 1000f)

        assertEquals(
            listOf(
                "button" to 0,
                "axisRegionEdge" to encodeFloatingZoneEdgeSelection(0, FloatingZoneEdge.LEFT),
            ),
            hits,
        )
    }

    @Test
    fun selectedAxisRegionEdgeDragResizesThatEdge() {
        val original = FloatingZone(leftPct = 20f, topPct = 20f, rightPct = 80f, bottomPct = 80f)
        val layout = TouchLayout(axisRegions = listOf(AxisRegionControl(id = "region0", zone = original)))

        val moved =
            moveControl(
                layout = layout,
                type = "axisRegionEdge",
                index = encodeFloatingZoneEdgeSelection(0, FloatingZoneEdge.LEFT),
                dxPct = 5f,
                dyPct = 9f,
                canvasWidth = 1000f,
                canvasHeight = 1000f,
            ).axisRegions.single().zone

        assertEquals(25f, moved.leftPct, 0.001f)
        assertEquals(original.topPct, moved.topPct, 0.001f)
        assertEquals(original.rightPct, moved.rightPct, 0.001f)
        assertEquals(original.bottomPct, moved.bottomPct, 0.001f)
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

    @Test
    fun humanReadableRadialRoundTripPreservesBindingType() {
        val layout =
            TouchLayout(
                radialMenus =
                    listOf(
                        RadialMenuControl(
                            id = "weapons",
                            xPct = 50f,
                            yPct = 50f,
                            segments =
                                listOf(
                                    RadialSegment(
                                        label = "Fire Primary",
                                        binding = TouchBindings.BTN_FIRE_PRIMARY,
                                        bindingType = "action",
                                    ),
                                ),
                        ),
                    ),
            )

        val parsed = HumanReadableConfig.humanJsonToTouchLayout(HumanReadableConfig.touchLayoutToHumanJson(layout))

        assertTrue(parsed.warnings.isEmpty())
        assertEquals(
            "action",
            parsed.value
                ?.radialMenus
                ?.single()
                ?.segments
                ?.single()
                ?.bindingType,
        )
    }

    @Test
    fun persistedCrossedAndCollapsedZonesAreNormalizedWithoutLosingControls() {
        val raw =
            TouchLayout(
                sticks =
                    listOf(
                        AnalogStickControl(
                            id = "move",
                            xPct = 20f,
                            yPct = 80f,
                            axisX = TouchBindings.AXIS_LEFT_X,
                            axisY = TouchBindings.AXIS_LEFT_Y,
                            floatingZone = FloatingZone(leftPct = 80f, topPct = 40f, rightPct = 20f, bottomPct = 40f),
                        ),
                    ),
            ).toJson()

        val parsed = TouchLayout.fromJson(raw)
        val zone = parsed.sticks.single().floatingZone

        assertEquals(20f, zone.leftPct, 0.001f)
        assertEquals(80f, zone.rightPct, 0.001f)
        assertEquals(MIN_TOUCH_ZONE_SIZE_PCT, zone.bottomPct - zone.topPct, 0.001f)
        assertEquals("move", parsed.sticks.single().id)
    }

    @Test
    fun appSerializedFloatMaximumsRoundTrip() {
        val layout =
            TouchLayout(
                sticks =
                    listOf(
                        AnalogStickControl(
                            id = "move",
                            xPct = 20f,
                            yPct = 80f,
                            axisX = TouchBindings.AXIS_LEFT_X,
                            axisY = TouchBindings.AXIS_LEFT_Y,
                            extremeActions =
                                listOf(
                                    StickExtremeAction(
                                        threshold = TouchBindings.MAX_STICK_EXTREME_THRESHOLD,
                                        releaseThreshold = 2.45f,
                                    ),
                                ),
                        ),
                    ),
                gyro =
                    GyroConfig(
                        maxAngleX = 1.57f,
                        maxAngleY = 1.57f,
                        maxAngleZ = 1.57f,
                    ),
            )

        val parsed = TouchLayout.fromJson(layout.toJson())

        assertEquals(1.57f, parsed.gyro.maxAngleX, 0f)
        assertEquals(1.57f, parsed.gyro.maxAngleY, 0f)
        assertEquals(1.57f, parsed.gyro.maxAngleZ, 0f)
        assertEquals(2.45f, parsed.sticks.single().extremeActions.single().releaseThreshold, 0f)
    }

    @Test
    fun storedSlotWithCrossedZoneRepairsInsteadOfRejectingWholeSet() {
        val layout =
            TouchLayout(
                axisRegions =
                    listOf(
                        AxisRegionControl(
                            id = "throttle",
                            zone = FloatingZone(leftPct = 90f, topPct = 75f, rightPct = 70f, bottomPct = 25f),
                        ),
                    ),
            )
        val slots =
            JSONArray().put(
                JSONObject()
                    .put("name", DEFAULT_CONFIG_SLOT_NAME)
                    .put("layout", HumanReadableConfig.touchLayoutToHumanJson(layout)),
            )

        val parsed = TouchLayoutSlotRepository.fromExportJsonArray(slots, activeIndex = 0)
        val zone = parsed?.activeSlot?.value?.axisRegions?.single()?.zone

        assertEquals(70f, zone?.leftPct ?: -1f, 0.001f)
        assertEquals(90f, zone?.rightPct ?: -1f, 0.001f)
        assertEquals(25f, zone?.topPct ?: -1f, 0.001f)
        assertEquals(75f, zone?.bottomPct ?: -1f, 0.001f)
    }
}
