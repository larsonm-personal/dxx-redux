package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class ControllerConfigSerializationTest {
    @Test
    fun buildJoySettingsArray_preservesD2HighButtonSlots() {
        val settings = buildJoySettingsArray(buildJoyPairs(emptyMap(), emptySet(), "d2"), "d2")

        assertEquals(56, settings.size)
        assertEquals(104, settings[4].toInt() and 0xFF)
        assertEquals(127, settings[27].toInt() and 0xFF)
        assertEquals(128, settings[28].toInt() and 0xFF)
        assertEquals(129, settings[29].toInt() and 0xFF)
        assertEquals(150, settings[50].toInt() and 0xFF)
        assertEquals(152, settings[52].toInt() and 0xFF)
        assertEquals(154, settings[54].toInt() and 0xFF)
    }

    @Test
    fun buildJoySettingsArray_keepsD1ButtonBounds() {
        val settings = buildJoySettingsArray(buildJoyPairs(emptyMap(), emptySet(), "d1"), "d1")

        assertEquals(50, settings.size)
        assertEquals(104, settings[4].toInt() and 0xFF)
        assertEquals(127, settings[27].toInt() and 0xFF)
        assertEquals(144, settings[44].toInt() and 0xFF)
        assertEquals(145, settings[45].toInt() and 0xFF)
    }
}
