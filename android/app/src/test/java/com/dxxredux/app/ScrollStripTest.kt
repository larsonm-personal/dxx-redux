package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ScrollStripTest {
    @Test
    fun dragEndpointsSelectFirstAndLastItems() {
        assertEquals(0f, scrollStripFractionalIndex(100f, 100f, 2, 5), 0.001f)
        assertEquals(4f, scrollStripFractionalIndex(-100f, 100f, 2, 5), 0.001f)
        assertEquals(2f, scrollStripFractionalIndex(0f, 100f, 2, 5), 0.001f)
    }

    @Test
    fun centerItemReachesConfiguredScale() {
        assertEquals(2f, scrollStripItemScale(2, 2f, 2f), 0.001f)
        assertEquals(1f, scrollStripItemScale(2, 2.5f, 2f), 0.001f)
        assertTrue(scrollStripItemScale(2, 2.25f, 2f) in 1f..2f)
    }

    @Test
    fun verticalPlacementStillUsesScreenWidth() {
        assertEquals(40f, clampScrollStripCenterPct(0f, 20f, 2000f, 500f, vertical = true), 0.001f)
        assertEquals(60f, clampScrollStripCenterPct(100f, 20f, 2000f, 500f, vertical = true), 0.001f)
    }

    @Test
    fun d2RowsUseFixedTiersAndHideUnavailableWeapons() {
        val ammo = IntArray(10).apply {
            this[0] = 2
            this[5] = 3
            this[6] = 0
        }
        val state =
            WeaponState(
                primaryFlags = flagsOf(0, 1, 6, 7),
                secondaryFlags = flagsOf(0, 5, 6),
                playerFlags = 0,
                primaryAmmo = IntArray(10),
                secondaryAmmo = ammo,
                primaryAmmoMax = IntArray(10),
                secondaryAmmoMax = IntArray(10),
                currentPrimary = 1,
                currentSecondary = 0,
                currentBomb = 0,
            )

        val primary = weaponStripRows("d2", state, isPrimary = true)
        assertEquals(listOf(0, 6, 7), primary.main.map { it.weaponIndex })
        assertEquals(listOf(1), primary.alternate.map { it.weaponIndex })

        val secondary = weaponStripRows("d2", state, isPrimary = false)
        assertEquals(listOf(5), secondary.main.map { it.weaponIndex })
        assertEquals(listOf(0), secondary.alternate.map { it.weaponIndex })
    }

    private fun flagsOf(vararg indices: Int): Int = indices.fold(0) { flags, index -> flags or (1 shl index) }
}
