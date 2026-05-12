package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RemainingKeyTouchActionsTest {
    @Test
    fun d2LayoutWithoutBindingsGetsOverflowActions() {
        val actions = remainingKeyTouchActions(TouchLayout(name = "Empty"), gameVariant = "d2")
        val bindings = actions.map { it.binding }

        assertEquals(TouchBindings.META_PAUSE, bindings.first())
        assertTrue(TouchBindings.BTN_HEADLIGHT in bindings)
        assertTrue(TouchBindings.META_QUICK_SAVE in bindings)
        assertTrue(TouchBindings.META_WEAPON_1 in bindings)
        assertTrue(TouchBindings.META_WEAPON_10 in bindings)
        assertTrue(TouchBindings.META_GUIDE_BOT_MENU in bindings)
    }

    @Test
    fun d1LayoutExcludesD2OnlyActions() {
        val actions = remainingKeyTouchActions(TouchLayout(name = "Empty"), gameVariant = "d1")
        val bindings = actions.map { it.binding }

        assertTrue(TouchBindings.META_PAUSE in bindings)
        assertTrue(TouchBindings.META_WEAPON_1 in bindings)
        assertTrue(TouchBindings.BTN_HEADLIGHT !in bindings)
        assertTrue(TouchBindings.META_DROP_MARKER !in bindings)
        assertTrue(TouchBindings.META_GUIDE_BOT_MENU !in bindings)
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
        val bindings = actions.map { it.binding }

        assertTrue(TouchBindings.BTN_HEADLIGHT !in bindings)
        assertTrue(TouchBindings.META_PAUSE !in bindings)
        assertTrue(TouchBindings.META_QUICK_SAVE in bindings)
    }

    @Test
    fun weaponRadialsFilterDirectWeaponActions() {
        val layout =
            TouchLayout(
                name = "Weapons",
                radialMenus =
                    listOf(
                        RadialMenuControl(id = "PriWpn", xPct = 10f, yPct = 10f, segments = emptyList()),
                        RadialMenuControl(id = "SecWpn", xPct = 20f, yPct = 10f, segments = emptyList()),
                    ),
            )

        val bindings = remainingKeyTouchActions(layout, gameVariant = "d2").map { it.binding }

        assertTrue(TouchBindings.META_WEAPON_1 !in bindings)
        assertTrue(TouchBindings.META_WEAPON_10 !in bindings)
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

    @Test
    fun boundBindingsIncludeMetaRadialSegmentsEvenWhenImportedAsKeycodeSegments() {
        val layout =
            TouchLayout(
                radialMenus =
                    listOf(
                        RadialMenuControl(
                            id = "Guide",
                            xPct = 50f,
                            yPct = 50f,
                            segments =
                                listOf(
                                    RadialSegment("Energy", TouchBindings.META_GUIDE_FIND_ENERGY),
                                ),
                        ),
                    ),
            )

        val bindings = touchLayoutBoundActionBindings(layout)

        assertTrue(TouchBindings.META_GUIDE_FIND_ENERGY in bindings)
    }
}
