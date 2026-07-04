package com.dxxredux.app

internal data class RemainingTouchAction(
    val label: String,
    val binding: Int = -1,
    val adminAction: Int? = null,
)

private val remainingBaseActionBindings =
    listOf(
        TouchBindings.BTN_AUTOMAP,
        TouchBindings.BTN_FIRE_FLARE,
        TouchBindings.BTN_DROP_BOMB,
        TouchBindings.BTN_HEADLIGHT,
        TouchBindings.BTN_TOGGLE_BOMB,
        TouchBindings.BTN_ENERGY_SHIELD,
        TouchBindings.META_DROP_MARKER,
        TouchBindings.META_DEMO_RECORD_TOGGLE,
        TouchBindings.META_REWIND,
    )

private val remainingMultiplayerActionBindings =
    listOf(
        TouchBindings.META_MULTIPLAYER_HUD,
        TouchBindings.META_DROP_FLAG,
    )

private val remainingPrimaryWeaponBindings =
    listOf(
        TouchBindings.META_WEAPON_1,
        TouchBindings.META_WEAPON_2,
        TouchBindings.META_WEAPON_3,
        TouchBindings.META_WEAPON_4,
        TouchBindings.META_WEAPON_5,
    )

private val remainingSecondaryWeaponBindings =
    listOf(
        TouchBindings.META_WEAPON_6,
        TouchBindings.META_WEAPON_7,
        TouchBindings.META_WEAPON_8,
        TouchBindings.META_WEAPON_9,
        TouchBindings.META_WEAPON_10,
    )

private val primaryWeaponCycleBindings = setOf(TouchBindings.BTN_CYCLE_PRIMARY)

private val secondaryWeaponCycleBindings = setOf(TouchBindings.BTN_CYCLE_SECONDARY)

private val remainingGuideBindings =
    listOf(
        TouchBindings.META_GUIDE_BOT_MENU,
        TouchBindings.META_GUIDE_NEXT_GOAL,
        TouchBindings.META_GUIDE_FIND_SECRET,
    )

internal fun touchLayoutBoundActionBindings(layout: TouchLayout): Set<Int> {
    val bindings = linkedSetOf<Int>()

    layout.buttons.forEach { button ->
        bindings.add(button.binding)
        if (button.longPressEnabled && button.longPressBinding >= 0) {
            bindings.add(button.longPressBinding)
        }
    }
    layout.sticks.forEach { stick ->
        if (stick.buttonMode) {
            bindings.add(stick.negXBinding)
            bindings.add(stick.posXBinding)
            bindings.add(stick.negYBinding)
            bindings.add(stick.posYBinding)
        }
        if (stick.doubleTapBinding >= 0) {
            bindings.add(stick.doubleTapBinding)
        }
    }
    layout.radialMenus.forEach { radial ->
        radial.segments.forEach { segment ->
            if (segment.bindingType == "action" || TouchBindings.isMetaAction(segment.binding)) {
                bindings.add(segment.binding)
            }
        }
        if (radial.centerBinding >= 0) {
            bindings.add(radial.centerBinding)
        }
    }
    layout.dpads.forEach { dpad ->
        bindings.add(dpad.upBinding)
        bindings.add(dpad.downBinding)
        bindings.add(dpad.leftBinding)
        bindings.add(dpad.rightBinding)
    }

    return bindings
}

internal fun controllerConfigBoundActionBindings(bindings: Map<String, String>): Set<Int> =
    bindings.values.mapNotNull { TouchBindings.nameToBinding(it) }.toSet()

private fun remainingCandidateBindings(
    layout: TouchLayout,
    isMultiplayerGame: Boolean,
): List<Int> =
    buildList {
        addAll(remainingBaseActionBindings)
        if (layout.gyro.enabled) add(TouchBindings.META_GYRO_TOGGLE)
        if (isMultiplayerGame) addAll(remainingMultiplayerActionBindings)
        if (layout.radialMenus.none { it.id == "Guide" }) addAll(remainingGuideBindings)
    }

private fun needsWeaponCycleFallback(
    layout: TouchLayout,
    boundBindings: Set<Int>,
    wheelId: String,
    directBindings: List<Int>,
    cycleBindings: Set<Int>,
): Boolean =
    layout.radialMenus.none { it.id == wheelId } &&
        directBindings.any { it !in boundBindings } &&
        cycleBindings.none { it in boundBindings }

internal fun remainingActionLabel(
    binding: Int,
    gameVariant: String,
    weaponState: WeaponState? = null,
): String =
    when (binding) {
        TouchBindings.BTN_CYCLE_PRIMARY -> {
            "Next Primary"
        }

        TouchBindings.BTN_CYCLE_SECONDARY -> {
            "Next Secondary"
        }

        TouchBindings.BTN_TOGGLE_BOMB -> {
            val currentBomb = currentBombName(gameVariant, weaponState)
            if (currentBomb != null) {
                "Toggle Bomb [current: $currentBomb]"
            } else {
                TouchBindings.ALL_BUTTON_LABELS[binding]
                    ?: TouchBindings.bindingToName(binding).removePrefix("Meta: ")
            }
        }

        else -> {
            TouchBindings.ALL_BUTTON_LABELS[binding]
                ?: TouchBindings.bindingToName(binding).removePrefix("Meta: ")
        }
    }

internal fun remainingActionUsesHeldActivation(binding: Int): Boolean = binding == TouchBindings.BTN_ENERGY_SHIELD

internal fun remainingTouchActionStartsHeldActivation(action: RemainingTouchAction): Boolean =
    action.adminAction == null && remainingActionUsesHeldActivation(action.binding)

internal fun remainingKeyTouchActions(
    layout: TouchLayout,
    gameVariant: String,
    isMultiplayerGame: Boolean = false,
    weaponState: WeaponState? = null,
    controllerBoundBindings: Set<Int> = emptySet(),
    workingControllerInUse: Boolean = false,
    extraBoundBindings: Set<Int> = emptySet(),
): List<RemainingTouchAction> {
    val boundBindings =
        touchLayoutBoundActionBindings(layout) +
            extraBoundBindings +
            if (workingControllerInUse) controllerBoundBindings else emptySet()
    val candidateBindings =
        buildList {
            addAll(remainingCandidateBindings(layout, isMultiplayerGame))
            if (
                needsWeaponCycleFallback(
                    layout,
                    boundBindings,
                    "PriWpn",
                    remainingPrimaryWeaponBindings,
                    primaryWeaponCycleBindings,
                )
            ) {
                add(TouchBindings.BTN_CYCLE_PRIMARY)
            }
            if (
                needsWeaponCycleFallback(
                    layout,
                    boundBindings,
                    "SecWpn",
                    remainingSecondaryWeaponBindings,
                    secondaryWeaponCycleBindings,
                )
            ) {
                add(TouchBindings.BTN_CYCLE_SECONDARY)
            }
        }
    return candidateBindings
        .filter { binding ->
            gameVariant != "d1" ||
                (binding !in TouchBindings.D2_ONLY_BUTTONS && binding !in TouchBindings.D2_ONLY_META_ACTIONS)
        }.filter { it !in boundBindings }
        .distinct()
        .map { RemainingTouchAction(remainingActionLabel(it, gameVariant, weaponState), binding = it) }
}

internal fun remainingActionsWithControllerAdminActions(
    keyActions: List<RemainingTouchAction>,
    gamepadOnlyMode: Boolean,
    controllerAdminActions: List<RemainingTouchAction>,
): List<RemainingTouchAction> =
    if (gamepadOnlyMode && controllerAdminActions.isNotEmpty()) {
        controllerAdminActions + keyActions
    } else {
        keyActions
    }
