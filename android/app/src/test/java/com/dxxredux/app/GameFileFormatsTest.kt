package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class GameFileFormatsTest {
    @Test
    fun normalizesDxaSuffixesAndLabelsFiles() {
        assertEquals("dxa", GameFileFormats.extensionOf("uud1sp.dxa (1)"))
        assertTrue(GameFileFormats.isDxa("uud1sp.dxa (1)"))
        assertEquals("uud1sp", GameFileFormats.stripDxaSuffix("uud1sp.dxa (1)"))
        assertEquals("Game mod", GameFileFormats.typeLabel("uud1sp.dxa (1)"))
        assertEquals(".dxa - game mod", GameFileFormats.extensionDescription("uud1sp.dxa (1)"))
        assertEquals("Saved game", GameFileFormats.typeLabel("player.sg1"))
    }

    @Test
    fun exposesImportAndExtractorFactsFromOneRegistry() {
        assertTrue(GameFileFormats.hasGameImportExtension("custom.mn2"))
        assertTrue(GameFileFormats.hasGameImportExtension("DESCENT2.HOG"))
        assertTrue(GameFileFormats.hasDiscExtractExtension("level.rl2"))
        assertTrue(GameFileFormats.isGogAudioFile("descent_ii.inst"))
        assertFalse(GameFileFormats.hasGameImportExtension("readme.txt"))
    }

    @Test
    fun parsesMissionDescriptorsWithDeclaredLevelsAndGameHints() {
        val mission =
            GameFileFormats.parseMissionDescriptor(
                "missions/Uneasy4.mn2",
                """
                name = Uneasy 4
                type = normal
                num_levels = 1
                Uneasy4.rl2
                num_secrets = 1
                Uneasy4s.rl2,1
                author = Blarget 2 and Nightsurfer
                editor = Inferno 1.0.22
                briefing = uneasy.tex
                !ham = robots.ham
                """.trimIndent(),
            )

        assertEquals("missions/Uneasy4.mn2", mission.path)
        assertEquals("Uneasy 4", mission.displayName)
        assertEquals("normal", mission.type)
        assertEquals("Blarget 2 and Nightsurfer", mission.author)
        assertEquals("Inferno 1.0.22", mission.editor)
        assertEquals(1, mission.declaredLevelCount)
        assertEquals(listOf("Uneasy4.rl2"), mission.levelNames)
        assertEquals(1, mission.declaredSecretLevelCount)
        assertEquals(listOf("Uneasy4s.rl2"), mission.secretLevelNames)
        assertEquals(listOf(1), mission.secretLevelOrigins)
        assertEquals(mapOf("Briefing" to "uneasy.tex", "HAM" to "robots.ham"), mission.assetReferences)
        assertEquals(GameFileFormats.GAME_D2, mission.game)
    }

    @Test
    fun classifiesMissionZipAndModRoles() {
        assertEquals(GameFileFormats.MISSION_ZIP_DESCRIPTOR, GameFileFormats.missionZipRoleForFile("custom.mn2"))
        assertEquals(GameFileFormats.MISSION_ZIP_HOG, GameFileFormats.missionZipRoleForFile("custom.hog"))
        assertEquals(GameFileFormats.MISSION_ZIP_MOD_ARCHIVE, GameFileFormats.missionZipRoleForFile("hires.dxa"))
        assertEquals(GameFileFormats.MISSION_ZIP_DOCUMENTATION, GameFileFormats.missionZipRoleForFile("readme.txt"))
        assertEquals("Mission assets", GameFileFormats.missionZipRoleLabel(GameFileFormats.MISSION_ZIP_HOG))
        assertEquals("Texture replacements", GameFileFormats.modCategoryLabel("textures/door.ktx2"))
        assertEquals("Base game file replacements", GameFileFormats.modCategoryLabel("descent2.ham"))
        assertNull(GameFileFormats.modCategoryLabel("patches/manifest.rfc6902.json"))
    }

    @Test
    fun preservesStoragePurposeLabels() {
        assertEquals(
            "Game mod",
            GameFileFormats.storagePurpose(File("hires.dxa"), "mods/hires.dxa", importedRootFile = true),
        )
        assertEquals(
            "Crash report",
            GameFileFormats.storagePurpose(File("trace.txt"), "crashlogs/trace.txt", importedRootFile = false),
        )
    }
}
