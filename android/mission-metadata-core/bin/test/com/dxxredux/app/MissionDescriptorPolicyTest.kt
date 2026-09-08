package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class MissionDescriptorPolicyTest {
    @Test fun parsesLevelsSecretsModesAndEnhancedName() {
        val parsed = MissionDescriptorPolicy.parse("sample.mn2", """
            name = sample
            xname = Better Name
            normal = yes
            coop = true
            num_levels = 2
            one.rl2
            two.rl2
            num_secrets = 1
            secret.rl2,2
        """.trimIndent())
        assertEquals("Better Name", parsed.displayName)
        assertEquals(listOf("one.rl2", "two.rl2"), parsed.levelNames)
        assertEquals(listOf("secret.rl2"), parsed.secretLevelNames)
        assertEquals(listOf(2), parsed.secretLevelOrigins)
        assertEquals(setOf("normal", "coop"), parsed.modeFlags)
        assertEquals("d2", parsed.game)
        assertTrue(parsed.valid)
    }
}
