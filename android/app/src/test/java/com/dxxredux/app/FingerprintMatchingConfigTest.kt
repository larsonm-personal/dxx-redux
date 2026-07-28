package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import java.io.File

class FingerprintMatchingConfigTest {
    @Test
    fun checkedInConfigProvidesCanonicalThreshold() {
        val config = parseFingerprintMatchingConfig(assetFile().readText())

        assertEquals(0.65f, config.matchThreshold)
        assertEquals(0.10f, config.durationTolerance)
    }

    @Test
    fun explicitThresholdBoundariesArePreserved() {
        assertEquals(0.40f, parseFingerprintMatchingConfig(config("0.40")).matchThreshold)
        assertEquals(0.65f, parseFingerprintMatchingConfig(config("0.65")).matchThreshold)
    }

    @Test
    fun invalidConfigurationFailsClosed() {
        listOf(
            "{}",
            """{"match_threshold":0.65}""",
            """{"match_threshold":"0.65","duration_tolerance":0.10}""",
            """{"match_threshold":NaN,"duration_tolerance":0.10}""",
            """{"match_threshold":Infinity,"duration_tolerance":0.10}""",
            """{"match_threshold":0.65junk,"duration_tolerance":0.10}""",
            config("0"),
            config("1.01"),
            config("0.65") + " trailing",
        ).forEach { raw ->
            assertThrows(IllegalArgumentException::class.java) {
                parseFingerprintMatchingConfig(raw)
            }
        }
    }

    private fun config(threshold: String): String =
        """{"match_threshold":$threshold,"duration_tolerance":0.10}"""

    private fun assetFile(): File {
        val candidates =
            listOf(
                File("src/main/assets/fingerprint_config.json5"),
                File("app/src/main/assets/fingerprint_config.json5"),
                File("android/app/src/main/assets/fingerprint_config.json5"),
            )
        return candidates.firstOrNull(File::isFile)
            ?: error("fingerprint_config.json5 not found from ${File(".").absolutePath}")
    }
}
