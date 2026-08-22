package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class TouchMouseEdgeMovementTest {
    @Test
    fun edgeRampIsLinearAndUsesConfiguredMaximum() {
        assertEquals(0f, contribution(position = 85f), 0.0001f)
        assertEquals(0.25f, contribution(position = 92.5f, maxRate = 50f), 0.0001f)
        assertEquals(1f, contribution(position = 100f), 0.0001f)
        assertEquals(1f, contribution(position = 120f), 0.0001f)
        assertEquals(-0.5f, contribution(position = 7.5f), 0.0001f)
        assertEquals(-1f, contribution(position = 0f), 0.0001f)
    }

    @Test
    fun directionRequiresHalfAnEdgeWidthOfTravel() {
        assertEquals(0f, contribution(position = 96f, origin = 90f), 0.0001f)
        assertEquals(5f / 6f, contribution(position = 97.5f, origin = 90f), 0.0001f)
        assertEquals(0f, contribution(position = 100f, origin = 100f), 0.0001f)
        assertEquals(-1f, contribution(position = 0f, origin = 100f), 0.0001f)
    }

    @Test
    fun disabledOrInvalidRegionHasNoContribution() {
        assertEquals(0f, contribution(position = 100f, enabled = false), 0.0001f)
        assertEquals(
            0f,
            mouseEdgeAxisContribution(true, 100f, 50f, 100f, 100f, 15f, 100f),
            0.0001f,
        )
    }

    @Test
    fun edgeIsAddedToDragBeforeInversionAndClamp() {
        assertEquals(0.75f, combineMouseDragAndEdge(0.25f, 0.5f, inverted = false), 0.0001f)
        assertEquals(0.25f, combineMouseDragAndEdge(-0.25f, 0.5f, inverted = false), 0.0001f)
        assertEquals(1f, combineMouseDragAndEdge(0.75f, 0.75f, inverted = false), 0.0001f)
        assertEquals(-0.75f, combineMouseDragAndEdge(0.25f, 0.5f, inverted = true), 0.0001f)
    }

    @Test
    fun edgeSettingsRoundTripThroughStoredAndHumanJson() {
        val layout =
            TouchLayout(
                sticks =
                    listOf(
                        edgeStick(
                            enabled = true,
                            edgeRegionPct = 22f,
                            edgeMaxRatePct = 70f,
                        ),
                    ),
            )

        val stored = TouchLayout.fromJson(layout.toJson()).sticks.single()
        val human =
            HumanReadableConfig
                .humanJsonToTouchLayout(HumanReadableConfig.touchLayoutToHumanJson(layout))
                .value
                ?.sticks
                ?.single()

        assertTrue(stored.mouseEdgeContinuousMovement)
        assertEquals(22f, stored.mouseEdgeRegionPct, 0f)
        assertEquals(70f, stored.mouseEdgeMaxRatePct, 0f)
        assertTrue(human?.mouseEdgeContinuousMovement == true)
        assertEquals(22f, human?.mouseEdgeRegionPct ?: 0f, 0f)
        assertEquals(70f, human?.mouseEdgeMaxRatePct ?: 0f, 0f)
    }

    @Test
    fun edgeSettingsDefaultOffAndValidateRanges() {
        val defaults = edgeStick()
        assertEquals(false, defaults.mouseEdgeContinuousMovement)
        assertEquals(TouchBindings.DEFAULT_MOUSE_EDGE_REGION_PCT, defaults.mouseEdgeRegionPct, 0f)
        assertEquals(TouchBindings.DEFAULT_MOUSE_EDGE_MAX_RATE_PCT, defaults.mouseEdgeMaxRatePct, 0f)
        assertNull(validateTouchLayoutDomains(TouchLayout(sticks = listOf(defaults))))
        assertTrue(
            validateTouchLayoutDomains(
                TouchLayout(sticks = listOf(defaults.copy(mouseEdgeRegionPct = 51f))),
            )?.contains("mouseEdgeRegionPct") == true,
        )
        assertTrue(
            validateTouchLayoutDomains(
                TouchLayout(sticks = listOf(defaults.copy(mouseEdgeMaxRatePct = -1f))),
            )?.contains("mouseEdgeMaxRatePct") == true,
        )
    }

    private fun contribution(
        position: Float,
        origin: Float = 50f,
        enabled: Boolean = true,
        maxRate: Float = 100f,
    ): Float =
        mouseEdgeAxisContribution(
            enabled = enabled,
            positionPx = position,
            originPx = origin,
            lowPx = 0f,
            highPx = 100f,
            edgeRegionPct = 15f,
            edgeMaxRatePct = maxRate,
        )

    private fun edgeStick(
        enabled: Boolean = false,
        edgeRegionPct: Float = TouchBindings.DEFAULT_MOUSE_EDGE_REGION_PCT,
        edgeMaxRatePct: Float = TouchBindings.DEFAULT_MOUSE_EDGE_MAX_RATE_PCT,
    ) = AnalogStickControl(
        id = "look",
        xPct = 75f,
        yPct = 75f,
        axisX = TouchBindings.AXIS_RIGHT_X,
        axisY = TouchBindings.AXIS_RIGHT_Y,
        mouseMode = true,
        mouseEdgeContinuousMovement = enabled,
        mouseEdgeRegionPct = edgeRegionPct,
        mouseEdgeMaxRatePct = edgeMaxRatePct,
    )
}
