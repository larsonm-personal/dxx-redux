package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RemainingKeyTouchActionsTest {
    @Test
    fun d2LayoutWithoutBindingsGetsHeadlightAndPause() {
        val actions = remainingKeyTouchActions(TouchLayout(name = "Empty"), gameVariant = "d2")

        assertEquals(listOf(TouchBindings.BTN_HEADLIGHT, TouchBindings.META_PAUSE), actions.map { it.binding })
    }

    @Test
    fun d1LayoutExcludesHeadlight() {
        val actions = remainingKeyTouchActions(TouchLayout(name = "Empty"), gameVariant = "d1")

        assertEquals(listOf(TouchBindings.META_PAUSE), actions.map { it.binding })
    }

    @Test
    fun alreadyBoundHeadlightAndPauseAreFilteredOut() {
        val layout =
            TouchLayout(
                name = "Bound",
                buttons =
                    listOf(
                        ButtonControl(
                            id = "headlight",
                            xPct = 10f,
                            yPct = 10f,
                            binding = TouchBindings.BTN_HEADLIGHT,
                        ),
                    ),
                sticks =
                    listOf(
                        AnalogStickControl(
                            id = "look",
                            xPct = 60f,
                            yPct = 60f,
                            axisX = TouchBindings.AXIS_RIGHT_X,
                            axisY = TouchBindings.AXIS_RIGHT_Y,
                            doubleTapBinding = TouchBindings.META_PAUSE,
                        ),
                    ),
            )

        val actions = remainingKeyTouchActions(layout, gameVariant = "d2")

        assertTrue(actions.isEmpty())
    }

    @Test
    fun boundBindingsIncludeLongPressButtonModeRadialAndDpadSources() {
        val layout =
            TouchLayout(
                name = "Mixed",
                buttons =
                    listOf(
                        ButtonControl(
                            id = "gyro",
                            xPct = 10f,
                            yPct = 10f,
                            binding = TouchBindings.BTN_GYRO_RECENTER,
                            longPressEnabled = true,
                            longPressBinding = TouchBindings.META_PAUSE,
                        ),
                    ),
                sticks =
                    listOf(
                        AnalogStickControl(
                            id = "buttons",
                            xPct = 50f,
                            yPct = 50f,
                            axisX = TouchBindings.AXIS_LEFT_X,
                            axisY = TouchBindings.AXIS_LEFT_Y,
                            buttonMode = true,
                            negXBinding = TouchBindings.BTN_HEADLIGHT,
                            posXBinding = TouchBindings.BTN_FIRE_PRIMARY,
                            negYBinding = TouchBindings.BTN_FIRE_SECONDARY,
                            posYBinding = TouchBindings.BTN_AUTOMAP,
                        ),
                    ),
                radialMenus =
                    listOf(
                        RadialMenuControl(
                            id = "radial",
                            xPct = 40f,
                            yPct = 40f,
                            segments = listOf(RadialSegment("Pause", TouchBindings.META_PAUSE, bindingType = "action")),
                            centerBinding = TouchBindings.BTN_HEADLIGHT,
                        ),
                    ),
                dpads =
                    listOf(
                        DPadControl(
                            id = "dpad",
                            xPct = 20f,
                            yPct = 80f,
                            upBinding = TouchBindings.BTN_FIRE_PRIMARY,
                            downBinding = TouchBindings.BTN_FIRE_SECONDARY,
                            leftBinding = TouchBindings.BTN_HEADLIGHT,
                            rightBinding = TouchBindings.META_PAUSE,
                        ),
                    ),
            )

        val bindings = touchLayoutBoundActionBindings(layout)

        assertTrue(TouchBindings.META_PAUSE in bindings)
        assertTrue(TouchBindings.BTN_HEADLIGHT in bindings)
        assertTrue(TouchBindings.BTN_AUTOMAP in bindings)
    }
}
