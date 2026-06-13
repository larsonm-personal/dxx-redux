package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class WeaponStateTest {
    @Test
    fun fromArrayReadsAfterburnerChargePercent() {
        val arr = IntArray(59)
        arr[58] = 42

        val state = WeaponState.fromArray(arr)

        assertEquals(42, state.afterburnerChargePct)
    }

    @Test
    fun fromArrayDefaultsMissingAfterburnerChargeToFull() {
        val state = WeaponState.fromArray(IntArray(58))

        assertEquals(100, state.afterburnerChargePct)
    }

    @Test
    fun fromArrayClampsAfterburnerChargePercent() {
        val low = IntArray(59).apply { this[58] = -1 }
        val high = IntArray(59).apply { this[58] = 120 }

        assertEquals(0, WeaponState.fromArray(low).afterburnerChargePct)
        assertEquals(100, WeaponState.fromArray(high).afterburnerChargePct)
    }
}
