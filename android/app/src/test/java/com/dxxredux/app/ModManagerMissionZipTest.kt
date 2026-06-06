package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class ModManagerMissionZipTest {
    @Test
    fun importsMissionZipAndPersistsMetadata() {
        val filesDir = File("build/test-mod-manager-mission-zip").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()
        val source = createMissionZip()

        val imported = ModManager(filesDir).importMissionZipFile(source, "Uneasy4.zip")

        assertNotNull(imported)
        imported!!
        assertEquals(ModManager.MOD_KIND_MISSION_ZIP, imported.kind)
        assertEquals("Uneasy 4", imported.displayName)
        assertEquals("d2", imported.game)
        assertEquals("levels", imported.category)
        assertEquals("stored_zip", imported.importMode)

        val reloaded = ModManager(filesDir).listMods().single()
        assertEquals(ModManager.MOD_KIND_MISSION_ZIP, reloaded.kind)
        assertEquals("Uneasy 4", reloaded.missionTitle)

        val details = ModManager(filesDir).getModDetails(reloaded, File(filesDir, "sets/default"))
        assertNotNull(details.missionZip)
        assertEquals("Uneasy 4", details.missionZip!!.mission.displayName)
        assertEquals(3, details.missionZip.constituents.size)
        assertTrue(details.categories.any { it.label == "Mission descriptor" })
        assertTrue(details.categories.any { it.label == "Mission assets" })
        assertTrue(details.categories.any { it.label == "Bundled mod archive" })
    }

    @Test
    fun missionZipActivePathMountsAtMissions() {
        val filesDir = File("build/test-mod-manager-mission-zip-active-path").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()

        val imported = ModManager(filesDir).importMissionZipFile(createMissionZip(), "Uneasy4.zip")
        assertNotNull(imported)

        ModManager(filesDir).writeEnabledModPaths("d2")

        val pathFile = File(filesDir, "d2x-redux/.active_mod_paths")
        val lines = pathFile.readLines()
        assertEquals(2, lines.size)
        assertTrue(lines[0].endsWith(".generated_mission_zips${File.separator}Uneasy4.zip"))
        assertTrue(
            lines[1].endsWith(
                ".generated_mission_zips${File.separator}Uneasy4.zip${File.separator}missions${File.separator}Uneasy4.dxa",
            ),
        )

        val stageDir = File(filesDir, "d2x-redux/.generated_mission_zips/Uneasy4.zip/missions")
        assertTrue(File(stageDir, "Uneasy4.mn2").isFile)
        assertTrue(File(stageDir, "Uneasy4.hog").isFile)
        assertTrue(File(stageDir, "Uneasy4.dxa").isFile)
    }

    private fun createMissionZip(): File {
        val zipFile = File.createTempFile("missionzip-manager", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("Uneasy4.dxa"))
            zip.write(byteArrayOf(1, 2, 3, 4))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("Uneasy4.hog"))
            zip.write(byteArrayOf(5, 6, 7, 8))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("Uneasy4.mn2"))
            zip.write(
                """
                name = Uneasy 4
                type = normal
                num_levels = 1
                Uneasy4.rl2
                """.trimIndent().toByteArray(),
            )
            zip.closeEntry()
        }
        return zipFile
    }
}
