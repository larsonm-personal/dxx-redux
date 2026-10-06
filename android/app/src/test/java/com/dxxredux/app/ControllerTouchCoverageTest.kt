package com.dxxredux.app

import org.junit.Assert.*
import org.junit.Test

class ControllerTouchCoverageTest {
    private val available =
        ControllerInputAvailability(
            connected = true,
            buttons = (0..31).toSet(),
            axes = AXIS_CONTROLS.values.associateWith { setOf(false, true) },
        )

    private fun coverage(
        bindings: Map<String, String>,
        inputs: ControllerInputAvailability = available,
    ) = controllerTouchCoverage(bindings, emptySet(), inputs, "d2")

    private val stick =
        AnalogStickControl(
            "move",
            20f,
            30f,
            axisX = TouchBindings.AXIS_BANK,
            axisY = TouchBindings.AXIS_SLIDE_UD,
        )
    private val movement =
        mapOf(
            "DLeft" to "Bank Left",
            "DRight" to "Bank Right",
            "DUp" to "Slide Up",
            "DDown" to "Slide Down",
        )
    private val layout =
        TouchLayout(
            sticks = listOf(stick),
            buttons =
                listOf(
                    ButtonControl("flare", 80f, 80f, binding = TouchBindings.BTN_FIRE_FLARE),
                ),
        )

    @Test
    fun disconnectedOrDisabledFilteringKeepsOriginalLayout() {
        val bindings = movement + ("A" to "Fire Flare")
        val disconnected = coverage(bindings, available.copy(connected = false))
        assertSame(layout, controllerFilteredTouchLayout(layout, disconnected, "d2"))
        val disabled = layout.copy(hideControllerBoundControls = false)
        assertSame(disabled, controllerFilteredTouchLayout(disabled, coverage(bindings), "d2"))
        assertEquals(layout, controllerFilteredTouchLayout(layout, coverage(emptyMap()), "d2"))
    }

    @Test
    fun digitalDirectionsCoverAxesButMissingDirectionKeepsWholeStick() {
        assertTrue(controllerFilteredTouchLayout(layout, coverage(movement), "d2").sticks.isEmpty())
        assertEquals(layout.sticks, controllerFilteredTouchLayout(layout, coverage(movement - "DUp"), "d2").sticks)
        val missingKey = available.copy(buttons = available.buttons - DPAD_CONTROLS.getValue("DUp"))
        assertEquals(layout.sticks, controllerFilteredTouchLayout(layout, coverage(movement, missingKey), "d2").sticks)
    }

    @Test
    fun extraStickAndButtonActionsMustAlsoBeCovered() {
        val custom =
            layout.copy(
                sticks =
                    listOf(
                        stick.copy(
                            doubleTapBinding = TouchBindings.BTN_DROP_BOMB,
                            extremeActions = listOf(StickExtremeAction(enabled = true)),
                        ),
                    ),
                buttons =
                    listOf(
                        layout.buttons.single().copy(
                            longPressEnabled = true,
                            longPressBinding = TouchBindings.META_QUICK_SAVE,
                        ),
                    ),
            )
        val partial = coverage(movement + ("A" to "Fire Flare"))
        assertEquals(custom, controllerFilteredTouchLayout(custom, partial, "d2"))
        val complete =
            coverage(
                movement +
                    mapOf(
                        "A" to "Fire Flare",
                        "R3" to "Drop Bomb",
                        "L3" to "Afterburner",
                        "X" to "Quick Save",
                    ),
            )
        val filtered = controllerFilteredTouchLayout(custom, complete, "d2")
        assertTrue(filtered.sticks.isEmpty())
        assertTrue(filtered.buttons.isEmpty())
        assertEquals(1, custom.sticks.size)
    }

    @Test
    fun menusAndGyroActivationRemainAvailable() {
        val menu = ButtonControl("menu", 50f, 90f, binding = TouchBindings.META_GUIDE_BOT_MENU)
        val custom =
            layout.copy(
                buttons = listOf(menu),
                gyro = GyroConfig(enabled = true, activation = GyroActivation.TOUCH_STICK, axisX = stick.axisX),
            )
        assertEquals(
            custom,
            controllerFilteredTouchLayout(custom, coverage(movement + ("Y" to "Guide Bot Menu")), "d2"),
        )
    }

    @Test
    fun onlyConnectedTriggerAlternativesProvideActions() {
        val bindings = mapOf("RT" to "Fire Primary", "GAS" to "Fire Primary", "R2" to "Fire Primary")
        for (button in listOf(21, 31, 27)) {
            assertTrue(
                TouchBindings.BTN_FIRE_PRIMARY in coverage(bindings, available.copy(buttons = setOf(button))).actions,
            )
        }
        assertFalse(TouchBindings.BTN_FIRE_PRIMARY in coverage(bindings, available.copy(buttons = emptySet())).actions)
    }

    @Test
    fun fullAxesHalfAxesAndInversionUseDispatchAssignments() {
        val inputs = available.copy(axes = mapOf(4 to setOf(true), 5 to setOf(true)))
        val half = coverage(mapOf("RT" to "Accelerate"), inputs)
        assertTrue(("Throttle" to false) in half.directions)
        assertFalse(("Throttle" to true) in half.directions)
        val both = coverage(mapOf("RT" to "Accelerate", "LT" to "Reverse"), inputs)
        assertTrue(both.directions.containsAll(setOf("Throttle" to false, "Throttle" to true)))
        val inverted = controllerTouchCoverage(mapOf("RT" to "Throttle"), setOf("RT"), inputs, "d2")
        assertEquals(setOf("Throttle" to false), inverted.directions)
        // A configured but disconnected full axis wins engine assignment over a half-axis combiner
        val overridden = coverage(mapOf("LS_Y" to "Throttle", "RT" to "Accelerate"), inputs)
        assertFalse(("Throttle" to true) in overridden.directions)
    }

    @Test
    fun d1DoesNotKeepStickOnlyForUnavailableAfterburner() {
        val custom =
            layout.copy(
                sticks = listOf(stick.copy(extremeActions = listOf(StickExtremeAction(enabled = true)))),
            )
        assertTrue(controllerFilteredTouchLayout(custom, coverage(movement), "d1").sticks.isEmpty())
        assertFalse(controllerFilteredTouchLayout(custom, coverage(movement), "d2").sticks.isEmpty())
    }

    @Test
    fun axesCoverMatchingDigitalButtonsAndPartialControlsStay() {
        val axis = coverage(mapOf("LS_Y" to "Throttle"))
        assertTrue(TouchBindings.BTN_ACCELERATE in axis.actions)
        assertTrue(TouchBindings.BTN_REVERSE in axis.actions)
        val custom =
            TouchLayout(
                sliders = listOf(SliderControl("throttle", 10f, 20f, axis = TouchBindings.AXIS_LEFT_Y)),
                axisRegions = listOf(AxisRegionControl("vertical")),
                dpads =
                    listOf(
                        DPadControl(
                            "dpad",
                            10f,
                            80f,
                            upBinding = TouchBindings.BTN_ACCELERATE,
                            downBinding = TouchBindings.BTN_REVERSE,
                            leftBinding = TouchBindings.BTN_BANK_LEFT,
                            rightBinding = TouchBindings.BTN_BANK_RIGHT,
                        ),
                    ),
            )
        val filtered = controllerFilteredTouchLayout(custom, axis, "d2")
        assertTrue(filtered.sliders.isEmpty())
        assertEquals(custom.axisRegions, filtered.axisRegions)
        assertEquals(custom.dpads, filtered.dpads)
    }
}
