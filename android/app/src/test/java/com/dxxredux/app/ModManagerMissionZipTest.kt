package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayOutputStream
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

    @Test
    fun missionZipWithOggSongListRequestsBuiltinMusic() {
        val filesDir = File("build/test-mod-manager-mission-zip-builtin-music").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()

        val imported = ModManager(filesDir).importMissionZipFile(createMissionZipWithDxaMusic(), "Uneasy4.zip")
        assertNotNull(imported)

        assertTrue(ModManager(filesDir).hasEnabledMissionZipBuiltinMusic("d2"))
        assertFalse(ModManager(filesDir).hasEnabledMissionZipBuiltinMusic("d1"))
    }

    @Test
    fun missionZipWithTopLevelHmpSongListStagesDescentSongAlias() {
        val filesDir = File("build/test-mod-manager-mission-zip-obsidian-music").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()

        val manager = ModManager(filesDir)
        val imported = manager.importMissionZipFile(createObsidianStyleMissionZip(), "Obsidian.zip")
        assertNotNull(imported)

        assertTrue(manager.hasEnabledMissionZipBuiltinMusic("d2"))
        manager.writeEnabledModPaths("d2")

        val stageDir = File(filesDir, "d2x-redux/.generated_mission_zips/Obsidian.zip/missions")
        assertTrue(File(stageDir, "obsidian.sng").isFile)
        assertEquals(
            "descent.hmp\nbriefing.hmp\ngame01.hmp\n",
            File(stageDir, "descent.sng").readText(),
        )

        val details = manager.getModDetails(imported!!, File(filesDir, "sets/default"))
        assertTrue(details.notes.any { it == "Includes a mission song list" })
        assertTrue(details.notes.any { it.contains("Robot data patch") || it.contains("robot data patch") })
        assertTrue(details.notes.any { it.contains("Texture override") || it.contains("texture override") })
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

    private fun createObsidianStyleMissionZip(): File {
        val zipFile = File.createTempFile("missionzip-manager-obsidian", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("obsidian.hog"))
            zip.write(
                createHogBytes(
                    "game01.hmp" to ByteArray(16),
                    "obsidian.txb" to ByteArray(12),
                    "objec1.pcx" to ByteArray(20),
                    "argnentr.hxm" to ByteArray(24),
                    "argnentr.pog" to createPogBytes(),
                    "argnentr.rl2" to ByteArray(28),
                ),
            )
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("obsidian.mn2"))
            zip.write("name = Obsidian\nnum_levels = 1\nargnentr.rl2\n".toByteArray())
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("obsidian.sng"))
            zip.write("descent.hmp\nbriefing.hmp\ngame01.hmp\n".toByteArray())
            zip.closeEntry()
        }
        return zipFile
    }

    private fun createMissionZipWithDxaMusic(): File {
        val zipFile = File.createTempFile("missionzip-manager-music", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("Uneasy4.dxa"))
            zip.write(
                createZipBytes {
                    it.putNextEntry(ZipEntry("descent.sng"))
                    it.write(
                        """
                        descent.hmp
                        briefing.hmp
                        endlevel.hmp
                        endgame.hmp
                        credits.hmp
                        Uneasy4.ogg
                        """.trimIndent().toByteArray(),
                    )
                    it.closeEntry()

                    it.putNextEntry(ZipEntry("Uneasy4.ogg"))
                    it.write(byteArrayOf(1, 2, 3, 4))
                    it.closeEntry()
                },
            )
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("Uneasy4.hog"))
            zip.write(byteArrayOf(5, 6, 7, 8))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("Uneasy4.mn2"))
            zip.write("name = Uneasy 4\nnum_levels = 1\nUneasy4.rl2\n".toByteArray())
            zip.closeEntry()
        }
        return zipFile
    }

    private fun createZipBytes(writeEntries: (ZipOutputStream) -> Unit): ByteArray =
        ByteArrayOutputStream().use { output ->
            ZipOutputStream(output).use(writeEntries)
            output.toByteArray()
        }

    private fun createHogBytes(vararg entries: Pair<String, ByteArray>): ByteArray =
        ByteArrayOutputStream().use { output ->
            output.write("DHF".toByteArray(Charsets.US_ASCII))
            entries.forEach { (name, data) ->
                output.write(fixedName(name, 13))
                output.write(leInt(data.size))
                output.write(data)
            }
            output.toByteArray()
        }

    private fun createPogBytes(): ByteArray =
        ByteArrayOutputStream().use { output ->
            output.write("DPOG".toByteArray(Charsets.US_ASCII))
            output.write(leInt(1))
            output.write(leInt(0))
            output.toByteArray()
        }

    private fun fixedName(
        name: String,
        size: Int,
    ): ByteArray {
        val out = ByteArray(size)
        val bytes = name.toByteArray(Charsets.US_ASCII)
        bytes.copyInto(out, endIndex = minOf(bytes.size, size))
        return out
    }

    private fun leInt(value: Int): ByteArray =
        byteArrayOf(
            (value and 0xff).toByte(),
            ((value shr 8) and 0xff).toByte(),
            ((value shr 16) and 0xff).toByte(),
            ((value shr 24) and 0xff).toByte(),
        )
}
