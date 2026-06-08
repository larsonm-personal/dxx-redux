package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TouchCheatInjectionTest {
    @Test
    fun d2LamerAliasesAreDeduplicatedFromMenu() {
        val visibleCodes = TouchBindings.CHEATS_D2.map { it.code }.toSet()

        assertTrue(visibleCodes.contains("gabbagabbahey"))
        assertFalse(visibleCodes.contains("motherlode"))
        assertFalse(visibleCodes.contains("currygoat"))
        assertFalse(visibleCodes.contains("zingermans"))
        assertFalse(visibleCodes.contains("eatangelos"))
        assertFalse(visibleCodes.contains("ericaanne"))
        assertFalse(visibleCodes.contains("joshuaakira"))
        assertFalse(visibleCodes.contains("whammazoom"))
    }

    @Test
    fun visibleCheatMenusHaveNoDuplicateCodesOrLabels() {
        assertCheatsUnique(TouchBindings.CHEATS_D1)
        assertCheatsUnique(TouchBindings.CHEATS_D2)
    }

    @Test
    fun d2DescriptionsIncludeSourceDerivedMissingTableEntries() {
        val descriptions = TouchBindings.CHEATS_D2.associate { it.code to it.label }

        assertEquals("Mark path to exit", descriptions.getValue("flash"))
        assertEquals("Toggle ghost physics", descriptions.getValue("astral"))
        assertEquals("Toggle turbo mode", descriptions.getValue("buggin"))
        assertEquals("Kill robots; repeats can kill buddy", descriptions.getValue("spaniard"))
        assertEquals(
            "Collect powerups, kill robots, blow reactor, move to exit",
            descriptions.getValue("delshiftb"),
        )
    }

    @Test
    fun d1DescriptionsIncludeSourceDerivedMissingTableEntries() {
        val descriptions = TouchBindings.CHEATS_D1.associate { it.code to it.label }

        assertEquals("Enable cheat codes", descriptions.getValue("gabbagabbahey"))
        assertEquals("Toggle psychedelic walls", descriptions.getValue("bittersweet"))
        assertEquals("Mega weapons, 200 shields and energy", descriptions.getValue("porgys"))
        assertEquals(
            "Collect powerups, kill robots, blow reactor, move to exit",
            descriptions.getValue("poboys"),
        )
    }

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

    private fun assertCheatsUnique(cheats: List<TouchBindings.CheatDef>) {
        assertEquals(cheats.size, cheats.map { it.code }.toSet().size)
        assertEquals(cheats.size, cheats.map { it.label }.toSet().size)
    }
}
