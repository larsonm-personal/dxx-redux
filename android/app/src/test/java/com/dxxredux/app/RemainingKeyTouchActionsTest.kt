package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class RemainingKeyTouchActionsTest {
    @Test
    fun d2LayoutWithoutBindingsGetsOverflowActions() {
        val actions = remainingKeyTouchActions(TouchLayout(name = "Empty"), gameVariant = "d2")
        val bindings = actions.map { it.binding }

        assertEquals(TouchBindings.BTN_AUTOMAP, bindings.first())
        assertTrue(TouchBindings.BTN_HEADLIGHT in bindings)
        assertTrue(TouchBindings.META_QUICK_SAVE !in bindings)
        assertTrue(TouchBindings.META_QUICK_LOAD !in bindings)
        assertTrue(TouchBindings.META_GAME_MENU !in bindings)
        assertTrue(TouchBindings.META_PAUSE !in bindings)
        assertTrue(TouchBindings.META_MULTIPLAYER_HUD !in bindings)
        assertTrue(TouchBindings.META_DROP_FLAG !in bindings)
        assertTrue(TouchBindings.META_GYRO_TOGGLE !in bindings)
        assertTrue(TouchBindings.META_RETURN_TO_LAUNCHER !in bindings)
        assertTrue(TouchBindings.META_WEAPON_1 !in bindings)
        assertTrue(TouchBindings.META_WEAPON_10 !in bindings)
        assertTrue(TouchBindings.BTN_CYCLE_PRIMARY in bindings)
        assertTrue(TouchBindings.BTN_CYCLE_SECONDARY in bindings)
        assertTrue(TouchBindings.META_GUIDE_BOT_MENU in bindings)
        assertTrue(TouchBindings.META_GUIDE_RELEASE_CONTROL !in bindings)
        assertTrue(TouchBindings.META_CYCLE_LEFT_VIEW !in bindings)
        assertTrue(TouchBindings.META_CYCLE_RIGHT_VIEW !in bindings)
        assertTrue(TouchBindings.META_REWIND in bindings)
        assertEquals("Next Primary", actions.first { it.binding == TouchBindings.BTN_CYCLE_PRIMARY }.label)
        assertEquals("Next Secondary", actions.first { it.binding == TouchBindings.BTN_CYCLE_SECONDARY }.label)
    }

    @Test
    fun cockpitWindowCycleActionsUseSharedMetaLabels() {
        assertEquals("Cycle Left View", TouchBindings.META_BUTTON_LABELS[TouchBindings.META_CYCLE_LEFT_VIEW])
        assertEquals("Cycle Right View", TouchBindings.META_BUTTON_LABELS[TouchBindings.META_CYCLE_RIGHT_VIEW])
        assertEquals(TouchBindings.META_CYCLE_LEFT_VIEW, TouchBindings.nameToBinding("Cycle Left View"))
        assertEquals(TouchBindings.META_CYCLE_RIGHT_VIEW, TouchBindings.nameToBinding("Meta: Cycle Right View"))
    }

    @Test
    fun rewindOverflowActionUsesSharedMetaLabel() {
        val action =
            remainingKeyTouchActions(TouchLayout(name = "Empty"), gameVariant = "d1")
                .first { it.binding == TouchBindings.META_REWIND }

        assertEquals("Rewind", action.label)
        assertEquals("Rewind", TouchBindings.META_BUTTON_LABELS[TouchBindings.META_REWIND])
    }

    @Test
    fun configuredGyroAddsGyroToggleToOverflow() {
        val actions =
            remainingKeyTouchActions(
                TouchLayout(name = "Gyro", gyro = GyroConfig(enabled = true)),
                gameVariant = "d2",
            )
        val bindings = actions.map { it.binding }

        assertTrue(TouchBindings.META_GYRO_TOGGLE in bindings)
    }

    @Test
    fun d2MultiplayerLayoutGetsMultiplayerOnlyOverflowActions() {
        val actions =
            remainingKeyTouchActions(
                TouchLayout(name = "Empty"),
                gameVariant = "d2",
                isMultiplayerGame = true,
            )
        val bindings = actions.map { it.binding }

        assertTrue(TouchBindings.META_MULTIPLAYER_HUD in bindings)
        assertTrue(TouchBindings.META_DROP_FLAG in bindings)
        assertTrue(TouchBindings.META_QUICK_SAVE !in bindings)
        assertTrue(TouchBindings.META_PAUSE !in bindings)
    }

    @Test
    fun controllerAdminActionsArePrependedOnlyForGamepadOnlyMode() {
        val keyActions = remainingKeyTouchActions(TouchLayout(name = "Empty"), gameVariant = "d2")
        val controllerAdminActions =
            listOf(
                RemainingTouchAction(label = "Warp: Ace", adminAction = TouchOverlayView.ADMIN_WARP),
                RemainingTouchAction(label = "Accept: Blaze", adminAction = TouchOverlayView.ADMIN_ACCEPT_JOIN),
            )

        val gamepadActions =
            remainingActionsWithControllerAdminActions(
                keyActions = keyActions,
                gamepadOnlyMode = true,
                controllerAdminActions = controllerAdminActions,
            )
        val touchActions =
            remainingActionsWithControllerAdminActions(
                keyActions = keyActions,
                gamepadOnlyMode = false,
                controllerAdminActions = controllerAdminActions,
            )

        assertEquals(TouchOverlayView.ADMIN_WARP, gamepadActions[0].adminAction)
        assertEquals(TouchOverlayView.ADMIN_ACCEPT_JOIN, gamepadActions[1].adminAction)
        assertEquals(TouchBindings.BTN_AUTOMAP, gamepadActions[2].binding)
        assertEquals(keyActions, touchActions)
    }

    @Test
    fun d1LayoutExcludesD2OnlyActions() {
        val actions = remainingKeyTouchActions(TouchLayout(name = "Empty"), gameVariant = "d1")
        val bindings = actions.map { it.binding }

        assertTrue(TouchBindings.META_WEAPON_1 !in bindings)
        assertTrue(TouchBindings.BTN_CYCLE_PRIMARY in bindings)
        assertTrue(TouchBindings.BTN_CYCLE_SECONDARY in bindings)
        assertTrue(TouchBindings.META_PAUSE !in bindings)
        assertTrue(TouchBindings.BTN_HEADLIGHT !in bindings)
        assertTrue(TouchBindings.META_DROP_MARKER !in bindings)
        assertTrue(TouchBindings.META_GUIDE_BOT_MENU !in bindings)
        assertTrue(TouchBindings.META_CYCLE_LEFT_VIEW !in bindings)
        assertTrue(TouchBindings.META_CYCLE_RIGHT_VIEW !in bindings)
    }

    @Test
    fun alreadyBoundHeadlightAndLauncherAreFilteredOut() {
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
                            doubleTapBinding = TouchBindings.META_RETURN_TO_LAUNCHER,
                        ),
                    ),
            )

        val actions = remainingKeyTouchActions(layout, gameVariant = "d2")
        val bindings = actions.map { it.binding }

        assertTrue(TouchBindings.BTN_HEADLIGHT !in bindings)
        assertTrue(TouchBindings.META_RETURN_TO_LAUNCHER !in bindings)
        assertTrue(TouchBindings.BTN_FIRE_FLARE in bindings)
    }

    @Test
    fun alreadyBoundRewindIsFilteredOut() {
        val layout =
            TouchLayout(
                name = "Rewind",
                buttons =
                    listOf(
                        ButtonControl(
                            id = "rewind",
                            xPct = 15f,
                            yPct = 15f,
                            binding = TouchBindings.META_REWIND,
                        ),
                    ),
            )

        val bindings = remainingKeyTouchActions(layout, gameVariant = "d2").map { it.binding }

        assertTrue(TouchBindings.META_REWIND !in bindings)
    }

    @Test
    fun controllerBoundActionsAreFilteredOut() {
        val bindings =
            remainingKeyTouchActions(
                TouchLayout(name = "Empty"),
                gameVariant = "d2",
                extraBoundBindings = setOf(TouchBindings.META_REWIND, TouchBindings.BTN_HEADLIGHT),
            ).map { it.binding }

        assertTrue(TouchBindings.META_REWIND !in bindings)
        assertTrue(TouchBindings.BTN_HEADLIGHT !in bindings)
        assertTrue(TouchBindings.BTN_AUTOMAP in bindings)
    }

    @Test
    fun controllerBoundActionsFilterOnlyWhenWorkingControllerIsInUse() {
        val controllerBindings = setOf(TouchBindings.META_REWIND, TouchBindings.BTN_HEADLIGHT)
        val withoutController =
            remainingKeyTouchActions(
                TouchLayout(name = "Empty"),
                gameVariant = "d2",
                controllerBoundBindings = controllerBindings,
                workingControllerInUse = false,
            ).map { it.binding }
        val withController =
            remainingKeyTouchActions(
                TouchLayout(name = "Empty"),
                gameVariant = "d2",
                controllerBoundBindings = controllerBindings,
                workingControllerInUse = true,
            ).map { it.binding }

        assertTrue(TouchBindings.META_REWIND in withoutController)
        assertTrue(TouchBindings.BTN_HEADLIGHT in withoutController)
        assertTrue(TouchBindings.META_REWIND !in withController)
        assertTrue(TouchBindings.BTN_HEADLIGHT !in withController)
        assertTrue(TouchBindings.BTN_AUTOMAP in withController)
    }

    @Test
    fun controllerConfigBindingLabelsMapToActionBindings() {
        val bindings =
            controllerConfigBoundActionBindings(
                mapOf(
                    "A" to "Rewind",
                    "B" to "Headlight",
                    "RS_X" to "Turn L/R",
                    "X" to "Energy->Shield",
                    "Y" to "Energy\u2192Shield",
                ),
            )

        assertTrue(TouchBindings.META_REWIND in bindings)
        assertTrue(TouchBindings.BTN_HEADLIGHT in bindings)
        assertTrue(TouchBindings.BTN_ENERGY_SHIELD in bindings)
        assertTrue(TouchBindings.AXIS_RIGHT_X !in bindings)
        assertEquals(TouchBindings.BTN_ENERGY_SHIELD, TouchBindings.nameToBinding("Energy\u2192Shield"))
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
        assertTrue(TouchBindings.BTN_CYCLE_PRIMARY !in bindings)
        assertTrue(TouchBindings.BTN_CYCLE_SECONDARY !in bindings)
    }

    @Test
    fun completeDirectWeaponBindingsHideCycleFallbacks() {
        val layout =
            TouchLayout(
                name = "Direct Weapons",
                buttons =
                    (
                        primaryWeaponBindings() +
                            secondaryWeaponBindings()
                    ).mapIndexed { index, binding -> button(binding, "weapon$index") },
            )

        val bindings = remainingKeyTouchActions(layout, gameVariant = "d2").map { it.binding }

        assertTrue(TouchBindings.BTN_CYCLE_PRIMARY !in bindings)
        assertTrue(TouchBindings.BTN_CYCLE_SECONDARY !in bindings)
        assertTrue(TouchBindings.META_WEAPON_1 !in bindings)
        assertTrue(TouchBindings.META_WEAPON_10 !in bindings)
    }

    @Test
    fun partialDirectWeaponBindingsStillShowCycleFallbacks() {
        val layout =
            TouchLayout(
                name = "Partial Weapons",
                buttons =
                    listOf(
                        button(TouchBindings.META_WEAPON_1, "primary1"),
                        button(TouchBindings.META_WEAPON_6, "secondary1"),
                    ),
            )

        val bindings = remainingKeyTouchActions(layout, gameVariant = "d2").map { it.binding }

        assertTrue(TouchBindings.BTN_CYCLE_PRIMARY in bindings)
        assertTrue(TouchBindings.BTN_CYCLE_SECONDARY in bindings)
        assertTrue(TouchBindings.META_WEAPON_2 !in bindings)
        assertTrue(TouchBindings.META_WEAPON_7 !in bindings)
    }

    @Test
    fun cycleWeaponButtonsHideCycleFallbacks() {
        val layout =
            TouchLayout(
                name = "Cycles",
                buttons =
                    listOf(
                        button(TouchBindings.BTN_CYCLE_PRIMARY, "primary"),
                        button(TouchBindings.BTN_CYCLE_SECONDARY, "secondary"),
                    ),
            )

        val bindings = remainingKeyTouchActions(layout, gameVariant = "d2").map { it.binding }

        assertTrue(TouchBindings.BTN_CYCLE_PRIMARY !in bindings)
        assertTrue(TouchBindings.BTN_CYCLE_SECONDARY !in bindings)
    }

    @Test
    fun controllerWeaponAccessCountsOnlyWhenWorkingControllerIsInUse() {
        val controllerBindings = primaryWeaponBindings().toSet() + TouchBindings.BTN_CYCLE_SECONDARY
        val withoutController =
            remainingKeyTouchActions(
                TouchLayout(name = "Empty"),
                gameVariant = "d2",
                controllerBoundBindings = controllerBindings,
                workingControllerInUse = false,
            ).map { it.binding }
        val withController =
            remainingKeyTouchActions(
                TouchLayout(name = "Empty"),
                gameVariant = "d2",
                controllerBoundBindings = controllerBindings,
                workingControllerInUse = true,
            ).map { it.binding }

        assertTrue(TouchBindings.BTN_CYCLE_PRIMARY in withoutController)
        assertTrue(TouchBindings.BTN_CYCLE_SECONDARY in withoutController)
        assertTrue(TouchBindings.BTN_CYCLE_PRIMARY !in withController)
        assertTrue(TouchBindings.BTN_CYCLE_SECONDARY !in withController)
    }

    @Test
    fun toggleBombLabelShowsCurrentBombSelection() {
        val actions =
            remainingKeyTouchActions(
                TouchLayout(name = "Bombs"),
                gameVariant = "d2",
                weaponState = weaponState(currentBomb = 7),
            )

        val toggleBomb = actions.first { it.binding == TouchBindings.BTN_TOGGLE_BOMB }

        assertEquals("Toggle Bomb [current: Smart Mine]", toggleBomb.label)
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

    private fun weaponState(
        playerFlags: Int = 0,
        currentBomb: Int = -1,
    ) =
        WeaponState(
            primaryFlags = 0,
            secondaryFlags = 0,
            playerFlags = playerFlags,
            primaryAmmo = IntArray(10),
            secondaryAmmo = IntArray(10),
            primaryAmmoMax = IntArray(10),
            secondaryAmmoMax = IntArray(10),
            currentPrimary = 0,
            currentSecondary = 0,
            currentBomb = currentBomb,
        )

    private fun button(
        binding: Int,
        id: String = "button$binding",
    ) =
        ButtonControl(
            id = id,
            xPct = 10f,
            yPct = 10f,
            binding = binding,
        )

    private fun primaryWeaponBindings() =
        listOf(
            TouchBindings.META_WEAPON_1,
            TouchBindings.META_WEAPON_2,
            TouchBindings.META_WEAPON_3,
            TouchBindings.META_WEAPON_4,
            TouchBindings.META_WEAPON_5,
        )

    private fun secondaryWeaponBindings() =
        listOf(
            TouchBindings.META_WEAPON_6,
            TouchBindings.META_WEAPON_7,
            TouchBindings.META_WEAPON_8,
            TouchBindings.META_WEAPON_9,
            TouchBindings.META_WEAPON_10,
        )
}
