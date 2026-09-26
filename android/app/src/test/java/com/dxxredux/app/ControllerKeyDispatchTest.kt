package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ControllerKeyDispatchTest {
    @Test
    fun releaseStaysWithPressWhenDestinationChanges() {
        val router = ControllerKeyDispatch()
        val edges = mutableListOf<String>()
        var destination = "game"
        fun press(repeat: Int = 0) = router.press(1, repeat, false) {
            val owner = destination
            val send: (Boolean) -> Unit = { down -> edges.add("$owner:$down") }
            send
        }
        press()
        destination = "menu"
        press()
        press(1)
        assertTrue(router.release(1))
        assertFalse(router.release(1))
        press()
        router.releaseAll()
        assertEquals(listOf("game:true", "game:false", "menu:true", "menu:false"), edges)
    }

    @Test
    fun directionsRepeatButDuplicateHardwareEdgesAndOrphanRepeatsDoNot() {
        val router = ControllerKeyDispatch()
        val edges = mutableListOf<Boolean>()
        fun press(repeat: Int) = router.press(22, repeat, true) { { edges.add(it) } }
        press(1)
        press(0)
        press(0)
        press(1)
        router.releaseAll()
        assertEquals(listOf(true, true, false), edges)
    }
}
