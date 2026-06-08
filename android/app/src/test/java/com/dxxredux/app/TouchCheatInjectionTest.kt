package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TouchCheatInjectionTest {
    @Test
    fun d1NonEnableCheatInjectsEnableCodeFirst() {
        val injection =
            touchCheatCodeToInject(
                gameVariant = "d1",
                code = "scourge",
                d1CheatsEnabled = false,
            )

        assertEquals("gabbagabbaheyscourge", injection.code)
        assertTrue(injection.d1CheatsEnabled)
    }

    @Test
    fun d1EnableCheatDoesNotDuplicateEnableCode() {
        val injection =
            touchCheatCodeToInject(
                gameVariant = "d1",
                code = "gabbagabbahey",
                d1CheatsEnabled = false,
            )

        assertEquals("gabbagabbahey", injection.code)
        assertTrue(injection.d1CheatsEnabled)
    }

    @Test
    fun d1EnabledStateSendsSelectedCodeOnly() {
        val injection =
            touchCheatCodeToInject(
                gameVariant = "d1",
                code = "mitzi",
                d1CheatsEnabled = true,
            )

        assertEquals("mitzi", injection.code)
        assertTrue(injection.d1CheatsEnabled)
    }

    @Test
    fun d2CheatsAreUnchanged() {
        val injection =
            touchCheatCodeToInject(
                gameVariant = "d2",
                code = "honestbob",
                d1CheatsEnabled = false,
            )

        assertEquals("honestbob", injection.code)
        assertFalse(injection.d1CheatsEnabled)
    }
}
