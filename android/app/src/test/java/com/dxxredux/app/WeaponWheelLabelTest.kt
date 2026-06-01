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
    fun d2PrimaryWheelSeparatesCurrentWeaponFromSlotTarget() {
        val state =
            weaponState(
                currentPrimary = 7,
                primaryFlags = flagsOf(2, 7),
                primaryLastWasSuperFlags = intArrayOf(0, 0, 1, 0, 0),
            )

        assertEquals("Helix", weaponWheelCurrentLabel("d2", state, isPrimary = true))
        assertEquals("Spreadfire", weaponWheelSlotLabel("d2", state, isPrimary = true, slotIndex = 2))
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

    @Test
    fun laserStatusUsesEnergyThresholds() {
        assertEquals(AmmoStatusColor.GREEN, primaryStatus(energy = 71, primary = 0))
        assertEquals(AmmoStatusColor.YELLOW, primaryStatus(energy = 70, primary = 0))
        assertEquals(AmmoStatusColor.YELLOW, primaryStatus(energy = 31, primary = 0))
        assertEquals(AmmoStatusColor.RED, primaryStatus(energy = 30, primary = 0))
    }

    @Test
    fun vulcanStatusUsesDisplayedRoundThresholds() {
        val max = IntArray(10).apply { this[1] = 10000 }

        assertEquals(
            AmmoStatusColor.GREEN,
            primaryStatus(primary = 1, primaryAmmo = intArrayWith(1, 5001), primaryAmmoMax = max),
        )
        assertEquals(
            AmmoStatusColor.YELLOW,
            primaryStatus(primary = 1, primaryAmmo = intArrayWith(1, 5000), primaryAmmoMax = max),
        )
        assertEquals(
            AmmoStatusColor.YELLOW,
            primaryStatus(primary = 1, primaryAmmo = intArrayWith(1, 2001), primaryAmmoMax = max),
        )
        assertEquals(
            AmmoStatusColor.RED,
            primaryStatus(primary = 1, primaryAmmo = intArrayWith(1, 2000), primaryAmmoMax = max),
        )
    }

    @Test
    fun countedWeaponStatusUsesSharedFractionsAndCountText() {
        val max = IntArray(10).apply { this[3] = 10 }

        assertEquals(
            WeaponAmmoStatus(AmmoStatusColor.GREEN, "x6"),
            secondaryPresentation(3, secondaryAmmo = intArrayWith(3, 6), secondaryAmmoMax = max).ammoStatus,
        )
        assertEquals(
            AmmoStatusColor.YELLOW,
            secondaryPresentation(3, secondaryAmmo = intArrayWith(3, 3), secondaryAmmoMax = max).ammoStatus?.color,
        )
        assertEquals(
            AmmoStatusColor.RED,
            secondaryPresentation(3, secondaryAmmo = intArrayWith(3, 2), secondaryAmmoMax = max).ammoStatus?.color,
        )
    }

    @Test
    fun d2SecondaryWheelPresentationUsesPredictedPairedWeaponAmmo() {
        val ammo =
            IntArray(10).apply {
                this[0] = 2
                this[5] = 7
            }
        val max =
            IntArray(10).apply {
                this[0] = 10
                this[5] = 10
            }
        val state =
            weaponState(
                secondaryFlags = flagsOf(0, 5),
                currentSecondary = 1,
                secondaryAmmo = ammo,
                secondaryAmmoMax = max,
                secondaryLastWasSuperFlags = intArrayOf(1, 0, 0, 0, 0),
            )

        val presentation = weaponWheelSlotPresentation("d2", state, isPrimary = false, slotIndex = 0)
        assertEquals("Flash", presentation.label)
        assertEquals(WeaponAmmoStatus(AmmoStatusColor.GREEN, "x7"), presentation.ammoStatus)
    }

    @Test
    fun d2CurrentSecondaryPresentationStaysOnActuallySelectedWeapon() {
        val ammo =
            IntArray(10).apply {
                this[0] = 2
                this[5] = 7
            }
        val max =
            IntArray(10).apply {
                this[0] = 10
                this[5] = 10
            }
        val state =
            weaponState(
                secondaryFlags = flagsOf(0, 5),
                currentSecondary = 5,
                secondaryAmmo = ammo,
                secondaryAmmoMax = max,
                secondaryLastWasSuperFlags = intArrayOf(1, 0, 0, 0, 0),
            )

        val slotPresentation = weaponWheelSlotPresentation("d2", state, isPrimary = false, slotIndex = 0)
        val currentPresentation = weaponWheelCurrentPresentation("d2", state, isPrimary = false)

        assertEquals("Concussion", slotPresentation.label)
        assertEquals(WeaponAmmoStatus(AmmoStatusColor.RED, "x2"), slotPresentation.ammoStatus)
        assertEquals("Flash", currentPresentation?.label)
        assertEquals(WeaponAmmoStatus(AmmoStatusColor.GREEN, "x7"), currentPresentation?.ammoStatus)
    }

    private fun weaponState(
        primaryFlags: Int = 0,
        secondaryFlags: Int = 0,
        currentPrimary: Int = 0,
        currentSecondary: Int = 0,
        laserLevel: Int = 0,
        primaryAmmo: IntArray = IntArray(10),
        secondaryAmmo: IntArray = IntArray(10),
        primaryAmmoMax: IntArray = IntArray(10),
        secondaryAmmoMax: IntArray = IntArray(10),
        primaryLastWasSuperFlags: IntArray = IntArray(5),
        secondaryLastWasSuperFlags: IntArray = IntArray(5),
        playerFlags: Int = 0,
        energy: Int = 0,
    ) = WeaponState(
        primaryFlags = primaryFlags,
        secondaryFlags = secondaryFlags,
        playerFlags = playerFlags,
        primaryAmmo = primaryAmmo,
        secondaryAmmo = secondaryAmmo,
        primaryAmmoMax = primaryAmmoMax,
        secondaryAmmoMax = secondaryAmmoMax,
        currentPrimary = currentPrimary,
        currentSecondary = currentSecondary,
        currentBomb = -1,
        laserLevel = laserLevel,
        primaryLastWasSuperFlags = primaryLastWasSuperFlags,
        secondaryLastWasSuperFlags = secondaryLastWasSuperFlags,
        energy = energy,
    )

    private fun flagsOf(vararg indices: Int): Int {
        var flags = 0
        indices.forEach { flags = flags or (1 shl it) }
        return flags
    }

    private fun intArrayWith(
        index: Int,
        value: Int,
    ) = IntArray(10).apply { this[index] = value }

    private fun primaryStatus(
        energy: Int = 0,
        primary: Int,
        primaryAmmo: IntArray = IntArray(10),
        primaryAmmoMax: IntArray = IntArray(10),
    ) = weaponWheelCurrentPresentation(
        "d2",
        weaponState(
            currentPrimary = primary,
            energy = energy,
            primaryAmmo = primaryAmmo,
            primaryAmmoMax = primaryAmmoMax,
        ),
        isPrimary = true,
    )?.ammoStatus?.color

    private fun secondaryPresentation(
        secondary: Int,
        secondaryAmmo: IntArray,
        secondaryAmmoMax: IntArray,
    ) = weaponWheelCurrentPresentation(
        "d2",
        weaponState(
            currentSecondary = secondary,
            secondaryAmmo = secondaryAmmo,
            secondaryAmmoMax = secondaryAmmoMax,
        ),
        isPrimary = false,
    )!!
}
