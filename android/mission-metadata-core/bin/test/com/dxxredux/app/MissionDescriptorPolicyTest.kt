package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class MissionDescriptorPolicyTest {
    @Test fun parsesD2xNameWithoutOrdinaryName() {
        val parsed =
            MissionDescriptorPolicy.parse(
                "antho1-3.mn2",
                """
                d2x-name = Anthology (Split)
                type = normal
                num_levels = 3
                antho-d1.rl2
                antho-d2.rl2
                antho-d3.rl2
                """.trimIndent(),
            )
        assertTrue(parsed.valid)
        assertEquals("Anthology (Split)", parsed.displayName)
        assertEquals(listOf("antho-d1.rl2", "antho-d2.rl2", "antho-d3.rl2"), parsed.levelNames)
    }

    @Test fun parsesLevelsSecretsModesAndEnhancedName() {
        val parsed =
            MissionDescriptorPolicy.parse(
                "sample.mn2",
                """
                name = sample
                xname = Better Name
                normal = yes
                coop = true
                num_levels = 2
                one.rl2
                two.rl2
                num_secrets = 1
                secret.rl2,2
                """.trimIndent(),
            )
        assertEquals("Better Name", parsed.displayName)
        assertEquals(listOf("one.rl2", "two.rl2"), parsed.levelNames)
        assertEquals(listOf("secret.rl2"), parsed.secretLevelNames)
        assertEquals(listOf(2), parsed.secretLevelOrigins)
        assertEquals(setOf("normal", "coop"), parsed.modeFlags)
        assertEquals("d2", parsed.game)
        assertTrue(parsed.valid)
    }
}
