package com.dxxredux.app

import kotlin.math.hypot

private const val SECONDARY_PROXIMITY_INDEX = 2
private const val SECONDARY_SMART_MINE_INDEX = 7
private const val WEAPON_WHEEL_SLOT_COUNT = 5
private const val D2_SUPER_WEAPON_OFFSET = 5

private val d1PrimarySelectionNames =
    listOf(
        "Laser",
        "Vulcan",
        "Spreadfire",
        "Plasma",
        "Fusion",
    )

private val d2PrimarySelectionNames =
    listOf(
        "Laser",
        "Vulcan",
        "Spreadfire",
        "Plasma",
        "Fusion",
        "Super Laser",
        "Gauss",
        "Helix",
        "Phoenix",
        "Omega",
    )

private val d1SecondarySelectionNames =
    listOf(
        "Concussion",
        "Homing",
        "Proximity Bomb",
        "Smart Missile",
        "Mega Missile",
    )

private val d2SecondarySelectionNames =
    listOf(
        "Concussion",
        "Homing",
        "Proximity Bomb",
        "Smart Missile",
        "Mega Missile",
        "Flash Missile",
        "Guided Missile",
        "Smart Mine",
        "Mercury Missile",
        "Earthshaker",
    )

private val d1PrimaryWheelSelectionNames =
    listOf(
        "Laser",
        "Vulcan",
        "Spreadfire",
        "Plasma",
        "Fusion",
    )

private val d2PrimaryWheelSelectionNames =
    listOf(
        "Laser",
        "Vulcan",
        "Spreadfire",
        "Plasma",
        "Fusion",
        "Laser",
        "Gauss",
        "Helix",
        "Phoenix",
        "Omega",
    )

private val d1SecondaryWheelSelectionNames =
    listOf(
        "Concussion",
        "Homing",
        "Proximity",
        "Smart",
        "Mega",
    )

private val d2SecondaryWheelSelectionNames =
    listOf(
        "Concussion",
        "Homing",
        "Proximity",
        "Smart",
        "Mega",
        "Flash",
        "Guided",
        "Smart\nMine",
        "Mercury",
        "Earthshaker",
    )

private fun buttonUsesBindingIndicator(
    button: ButtonControl,
    binding: Int,
): Boolean =
    button.binding == binding ||
        (button.longPressEnabled && button.longPressBinding == binding)

internal fun buttonUsesGyroToggleIndicator(button: ButtonControl): Boolean =
    buttonUsesBindingIndicator(button, TouchBindings.META_GYRO_TOGGLE)

internal fun buttonUsesHeadlightIndicator(button: ButtonControl): Boolean =
    buttonUsesBindingIndicator(button, TouchBindings.BTN_HEADLIGHT)

internal fun currentBombName(
    gameVariant: String,
    weaponState: WeaponState?,
): String? =
    if (gameVariant != "d2" || weaponState == null) {
        null
    } else {
        when (weaponState.currentBomb) {
            SECONDARY_PROXIMITY_INDEX -> "Proximity Bomb"
            SECONDARY_SMART_MINE_INDEX -> "Smart Mine"
            else -> null
        }
    }

internal fun defaultWeaponWheelSlotLabel(
    gameVariant: String,
    isPrimary: Boolean,
    slotIndex: Int,
): String {
    val names =
        when {
            isPrimary && gameVariant == "d1" -> d1PrimaryWheelSelectionNames
            isPrimary -> d2PrimaryWheelSelectionNames
            gameVariant == "d1" -> d1SecondaryWheelSelectionNames
            else -> d2SecondaryWheelSelectionNames
        }
    return names.getOrElse(slotIndex) {
        if (isPrimary) {
            "Laser"
        } else {
            "Concussion"
        }
    }
}

internal fun laserWheelLabel(weaponState: WeaponState?): String {
    val level = weaponState?.laserLevel?.plus(1)?.coerceAtLeast(1)
    return if (level != null) {
        "Laser\nlvl $level"
    } else {
        "Laser"
    }
}

internal data class WeaponWheelPresentation(
    val label: String,
    val ammoStatus: WeaponAmmoStatus?,
)

private fun currentWeaponWheelSlotIndex(
    gameVariant: String,
    currentWeapon: Int,
): Int? =
    when {
        currentWeapon in 0 until WEAPON_WHEEL_SLOT_COUNT -> {
            currentWeapon
        }

        gameVariant == "d2" &&
            currentWeapon in D2_SUPER_WEAPON_OFFSET until D2_SUPER_WEAPON_OFFSET + WEAPON_WHEEL_SLOT_COUNT -> {
            currentWeapon - D2_SUPER_WEAPON_OFFSET
        }

        else -> {
            null
        }
    }

private fun resolvePairedWheelWeaponIndex(
    slotIndex: Int,
    currentWeapon: Int,
    lastWasSuper: Boolean,
    baseSelectable: Boolean,
    superSelectable: Boolean,
    baseOwned: Boolean,
    superOwned: Boolean,
): Int? {
    if (currentWeapon == slotIndex || currentWeapon == slotIndex + D2_SUPER_WEAPON_OFFSET) {
        val alternateIndex =
            if (currentWeapon == slotIndex) {
                slotIndex + D2_SUPER_WEAPON_OFFSET
            } else {
                slotIndex
            }
        if (alternateIndex == slotIndex) {
            if (baseSelectable || baseOwned) return alternateIndex
        } else if (superSelectable || superOwned) {
            return alternateIndex
        }
        return currentWeapon
    }

    fun isSelectable(index: Int) = if (index == slotIndex) baseSelectable else superSelectable

    fun isOwned(index: Int) = if (index == slotIndex) baseOwned else superOwned

    var preferredIndex =
        if (lastWasSuper) {
            slotIndex + D2_SUPER_WEAPON_OFFSET
        } else {
            slotIndex
        }
    if (isSelectable(preferredIndex)) {
        return preferredIndex
    }

    preferredIndex =
        if (preferredIndex == slotIndex) {
            slotIndex + D2_SUPER_WEAPON_OFFSET
        } else {
            slotIndex
        }
    if (isSelectable(preferredIndex)) {
        return preferredIndex
    }
    if (isOwned(preferredIndex)) {
        return preferredIndex
    }

    preferredIndex =
        if (preferredIndex == slotIndex) {
            slotIndex + D2_SUPER_WEAPON_OFFSET
        } else {
            slotIndex
        }
    return preferredIndex.takeIf(::isOwned)
}

internal fun weaponWheelSlotWeaponIndex(
    gameVariant: String,
    weaponState: WeaponState?,
    isPrimary: Boolean,
    slotIndex: Int,
): Int? {
    if (slotIndex !in 0 until WEAPON_WHEEL_SLOT_COUNT) {
        return null
    }
    if (weaponState == null || gameVariant != "d2") {
        return slotIndex
    }

    return if (isPrimary) {
        resolvePairedWheelWeaponIndex(
            slotIndex = slotIndex,
            currentWeapon = weaponState.currentPrimary,
            lastWasSuper = weaponState.primarySlotPrefersSuper(slotIndex),
            baseSelectable = weaponState.hasPrimary(slotIndex),
            superSelectable = weaponState.hasPrimary(slotIndex + D2_SUPER_WEAPON_OFFSET),
            baseOwned = weaponState.hasPrimary(slotIndex),
            superOwned = weaponState.hasPrimary(slotIndex + D2_SUPER_WEAPON_OFFSET),
        )
    } else {
        resolvePairedWheelWeaponIndex(
            slotIndex = slotIndex,
            currentWeapon = weaponState.currentSecondary,
            lastWasSuper = weaponState.secondarySlotPrefersSuper(slotIndex),
            baseSelectable = weaponState.hasSecondary(slotIndex) && weaponState.secondarySlotHasAmmo(slotIndex),
            superSelectable =
                weaponState.hasSecondary(slotIndex + D2_SUPER_WEAPON_OFFSET) &&
                    weaponState.secondarySlotHasAmmo(slotIndex + D2_SUPER_WEAPON_OFFSET),
            baseOwned = weaponState.hasSecondary(slotIndex),
            superOwned = weaponState.hasSecondary(slotIndex + D2_SUPER_WEAPON_OFFSET),
        )
    }
}

internal fun weaponWheelSlotPresentation(
    gameVariant: String,
    weaponState: WeaponState?,
    isPrimary: Boolean,
    slotIndex: Int,
): WeaponWheelPresentation {
    val fallback = defaultWeaponWheelSlotLabel(gameVariant, isPrimary, slotIndex)
    if (slotIndex !in 0 until WEAPON_WHEEL_SLOT_COUNT) {
        return WeaponWheelPresentation(fallback, null)
    }
    if (isPrimary && slotIndex == 0) {
        return WeaponWheelPresentation(
            laserWheelLabel(weaponState),
            weaponState?.let { weaponAmmoStatus(gameVariant, it, true, slotIndex) },
        )
    }

    val resolvedIndex = weaponWheelSlotWeaponIndex(gameVariant, weaponState, isPrimary, slotIndex)
    val names =
        when {
            isPrimary && gameVariant == "d2" -> d2PrimaryWheelSelectionNames
            isPrimary -> d1PrimaryWheelSelectionNames
            gameVariant == "d2" -> d2SecondaryWheelSelectionNames
            else -> d1SecondaryWheelSelectionNames
        }
    val label = resolvedIndex?.let { names.getOrNull(it) } ?: fallback
    val ammoStatus =
        if (weaponState != null && resolvedIndex != null) {
            weaponAmmoStatus(gameVariant, weaponState, isPrimary, resolvedIndex)
        } else {
            null
        }
    return WeaponWheelPresentation(label, ammoStatus)
}

internal fun weaponWheelSlotLabel(
    gameVariant: String,
    weaponState: WeaponState?,
    isPrimary: Boolean,
    slotIndex: Int,
): String = weaponWheelSlotPresentation(gameVariant, weaponState, isPrimary, slotIndex).label

internal fun weaponWheelCurrentLabel(
    gameVariant: String,
    weaponState: WeaponState?,
    isPrimary: Boolean,
): String? = weaponWheelCurrentPresentation(gameVariant, weaponState, isPrimary)?.label

internal fun weaponWheelCurrentPresentation(
    gameVariant: String,
    weaponState: WeaponState?,
    isPrimary: Boolean,
): WeaponWheelPresentation? {
    val ws = weaponState ?: return null
    val currentIndex = if (isPrimary) ws.currentPrimary else ws.currentSecondary
    val currentSlot = currentWeaponWheelSlotIndex(gameVariant, currentIndex) ?: return null
    val names =
        when {
            isPrimary && gameVariant == "d1" -> d1PrimaryWheelSelectionNames
            isPrimary -> d2PrimaryWheelSelectionNames
            gameVariant == "d1" -> d1SecondaryWheelSelectionNames
            else -> d2SecondaryWheelSelectionNames
        }
    val label =
        if (isPrimary && currentSlot == 0) {
            laserWheelLabel(ws)
        } else {
            names.getOrNull(currentIndex) ?: return null
        }
    return WeaponWheelPresentation(label, weaponAmmoStatus(gameVariant, ws, isPrimary, currentIndex))
}

private fun currentPrimaryName(
    gameVariant: String,
    weaponState: WeaponState?,
): String? {
    val currentPrimary = weaponState?.currentPrimary ?: return null
    val names = if (gameVariant == "d1") d1PrimarySelectionNames else d2PrimarySelectionNames
    return names.getOrNull(currentPrimary)
}

private fun currentSecondaryName(
    gameVariant: String,
    weaponState: WeaponState?,
): String? {
    val currentSecondary = weaponState?.currentSecondary ?: return null
    val names = if (gameVariant == "d1") d1SecondarySelectionNames else d2SecondarySelectionNames
    return names.getOrNull(currentSecondary)
}

internal fun buttonDisplayLabel(
    button: ButtonControl,
    gameVariant: String,
    weaponState: WeaponState?,
): String {
    val action =
        when (button.binding) {
            TouchBindings.BTN_DROP_BOMB -> "Drop"

            TouchBindings.BTN_TOGGLE_BOMB,
            TouchBindings.BTN_CYCLE_PRIMARY,
            TouchBindings.BTN_CYCLE_SECONDARY,
            -> "Cycle"

            TouchBindings.BTN_FIRE_PRIMARY,
            TouchBindings.BTN_FIRE_SECONDARY,
            -> "Fire"

            else -> null
        }
    val selection =
        when (button.binding) {
            TouchBindings.BTN_DROP_BOMB,
            TouchBindings.BTN_TOGGLE_BOMB,
            -> currentBombName(gameVariant, weaponState)

            TouchBindings.BTN_FIRE_PRIMARY,
            TouchBindings.BTN_CYCLE_PRIMARY,
            -> currentPrimaryName(gameVariant, weaponState)

            TouchBindings.BTN_FIRE_SECONDARY,
            TouchBindings.BTN_CYCLE_SECONDARY,
            -> currentSecondaryName(gameVariant, weaponState)

            else -> null
        }

    return if (action != null && selection != null) "$action\n$selection" else button.label
}

internal fun buttonHasActiveIndicatorState(
    button: ButtonControl,
    gameVariant: String,
    weaponState: WeaponState?,
    gyroConfigured: Boolean,
    gyroActiveInGame: Boolean,
): Boolean {
    val gyroActive = buttonUsesGyroToggleIndicator(button) && gyroConfigured && gyroActiveInGame
    val headlightActive =
        buttonUsesHeadlightIndicator(button) &&
            gameVariant == "d2" &&
            weaponState?.isHeadlightOn == true
    return gyroActive || headlightActive
}

internal fun dragZoneButtonLatchAllowed(
    gameVariant: String,
    binding: Int,
    pointerId: Int,
    toggle: Boolean,
): Boolean {
    if (gameVariant == "d1" && binding in TouchBindings.D2_ONLY_BUTTONS) return false
    if (pointerId >= 0 || toggle) return false
    return when (binding) {
        TouchBindings.BTN_CHEATS_MENU,
        TouchBindings.BTN_GYRO_RECENTER,
        TouchBindings.BTN_AUTOMAP,
        -> false

        else -> true
    }
}

internal fun buttonExtendsDragZoneStart(
    zoneLeft: Float,
    zoneTop: Float,
    zoneRight: Float,
    zoneBottom: Float,
    buttonCenterX: Float,
    buttonCenterY: Float,
    buttonRadius: Float,
    touchX: Float,
    touchY: Float,
): Boolean =
    buttonCenterX in zoneLeft..zoneRight &&
        buttonCenterY in zoneTop..zoneBottom &&
        hypot(touchX - buttonCenterX, touchY - buttonCenterY) <= buttonRadius * 1.3f
