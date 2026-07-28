package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class AndroidGameFileExtensionsTest {
    private fun nativePolicySource(): String {
        val candidates =
            listOf(
                File("src/main/cpp/extract/game_file_extensions.c"),
                File("app/src/main/cpp/extract/game_file_extensions.c"),
                File("android/app/src/main/cpp/extract/game_file_extensions.c"),
            )
        return candidates.firstOrNull { it.isFile }?.readText()
            ?: error("Cannot locate native extension policy from ${File(".").absolutePath}")
    }

    private fun nativeTable(
        source: String,
        name: String,
    ): Set<String> {
        val body =
            Regex(
                """const\s+char\s+\*\s*$name\s*\[\]\s*=\s*\{(.*?)\};""",
                RegexOption.DOT_MATCHES_ALL,
            ).find(source)?.groupValues?.get(1)
                ?: error("Cannot find native extension table $name")
        check(Regex("""\bNULL\b""").containsMatchIn(body)) { "$name must be NULL terminated" }
        return Regex(""""([^"]+)"""").findAll(body).map { it.groupValues[1].removePrefix(".") }.toSet()
    }

    @Test
    fun nativeTablesMirrorAuthoritativeKotlinRoles() {
        val source = nativePolicySource()
        assertEquals(
            GameFileFormats.gameImportExtensions,
            nativeTable(source, "dxx_android_game_file_extensions"),
        )
        assertEquals(
            GameFileFormats.discExtractExtensions,
            nativeTable(source, "dxx_android_disc_extract_extensions"),
        )
        assertEquals(
            GameFileFormats.gogAudioExtensions,
            nativeTable(source, "dxx_android_gog_audio_extensions"),
        )
        assertEquals(
            GameFileFormats.macDiscExtractExtensions,
            nativeTable(source, "dxx_android_mac_disc_extract_extensions"),
        )
    }

    @Test
    fun macPolicyExceptionsAreExplicit() {
        assertEquals(
            setOf("256", "cfg", "txt"),
            GameFileFormats.macDiscExtractExtensions - GameFileFormats.discExtractExtensions,
        )
        assertEquals(
            setOf("dtx", "hxm", "pog", "rdl", "rl2"),
            GameFileFormats.discExtractExtensions - GameFileFormats.macDiscExtractExtensions,
        )
    }

    @Test
    fun recognizesSharedGameExtensions() {
        assertTrue(AndroidGameFileExtensions.hasGameExtension("custom.mn2"))
        assertTrue(AndroidGameFileExtensions.hasGameExtension("DESCENT2.HOG"))
        assertTrue(AndroidGameFileExtensions.hasGameExtension("demo.DEM"))
        assertFalse(AndroidGameFileExtensions.hasGameExtension("readme.txt"))
    }

    @Test
    fun recognizesGogAudioSubset() {
        assertTrue(AndroidGameFileExtensions.isGogAudioFile("descent_ii.gog"))
        assertTrue(AndroidGameFileExtensions.isGogAudioFile("DESCENT_II.INST"))
        assertFalse(AndroidGameFileExtensions.isGogAudioFile("descent2.hog"))
    }

    @Test
    fun genericAndMacRolesUseCaseInsensitiveNames() {
        assertTrue(GameFileFormats.hasDiscExtractExtension("LEVEL.RL2"))
        assertTrue(GameFileFormats.extensionOf("README.TXT") in GameFileFormats.macDiscExtractExtensions)
        assertFalse(GameFileFormats.hasDiscExtractExtension("README.TXT"))
    }
}
