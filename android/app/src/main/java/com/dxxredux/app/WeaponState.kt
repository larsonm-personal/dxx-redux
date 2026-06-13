package com.dxxredux.app

/**
 * Weapon inventory from nativeGetWeaponState().
 * Array layout: [priFlags, secFlags, playerFlags, priAmmo[0..9], secAmmo[0..9], priMax[0..9], secMax[0..9],
 *                currentPrimary, currentSecondary, currentBomb, laserLevel,
 *                primaryLastWasSuper[0..4], secondaryLastWasSuper[0..4], energy, afterburnerChargePct]
 * Shared constants with C (jni_main.c / player.h):
 *   PLAYER_FLAGS_AMMO_RACK = 128
 *   PLAYER_FLAGS_HEADLIGHT_ON = 16384
 * Indices 43-58 duplicated with jni_main.c
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
    val laserLevel: Int = 0,
    val primaryLastWasSuperFlags: IntArray = IntArray(PAIRED_SLOT_COUNT),
    val secondaryLastWasSuperFlags: IntArray = IntArray(PAIRED_SLOT_COUNT),
    val energy: Int = 0,
    val afterburnerChargePct: Int = 100,
) {
    fun hasPrimary(index: Int) = index in 0..9 && (primaryFlags and (1 shl index)) != 0

    fun hasSecondary(index: Int) = index in 0..9 && (secondaryFlags and (1 shl index)) != 0

    fun primarySlotPrefersSuper(slotIndex: Int) =
        slotIndex in primaryLastWasSuperFlags.indices && primaryLastWasSuperFlags[slotIndex] != 0

    fun secondarySlotPrefersSuper(slotIndex: Int) =
        slotIndex in secondaryLastWasSuperFlags.indices && secondaryLastWasSuperFlags[slotIndex] != 0

    fun secondarySlotHasAmmo(index: Int) = index in secondaryAmmo.indices && secondaryAmmo[index] > 0

    val isHeadlightOn: Boolean
        get() = (playerFlags and PLAYER_FLAGS_HEADLIGHT_ON) != 0

    companion object {
        private const val PAIRED_SLOT_COUNT = 5
        const val PLAYER_FLAGS_AMMO_RACK = 128
        const val PLAYER_FLAGS_HEADLIGHT_ON = 16384

        fun fromArray(arr: IntArray): WeaponState {
            require(arr.size >= 46)
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
                laserLevel = arr.getOrElse(46) { 0 },
                primaryLastWasSuperFlags =
                    IntArray(PAIRED_SLOT_COUNT) { slot ->
                        arr.getOrElse(47 + slot) { 0 }
                    },
                secondaryLastWasSuperFlags =
                    IntArray(PAIRED_SLOT_COUNT) { slot ->
                        arr.getOrElse(47 + PAIRED_SLOT_COUNT + slot) { 0 }
                    },
                energy = arr.getOrElse(57) { 0 },
                afterburnerChargePct = arr.getOrElse(58) { 100 }.coerceIn(0, 100),
            )
        }
    }
}
