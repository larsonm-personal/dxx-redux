package com.dxxredux.app

internal data class RemainingTouchAction(
    val binding: Int,
    val label: String,
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

private val remainingGuideBindings =
    listOf(
        TouchBindings.META_GUIDE_BOT_MENU,
        TouchBindings.META_GUIDE_RELEASE_CONTROL,
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
        val hasPrimaryWheel = layout.radialMenus.any { it.id == "PriWpn" }
        val hasSecondaryWheel = layout.radialMenus.any { it.id == "SecWpn" }

        addAll(remainingBaseActionBindings)
        if (layout.gyro.enabled) add(TouchBindings.META_GYRO_TOGGLE)
        if (isMultiplayerGame) addAll(remainingMultiplayerActionBindings)
        if (!hasPrimaryWheel) {
            add(TouchBindings.BTN_CYCLE_PRIMARY)
            addAll(remainingPrimaryWeaponBindings)
        }
        if (!hasSecondaryWheel) {
            add(TouchBindings.BTN_CYCLE_SECONDARY)
            addAll(remainingSecondaryWeaponBindings)
        }
        if (layout.radialMenus.none { it.id == "Guide" }) addAll(remainingGuideBindings)
    }

internal fun remainingActionLabel(
    binding: Int,
    gameVariant: String,
    weaponState: WeaponState? = null,
): String =
    when (binding) {
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

internal fun remainingKeyTouchActions(
    layout: TouchLayout,
    gameVariant: String,
    isMultiplayerGame: Boolean = false,
    weaponState: WeaponState? = null,
    extraBoundBindings: Set<Int> = emptySet(),
): List<RemainingTouchAction> {
    val boundBindings = touchLayoutBoundActionBindings(layout) + extraBoundBindings
    return remainingCandidateBindings(layout, isMultiplayerGame)
        .filter { binding ->
            gameVariant != "d1" ||
                (binding !in TouchBindings.D2_ONLY_BUTTONS && binding !in TouchBindings.D2_ONLY_META_ACTIONS)
        }.filter { it !in boundBindings }
        .distinct()
        .map { RemainingTouchAction(it, remainingActionLabel(it, gameVariant, weaponState)) }
}
