package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class MissionDescriptorPolicyTest {
    @Test fun rejectsInvalidSecretOriginTokensWithoutPromotingLaterOnes() {
        for ((descriptor, level) in listOf("msn" to "rdl", "mn2" to "rl2")) {
            for (token in listOf("", "bad", "2147483648", "-2147483649", "4294967297", "0", "-1", "3")) {
                for (origins in listOf(token, "$token,2", "1,$token", "1,$token,2")) {
                    val parsed =
                        MissionDescriptorPolicy.parse(
                            "sample.$descriptor",
                            "name = Sample\nnum_levels = 2\none.$level\ntwo.$level\nnum_secrets = 1\nsecret.$level,$origins\n",
                        )
                    assertFalse("$descriptor origins '$origins'", parsed.valid)
                    assertTrue(parsed.secretLevelNames.isEmpty())
                    assertTrue(parsed.secretLevelOrigins.isEmpty())
                }
            }
        }
    }

    @Test fun preservesFirstValidSecretOriginIncludingMultipleOrigins() {
        for ((descriptor, level) in listOf("msn" to "rdl", "mn2" to "rl2")) {
            for ((origins, first) in listOf("1" to 1, "2" to 2, "1,2" to 1, "2,1" to 2, " 2 , 1 " to 2)) {
                val parsed =
                    MissionDescriptorPolicy.parse(
                        "sample.$descriptor",
                        "name = Sample\nnum_levels = 2\none.$level\ntwo.$level\nnum_secrets = 1\nsecret.$level,$origins\n",
                    )
                assertTrue(parsed.valid)
                assertEquals(listOf("secret.$level"), parsed.secretLevelNames)
                assertEquals(listOf(first), parsed.secretLevelOrigins)
            }
        }
    }

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
