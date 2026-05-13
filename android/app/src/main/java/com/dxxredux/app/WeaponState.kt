package com.dxxredux.app

/**
 * Weapon inventory from nativeGetWeaponState().
 * Array layout: [priFlags, secFlags, playerFlags, priAmmo[0..9], secAmmo[0..9], priMax[0..9], secMax[0..9],
 *                currentPrimary, currentSecondary, currentBomb]
 * Shared constants with C (jni_main.c / player.h):
 *   PLAYER_FLAGS_AMMO_RACK = 128
 *   PLAYER_FLAGS_HEADLIGHT_ON = 16384
 * Indices 43-45 duplicated with jni_main.c
 */
data class WeaponState(
    val primaryFlags: Int,
    val secondaryFlags: Int,
    val playerFlags: Int,
    val primaryAmmo: IntArray,
    val secondaryAmmo: IntArray,
    val primaryAmmoMax: IntArray,
    val secondaryAmmoMax: IntArray,
    val currentPrimary: Int,
    val currentSecondary: Int,
    val currentBomb: Int,
) {
    fun hasPrimary(index: Int) = index in 0..9 && (primaryFlags and (1 shl index)) != 0

    fun hasSecondary(index: Int) = index in 0..9 && (secondaryFlags and (1 shl index)) != 0

    val isHeadlightOn: Boolean
        get() = (playerFlags and PLAYER_FLAGS_HEADLIGHT_ON) != 0

    companion object {
        const val PLAYER_FLAGS_AMMO_RACK = 128
        const val PLAYER_FLAGS_HEADLIGHT_ON = 16384

        fun fromArray(arr: IntArray): WeaponState {
            require(arr.size >= 45)
            return WeaponState(
                primaryFlags = arr[0],
                secondaryFlags = arr[1],
                playerFlags = arr[2],
                primaryAmmo = arr.sliceArray(3..12),
                secondaryAmmo = arr.sliceArray(13..22),
                primaryAmmoMax = arr.sliceArray(23..32),
                secondaryAmmoMax = arr.sliceArray(33..42),
                currentPrimary = arr[43],
                currentSecondary = arr[44],
                currentBomb = arr.getOrElse(45) { -1 },
            )
        }
    }
}
