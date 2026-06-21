package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TouchStickExtremeActionTest {
    @Test
    fun missingExtremeActionsParsesAsEmpty() {
        val stick =
            AnalogStickControl.fromJson(
                JSONObject(
                    """
                    {
                      "id": "move",
                      "x": 10,
                      "y": 20,
                      "axisX": 0,
                      "axisY": 1
                    }
                    """.trimIndent(),
                ),
            )

        assertTrue(stick.extremeActions.isEmpty())
    }

    @Test
    fun rawJsonRoundTripsExtremeAction() {
        val layout =
            TouchLayout(
                sticks =
                    listOf(
                        AnalogStickControl(
                            id = "move",
                            xPct = 10f,
                            yPct = 20f,
                            axisX = TouchBindings.AXIS_LEFT_X,
                            axisY = TouchBindings.AXIS_LEFT_Y,
                            extremeActions =
                                listOf(
                                    StickExtremeAction(
                                        enabled = true,
                                        axis = StickExtremeAxis.Y,
                                        direction = StickExtremeDirection.NEGATIVE,
                                        threshold = 1.5f,
                                        releaseThreshold = 1.35f,
                                        binding = TouchBindings.BTN_AFTERBURNER,
                                        mode = StickExtremeActionMode.HOLD,
                                    ),
                                ),
                        ),
                    ),
            )

        val roundTrip = TouchLayout.fromJson(layout.toJson())
        val action = roundTrip.sticks.single().extremeActions.single()

        assertTrue(action.enabled)
        assertEquals(StickExtremeAxis.Y, action.axis)
        assertEquals(StickExtremeDirection.NEGATIVE, action.direction)
        assertEquals(TouchBindings.BTN_AFTERBURNER, action.binding)
        assertEquals(StickExtremeActionMode.HOLD, action.mode)
        assertEquals(1.5f, action.threshold, 0.0001f)
        assertEquals(1.35f, action.releaseThreshold, 0.0001f)
    }

    @Test
    fun humanReadableJsonRoundTripsExtremeBindingName() {
        val layout =
            TouchLayout(
                sticks =
                    listOf(
                        AnalogStickControl(
                            id = "move",
                            xPct = 10f,
                            yPct = 20f,
                            axisX = TouchBindings.AXIS_LEFT_X,
                            axisY = TouchBindings.AXIS_LEFT_Y,
                            extremeActions = listOf(StickExtremeAction(enabled = true)),
                        ),
                    ),
            )

        val json = HumanReadableConfig.touchLayoutToHumanJson(layout)
        val actionJson = json.getJSONArray("sticks").getJSONObject(0).getJSONArray("extremeActions").getJSONObject(0)
        val parsed = HumanReadableConfig.humanJsonToTouchLayout(json)

        assertEquals("Afterburner", actionJson.getString("binding"))
        assertTrue(parsed.warnings.isEmpty())
        assertEquals(TouchBindings.BTN_AFTERBURNER, parsed.value?.sticks?.single()?.extremeActions?.single()?.binding)
        assertEquals(StickExtremeDirection.POSITIVE, parsed.value?.sticks?.single()?.extremeActions?.single()?.direction)
    }

    @Test
    fun extremeActionUsesThresholdAndReleaseThreshold() {
        val action =
            StickExtremeAction(
                enabled = true,
                axis = StickExtremeAxis.Y,
                direction = StickExtremeDirection.POSITIVE,
                threshold = 1.5f,
                releaseThreshold = 1.35f,
            )

        assertFalse(stickExtremeActionPressed(action, axisX = 0f, axisY = 1.49f, wasPressed = false))
        assertTrue(stickExtremeActionPressed(action, axisX = 0f, axisY = 1.51f, wasPressed = false))
        assertTrue(stickExtremeActionPressed(action, axisX = 0f, axisY = 1.36f, wasPressed = true))
        assertFalse(stickExtremeActionPressed(action, axisX = 0f, axisY = 1.34f, wasPressed = true))
    }

    @Test
    fun afterburnerChargeBarIsD2Only() {
        val actions = listOf(StickExtremeAction(enabled = true, binding = TouchBindings.BTN_AFTERBURNER))

        assertTrue(stickAfterburnerChargeVisible(gameVariant = "d2", actions))
        assertFalse(stickAfterburnerChargeVisible(gameVariant = "d1", actions))
    }

    @Test
    fun afterburnerChargeBarRequiresEnabledAfterburnerAction() {
        assertFalse(
            stickAfterburnerChargeVisible(
                gameVariant = "d2",
                actions = listOf(StickExtremeAction(enabled = false, binding = TouchBindings.BTN_AFTERBURNER)),
            ),
        )
        assertFalse(
            stickAfterburnerChargeVisible(
                gameVariant = "d2",
                actions = listOf(StickExtremeAction(enabled = true, binding = TouchBindings.BTN_FIRE_PRIMARY)),
            ),
        )
    }

    @Test
    fun upwardTouchDragIsPositiveExtremeY() {
        val (axisX, axisY) =
            stickExtremeTravelFromTouch(
                dxPx = 0f,
                dyPx = -150f,
                radiusPx = 100f,
                invertX = false,
                invertY = false,
            )

        assertEquals(0f, axisX, 0.0001f)
        assertEquals(1.5f, axisY, 0.0001f)
    }
}
