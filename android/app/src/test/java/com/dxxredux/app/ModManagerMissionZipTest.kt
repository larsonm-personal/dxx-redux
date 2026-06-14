package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.FileOutputStream
import java.io.RandomAccessFile
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

        ModManager(filesDir).writeEnabledModPaths("d1")
        assertFalse(File(filesDir, "d1x-redux/.active_mod_paths").exists())
    }

    @Test
    fun d1MsnMissionZipStagesForD1AndD2Engine() {
        val filesDir = File("build/test-mod-manager-mission-zip-d1-msn").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()

        val manager = ModManager(filesDir)
        val imported = manager.importMissionZipFile(createTrine2StyleMissionZip(), "trine2.zip")
        assertNotNull(imported)
        assertEquals("d1", imported!!.game)
        assertEquals("Trine - Episode 2", imported.displayName)

        manager.writeEnabledModPaths("d2")
        val d2PathFile = File(filesDir, "d2x-redux/.active_mod_paths")
        val d2Lines = d2PathFile.readLines()
        assertEquals(1, d2Lines.size)
        assertTrue(d2Lines[0].endsWith(".generated_mission_zips${File.separator}trine2.zip"))

        val d2StageDir = File(filesDir, "d2x-redux/.generated_mission_zips/trine2.zip/missions")
        assertTrue(File(d2StageDir, "trine2.msn").isFile)
        val d2Hog = File(d2StageDir, "trine2.hog")
        assertTrue(d2Hog.isFile)
        assertHogHasEntry(d2Hog, "e2m1.dtx")
        assertHogHasEntry(d2Hog, "e2m1.pg1")
        assertD1CustomPig1Counts(d2Hog, "e2m1.dtx", bitmaps = 1, sounds = 1)

        manager.writeEnabledModPaths("d1")
        val d1PathFile = File(filesDir, "d1x-redux/.active_mod_paths")
        val d1Lines = d1PathFile.readLines()
        assertEquals(1, d1Lines.size)
        assertTrue(d1Lines[0].endsWith(".generated_mission_zips${File.separator}trine2.zip"))

        val d1StageDir = File(filesDir, "d1x-redux/.generated_mission_zips/trine2.zip/missions")
        assertTrue(File(d1StageDir, "trine2.msn").isFile)
        val d1Hog = File(d1StageDir, "trine2.hog")
        assertTrue(d1Hog.isFile)
        assertHogHasEntry(d1Hog, "e2m1.dtx")
        assertHogHasEntry(d1Hog, "e2m1.pg1")
        assertD1CustomPig1Counts(d1Hog, "e2m1.dtx", bitmaps = 1, sounds = 1)
    }

    @Test
    fun missionZipWithOggSongListRequestsMissionSoundtrack() {
        val filesDir = File("build/test-mod-manager-mission-zip-soundtrack").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()

        val imported = ModManager(filesDir).importMissionZipFile(createMissionZipWithDxaMusic(), "Uneasy4.zip")
        assertNotNull(imported)

        assertTrue(ModManager(filesDir).hasEnabledMissionZipSoundtrack("d2"))
        assertFalse(ModManager(filesDir).hasEnabledMissionZipSoundtrack("d1"))
    }

    @Test
    fun missionZipWithMixedMidiAndOggRequestsMissionSoundtrack() {
        val filesDir = File("build/test-mod-manager-mission-zip-mixed-soundtrack").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()

        val imported = ModManager(filesDir).importMissionZipFile(createMissionZipWithMixedSoundtrack(), "Mixed.zip")
        assertNotNull(imported)

        assertTrue(ModManager(filesDir).hasEnabledMissionZipSoundtrack("d2"))
    }

    @Test
    fun missionZipWithTopLevelHmpSongListStagesDescentSongAlias() {
        val filesDir = File("build/test-mod-manager-mission-zip-obsidian-music").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()

        val manager = ModManager(filesDir)
        val imported = manager.importMissionZipFile(createObsidianStyleMissionZip(), "Obsidian.zip")
        assertNotNull(imported)

        assertTrue(manager.hasEnabledMissionZipSoundtrack("d2"))
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

    @Test
    fun rebirthMissionZipKeepsExistingMissionsDirectory() {
        val filesDir = File("build/test-mod-manager-mission-zip-rebirth-layout").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()

        val manager = ModManager(filesDir)
        val imported = manager.importMissionZipFile(createEnemyWithinStyleMissionZip(), "ewithin-rebirth.zip")
        assertNotNull(imported)

        manager.writeEnabledModPaths("d2")

        val pathFile = File(filesDir, "d2x-redux/.active_mod_paths")
        val lines = pathFile.readLines()
        assertEquals(2, lines.size)
        assertTrue(lines[0].endsWith(".generated_mission_zips${File.separator}ewithin-rebirth.zip"))
        assertTrue(
            lines[1].endsWith(
                ".generated_mission_zips${File.separator}ewithin-rebirth.zip${File.separator}ewithin.dxa",
            ),
        )

        val stageRoot = File(filesDir, "d2x-redux/.generated_mission_zips/ewithin-rebirth.zip")
        assertTrue(File(stageRoot, "ewithin.dxa").isFile)
        assertTrue(File(stageRoot, "missions/ewithin.mn2").isFile)
        assertTrue(File(stageRoot, "missions/ewithin.hog").isFile)
        assertFalse(File(stageRoot, "missions/missions/ewithin.mn2").exists())

        val details = manager.getModDetails(imported!!, File(filesDir, "sets/default"))
        assertTrue(details.notes.any { it.startsWith("ewithin.dxa contents:") })
    }

    @Test
    fun parentMissionZipImportsRebirthChild() {
        val filesDir = File("build/test-mod-manager-mission-zip-parent-rebirth").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()

        val imported = ModManager(filesDir).importMissionZipFile(createEnemyWithinParentZip(), "ewithin-versions.zip")

        assertNotNull(imported)
        imported!!
        assertEquals("ewithin-rebirth.zip", imported.filename)
        assertEquals("Descent: The Enemy Within", imported.displayName)
        assertTrue(File(filesDir, "mods/ewithin-rebirth.zip").isFile)
        assertFalse(File(filesDir, "mods/ewithin-xl.zip").exists())
    }

    @Test
    fun largeMissionZipImportsDurableExtractionAndDeletesWithOwner() {
        val filesDir = File("build/test-mod-manager-mission-zip-large-extract").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()

        val manager = ModManager(filesDir)
        val imported = manager.importMissionZipFile(createLargePreambleMissionZip(), "LargeMission.zip")

        assertNotNull(imported)
        imported!!
        assertEquals("extracted_bundle", imported.importMode)

        val extractedRoot = File(filesDir, "mods/.extracted_mission_zips/LargeMission.zip")
        assertTrue(File(extractedRoot, "missions/Uneasy4.hog").isFile)
        assertTrue(File(extractedRoot, "missions/Uneasy4.mn2").isFile)

        val modFile = File(filesDir, "mods/LargeMission.zip")
        val scan = MissionZip.inspect(modFile)
        assertNotNull(scan)
        val target =
            LevelMetadataTargets
                .missionZipTargets(modFile.absolutePath, File(filesDir, "sets/default"), scan!!)
                .single()
        assertEquals(extractedRoot.absolutePath, target.sourcePath)
        assertEquals(null, target.archivePath)
        assertEquals(listOf("missions/Uneasy4.hog"), target.hogFiles)

        val descriptorTarget =
            LevelMetadataTargets.zipConstituent(
                modFile.absolutePath,
                File(filesDir, "sets/default"),
                scan.constituents.single { it.name == "Uneasy4.mn2" },
            )
        assertNotNull(descriptorTarget)
        descriptorTarget!!
        assertEquals("mission_files", descriptorTarget.sourceType)
        assertEquals(extractedRoot.absolutePath, descriptorTarget.sourcePath)
        assertEquals(null, descriptorTarget.archivePath)
        assertEquals(listOf("missions/Uneasy4.hog"), descriptorTarget.hogFiles)

        val hogFile = File(extractedRoot, "missions/Uneasy4.hog")
        val hogTarget =
            LevelMetadataTargets.zipConstituent(
                modFile.absolutePath,
                File(filesDir, "sets/default"),
                scan.constituents.single { it.name == "Uneasy4.hog" },
                GameFileMetadata.summarizeLocalFile(hogFile),
            )
        assertNotNull(hogTarget)
        hogTarget!!
        assertEquals("mission_files", hogTarget.sourceType)
        assertEquals(extractedRoot.absolutePath, hogTarget.sourcePath)
        assertEquals(null, hogTarget.archivePath)
        assertEquals(listOf("missions/Uneasy4.hog"), hogTarget.hogFiles)

        manager.writeEnabledModPaths("d2")
        val lines = File(filesDir, "d2x-redux/.active_mod_paths").readLines()
        assertEquals(listOf(extractedRoot.absolutePath), lines)

        manager.deleteMod(imported.filename)
        assertFalse(File(filesDir, "mods/LargeMission.zip").exists())
        assertFalse(extractedRoot.exists())
    }

    @Test
    fun movingMissionZipImportDoesNotKeepScratchCopy() {
        val filesDir = File("build/test-mod-manager-mission-zip-moving-source").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()
        val scratchDir = File(filesDir, "mission_zip_batch_cache").apply { mkdirs() }
        val source = File(scratchDir, "LargeMission.zip")
        val temp = createLargePreambleMissionZip()
        temp.copyTo(source, overwrite = true)
        temp.delete()

        val imported = ModManager(filesDir).importMissionZipFileMovingSource(source, "LargeMission.zip")

        assertNotNull(imported)
        imported!!
        assertEquals("extracted_bundle", imported.importMode)
        assertFalse(source.exists())
        assertTrue(File(filesDir, "mods/LargeMission.zip").isFile)
        assertTrue(File(filesDir, "mods/.extracted_mission_zips/LargeMission.zip/missions/Uneasy4.hog").isFile)
    }

    @Test
    fun missionZipWithLargeNestedHogImportsDurableExtraction() {
        val filesDir = File("build/test-mod-manager-mission-zip-large-inner-hog").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()

        val imported = ModManager(filesDir).importMissionZipFile(createLargeNestedHogMissionZip(), "LargeInnerHog.zip")

        assertNotNull(imported)
        imported!!
        assertEquals("extracted_bundle", imported.importMode)
        assertTrue(File(filesDir, "mods/.extracted_mission_zips/LargeInnerHog.zip/missions/Uneasy4.hog").isFile)
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

    private fun createLargeNestedHogMissionZip(): File {
        val zipFile = File.createTempFile("missionzip-manager-large-inner-hog", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("Uneasy4.hog"))
            val buffer = ByteArray(1024 * 1024)
            val chunks = (MissionZip.SMALL_NESTED_ARCHIVE_LIMIT_BYTES / buffer.size).toInt() + 1
            repeat(chunks) {
                zip.write(buffer)
            }
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

    private fun createLargePreambleMissionZip(): File {
        val zipFile = File.createTempFile("missionzip-manager-large", ".zip")
        zipFile.deleteOnExit()
        RandomAccessFile(zipFile, "rw").use { it.setLength(MissionZip.SMALL_IN_MEMORY_LIMIT_BYTES + 1L) }
        FileOutputStream(zipFile, true).use { output ->
            output.write(
                createZipBytes {
                    it.putNextEntry(ZipEntry("Uneasy4.hog"))
                    it.write(createHogBytes("Uneasy4.rl2" to ByteArray(12)))
                    it.closeEntry()

                    it.putNextEntry(ZipEntry("Uneasy4.mn2"))
                    it.write("name = Uneasy 4\nnum_levels = 1\nUneasy4.rl2\n".toByteArray())
                    it.closeEntry()
                },
            )
        }
        return zipFile
    }

    private fun createTrine2StyleMissionZip(): File {
        val zipFile = File.createTempFile("missionzip-manager-trine2", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("trine2.hog"))
            zip.write(
                createHogBytes(
                    "e2m1.rdl" to ByteArray(12),
                    "e2m1.pg1" to createD1PogLikeTextureBytes(),
                    "e2m1.dtx" to createD1CustomTextureBytes(),
                ),
            )
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("trine2.msn"))
            zip.write(
                """
                name = Trine - Episode 2
                type = normal
                num_levels = 1
                e2m1.rdl
                custom_music = yes
                custom_textures = yes
                """.trimIndent().toByteArray(),
            )
            zip.closeEntry()
        }
        return zipFile
    }

    private fun createEnemyWithinParentZip(): File {
        val zipFile = File.createTempFile("missionzip-manager-ewithin-parent", ".zip")
        zipFile.deleteOnExit()
        val rebirthBytes = createZipBytes { writeEnemyWithinStyleEntries(it) }
        val xlBytes =
            createZipBytes {
                it.putNextEntry(ZipEntry("xl.txt"))
                it.write("not the package we want".toByteArray())
                it.closeEntry()
            }
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("ewithin-xl.zip"))
            zip.write(xlBytes)
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("ewithin-rebirth.zip"))
            zip.write(rebirthBytes)
            zip.closeEntry()
        }
        return zipFile
    }

    private fun createEnemyWithinStyleMissionZip(): File {
        val zipFile = File.createTempFile("missionzip-manager-ewithin", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip -> writeEnemyWithinStyleEntries(zip) }
        return zipFile
    }

    private fun writeEnemyWithinStyleEntries(zip: ZipOutputStream) {
        zip.putNextEntry(ZipEntry("ewithin.dxa"))
        zip.write(
            createZipBytes {
                it.putNextEntry(ZipEntry("descent.sng"))
                it.write("briefing.ogg\nendlevel.ogg\nlevel01.ogg\n".toByteArray())
                it.closeEntry()

                it.putNextEntry(ZipEntry("level01.ogg"))
                it.write(byteArrayOf(1, 2, 3, 4))
                it.closeEntry()

                it.putNextEntry(ZipEntry("descent2.ham"))
                it.write(byteArrayOf(5, 6, 7, 8))
                it.closeEntry()
            },
        )
        zip.closeEntry()

        zip.putNextEntry(ZipEntry("ewithin.txt"))
        zip.write("Enemy Within notes".toByteArray())
        zip.closeEntry()

        zip.putNextEntry(ZipEntry("missions/ewithin.hog"))
        zip.write(createHogBytes("level01.rl2" to ByteArray(12)))
        zip.closeEntry()

        zip.putNextEntry(ZipEntry("missions/ewithin.mn2"))
        zip.write(
            """
            name = Descent: The Enemy Within
            type = normal
            num_levels = 1
            level01.rl2
            """.trimIndent().toByteArray(),
        )
        zip.closeEntry()
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

    private fun createMissionZipWithMixedSoundtrack(): File {
        val zipFile = File.createTempFile("missionzip-manager-mixed-music", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("mixed.dxa"))
            zip.write(
                createZipBytes {
                    it.putNextEntry(ZipEntry("descent.sng"))
                    it.write("game01.hmp\nlevel02.ogg\n".toByteArray())
                    it.closeEntry()

                    it.putNextEntry(ZipEntry("game01.hmp"))
                    it.write(byteArrayOf(1, 2, 3, 4))
                    it.closeEntry()

                    it.putNextEntry(ZipEntry("level02.ogg"))
                    it.write(byteArrayOf(5, 6, 7, 8))
                    it.closeEntry()
                },
            )
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("mixed.hog"))
            zip.write(createHogBytes("level01.rl2" to ByteArray(12)))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("mixed.mn2"))
            zip.write("name = Mixed Music\nnum_levels = 1\nlevel01.rl2\n".toByteArray())
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

    private fun createD1CustomTextureBytes(): ByteArray =
        ByteArrayOutputStream().use { output ->
            output.write(leInt(1))
            output.write(leInt(1))
            output.write(fixedName("rock263", 8))
            output.write(byteArrayOf(0, 1, 1, 0, 7))
            output.write(leInt(0))
            output.write(fixedName("laser", 8))
            output.write(leInt(1))
            output.write(leInt(1))
            output.write(leInt(1))
            output.write(byteArrayOf(7))
            output.write(byteArrayOf(42))
            output.toByteArray()
        }

    private fun createD1PogLikeTextureBytes(): ByteArray =
        ByteArrayOutputStream().use { output ->
            output.write("DPOG".toByteArray(Charsets.US_ASCII))
            output.write(leInt(1))
            output.write(leInt(1))
            output.write(leShort(0))
            output.write(fixedName("rock263", 8))
            output.write(byteArrayOf(0, 1, 1, 0, 0, 7))
            output.write(leInt(0))
            output.write(byteArrayOf(7))
            output.toByteArray()
        }

    private fun assertHogHasEntry(
        hog: File,
        name: String,
    ) {
        assertNotNull("Missing HOG entry $name", readHogEntryData(hog, name))
    }

    private fun assertD1CustomPig1Counts(
        hog: File,
        name: String,
        bitmaps: Int,
        sounds: Int,
    ) {
        val data = readHogEntryData(hog, name)
        assertNotNull("Missing HOG entry $name", data)
        data!!
        assertTrue(data.size >= 8)
        assertEquals(bitmaps, readLeInt(data, 0))
        assertEquals(sounds, readLeInt(data, 4))
    }

    private fun readHogEntryData(
        hog: File,
        name: String,
    ): ByteArray? {
        val data = hog.readBytes()
        assertTrue(data.size >= 3)
        assertEquals("DHF", data.copyOfRange(0, 3).toString(Charsets.US_ASCII))
        var offset = 3
        while (offset + 17 <= data.size) {
            val entryName =
                data
                    .copyOfRange(offset, offset + 13)
                    .takeWhile { it.toInt() != 0 }
                    .toByteArray()
                    .toString(Charsets.US_ASCII)
            offset += 13
            val size =
                readLeInt(data, offset)
            offset += 4
            if (entryName.equals(name, ignoreCase = true)) return data.copyOfRange(offset, offset + size)
            offset += size
        }
        return null
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

    private fun readLeInt(
        data: ByteArray,
        offset: Int,
    ): Int =
        (data[offset].toInt() and 0xff) or
            ((data[offset + 1].toInt() and 0xff) shl 8) or
            ((data[offset + 2].toInt() and 0xff) shl 16) or
            ((data[offset + 3].toInt() and 0xff) shl 24)

    private fun leShort(value: Int): ByteArray =
        byteArrayOf(
            (value and 0xff).toByte(),
            ((value shr 8) and 0xff).toByte(),
        )
}
