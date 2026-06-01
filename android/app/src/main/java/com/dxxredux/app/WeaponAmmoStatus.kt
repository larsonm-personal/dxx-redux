package com.dxxredux.app

internal const val LASER_STATUS_GREEN_ENERGY = 70
internal const val LASER_STATUS_YELLOW_ENERGY = 30
internal const val VULCAN_STATUS_GREEN_ROUNDS = 5000
internal const val VULCAN_STATUS_YELLOW_ROUNDS = 2000
internal const val DEFAULT_AMMO_STATUS_GREEN_FRACTION = 0.50f
internal const val DEFAULT_AMMO_STATUS_YELLOW_FRACTION = 0.25f

private const val D2_SUPER_WEAPON_OFFSET = 5
private const val LASER_INDEX = 0
private const val VULCAN_INDEX = 1
private const val SUPER_LASER_INDEX = 5
private const val GAUSS_INDEX = 6
private const val DISPLAYED_VULCAN_ROUNDS_AT_BASE_MAX = 10000

internal enum class AmmoStatusColor { GREEN, YELLOW, RED }

internal data class WeaponAmmoStatus(
    val color: AmmoStatusColor,
    val countText: String?,
)

internal fun weaponAmmoStatus(
    gameVariant: String,
    weaponState: WeaponState,
    isPrimary: Boolean,
    weaponIndex: Int,
): WeaponAmmoStatus? {
    if (isPrimary) {
        return when {
            isLaserIndex(gameVariant, weaponIndex) -> {
                WeaponAmmoStatus(
                    thresholdStatus(weaponState.energy, LASER_STATUS_GREEN_ENERGY, LASER_STATUS_YELLOW_ENERGY),
                    null,
                )
            }

            isVulcanLikeIndex(gameVariant, weaponIndex) -> {
                WeaponAmmoStatus(
                    thresholdStatus(
                        vulcanDisplayRounds(weaponState),
                        VULCAN_STATUS_GREEN_ROUNDS,
                        VULCAN_STATUS_YELLOW_ROUNDS,
                    ),
                    null,
                )
            }

            else -> {
                null
            }
        }
    }

    if (weaponIndex !in weaponState.secondaryAmmo.indices) {
        return null
    }
    val count = weaponState.secondaryAmmo[weaponIndex]
    val max = weaponState.secondaryAmmoMax.getOrElse(weaponIndex) { 0 }
    return WeaponAmmoStatus(fractionStatus(count, max), "x$count")
}

internal fun ammoStatusColorArgb(
    status: AmmoStatusColor,
    alpha: Int,
): Int {
    val clampedAlpha = alpha.coerceIn(0, 255)
    val rgb =
        when (status) {
            AmmoStatusColor.GREEN -> 0x34C759
            AmmoStatusColor.YELLOW -> 0xFFD60A
            AmmoStatusColor.RED -> 0xFF453A
        }
    return (clampedAlpha shl 24) or rgb
}

private fun isLaserIndex(
    gameVariant: String,
    weaponIndex: Int,
) = weaponIndex == LASER_INDEX || (gameVariant == "d2" && weaponIndex == SUPER_LASER_INDEX)

private fun isVulcanLikeIndex(
    gameVariant: String,
    weaponIndex: Int,
) = weaponIndex == VULCAN_INDEX || (gameVariant == "d2" && weaponIndex == GAUSS_INDEX)

private fun thresholdStatus(
    value: Int,
    greenAbove: Int,
    yellowAbove: Int,
) = when {
    value > greenAbove -> AmmoStatusColor.GREEN
    value > yellowAbove -> AmmoStatusColor.YELLOW
    else -> AmmoStatusColor.RED
}

private fun fractionStatus(
    count: Int,
    max: Int,
): AmmoStatusColor {
    if (max <= 0) {
        return AmmoStatusColor.RED
    }
    val fraction = count.toFloat() / max.toFloat()
    return when {
        fraction > DEFAULT_AMMO_STATUS_GREEN_FRACTION -> AmmoStatusColor.GREEN
        fraction > DEFAULT_AMMO_STATUS_YELLOW_FRACTION -> AmmoStatusColor.YELLOW
        else -> AmmoStatusColor.RED
    }
}

private fun vulcanDisplayRounds(weaponState: WeaponState): Int {
    val rawAmmo = weaponState.primaryAmmo.getOrElse(VULCAN_INDEX) { 0 }
    val effectiveMax = weaponState.primaryAmmoMax.getOrElse(VULCAN_INDEX) { 0 }
    val rackMultiplier =
        if ((weaponState.playerFlags and WeaponState.PLAYER_FLAGS_AMMO_RACK) != 0) {
            2
        } else {
            1
        }
    val baseMax = effectiveMax / rackMultiplier
    if (baseMax <= 0) {
        return 0
    }
    return rawAmmo * DISPLAYED_VULCAN_ROUNDS_AT_BASE_MAX / baseMax
}
