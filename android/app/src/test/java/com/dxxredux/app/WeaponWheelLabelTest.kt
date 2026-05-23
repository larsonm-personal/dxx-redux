package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class WeaponWheelLabelTest {
    @Test
    fun currentLaserWheelLabelShowsLevel() {
        val d1State = weaponState(currentPrimary = 0, laserLevel = 2)
        val d2State = weaponState(currentPrimary = 5, laserLevel = 5)

        assertEquals("Laser\nlvl 3", weaponWheelCurrentLabel("d1", d1State, isPrimary = true))
        assertEquals("Laser\nlvl 6", weaponWheelCurrentLabel("d2", d2State, isPrimary = true))
    }

    @Test
    fun d2PrimaryWheelShowsCurrentWeaponForSelectedSlot() {
        val state =
            weaponState(
                currentPrimary = 7,
                primaryFlags = flagsOf(2, 7),
                primaryLastWasSuperFlags = intArrayOf(0, 0, 1, 0, 0),
            )

        assertEquals("Helix", weaponWheelSlotLabel("d2", state, isPrimary = true, slotIndex = 2))
    }

    @Test
    fun d2PrimaryWheelFallsBackToBaseWhenSuperNotOwned() {
        val state =
            weaponState(
                primaryFlags = flagsOf(2),
                primaryLastWasSuperFlags = intArrayOf(0, 0, 1, 0, 0),
            )

        assertEquals("Spreadfire", weaponWheelSlotLabel("d2", state, isPrimary = true, slotIndex = 2))
    }

    @Test
    fun d2SecondaryWheelShowsPredictedSuperWeaponForSlot() {
        val ammo = IntArray(10).apply { this[6] = 4 }
        val state =
            weaponState(
                secondaryFlags = flagsOf(1, 6),
                secondaryAmmo = ammo,
                secondaryLastWasSuperFlags = intArrayOf(0, 1, 0, 0, 0),
            )

        assertEquals("Guided", weaponWheelSlotLabel("d2", state, isPrimary = false, slotIndex = 1))
    }

    private fun weaponState(
        primaryFlags: Int = 0,
        secondaryFlags: Int = 0,
        currentPrimary: Int = 0,
        currentSecondary: Int = 0,
        laserLevel: Int = 0,
        primaryAmmo: IntArray = IntArray(10),
        secondaryAmmo: IntArray = IntArray(10),
        primaryLastWasSuperFlags: IntArray = IntArray(5),
        secondaryLastWasSuperFlags: IntArray = IntArray(5),
    ) =
        WeaponState(
            primaryFlags = primaryFlags,
            secondaryFlags = secondaryFlags,
            playerFlags = 0,
            primaryAmmo = primaryAmmo,
            secondaryAmmo = secondaryAmmo,
            primaryAmmoMax = IntArray(10),
            secondaryAmmoMax = IntArray(10),
            currentPrimary = currentPrimary,
            currentSecondary = currentSecondary,
            currentBomb = -1,
            laserLevel = laserLevel,
            primaryLastWasSuperFlags = primaryLastWasSuperFlags,
            secondaryLastWasSuperFlags = secondaryLastWasSuperFlags,
        )

    private fun flagsOf(vararg indices: Int): Int {
        var flags = 0
        indices.forEach { flags = flags or (1 shl it) }
        return flags
    }
}