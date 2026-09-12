package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assume.assumeTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.nio.file.Files
import java.nio.file.attribute.BasicFileAttributes

class DiscContentImportTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    @Test
    fun discImportFiltersInstallerFilesAndOwnsMissionsAsOnePersistentItem() {
        val setDir = temporaryFolder.newFolder("set")
        File(setDir, "personal.txt").writeText("User content")
        val manager = FileSetContentManager(setDir)
        val personal = manager.reconcile().entries.single()
        manager.setEnabled(personal.id, false)

        fun importDisc() =
            extractCueDataTracks(
                setDir = setDir,
                tracks = listOf(DiscImportBridge.CueTrack(1, 0, 0, 0, 100, "Data")),
                imageCount = 1,
                sourceName = "DESCENT_II_ABYSS.cue",
                extractTrack = { _, staging, _, _ ->
                    val data = File(staging, "d2data").apply { mkdirs() }
                    File(data, "DESCENT2.HOG").writeText("Base game")
                    for (stem in listOf("D2-2PLYR", "D2CHAOS")) {
                        File(data, "$stem.HOG").writeText("Mission $stem")
                        File(data, "$stem.MN2").writeText(
                            "name = $stem\nbriefing = intro.txt\nnum_levels = 1\nlevel01.rl2\n",
                        )
                    }
                    File(data, "descent2.sow").writeText("Consumed installer archive")
                    File(data, "DESCENT2.DEM").writeText("Recording")
                    CueDataTrackAttempt(7, 0)
                },
                postProcess = { staging, _, _ ->
                    // Model additional SOW output before the real hoisting and publication path
                    File(staging, "readme.txt").writeText("Installer documentation")
                    File(staging, "setup.exe").writeText("Installer")
                    File(staging, "ARCAN01B.PCX").writeText("Standalone promotional screenshot")
                    File(staging, "descent.cfg").writeText("DOS configuration")
                    File(staging, "d2data/intro.txt").writeText("Mission briefing")
                    hoistNestedImportedGameFiles(staging)
                    4
                },
            )

        assertTrue(importDisc().succeeded)
        val entries = manager.reconcile().entries
        assertEquals(2, entries.size)
        val disc = entries.single { it.id != personal.id }
        assertEquals("DESCENT II ABYSS", disc.displayName)
        assertEquals(5, disc.files.size)
        assertEquals(
            setOf(
                "missions/d2data/D2-2PLYR.HOG",
                "missions/d2data/D2-2PLYR.MN2",
                "missions/d2data/D2CHAOS.HOG",
                "missions/d2data/D2CHAOS.MN2",
                "missions/d2data/intro.txt",
            ),
            disc.virtualPaths.toSet(),
        )
        assertFalse(entries.single { it.id == personal.id }.enabled)
        assertTrue(File(setDir, "DESCENT2.HOG").isFile)
        assertFalse(
            setDir.walkTopDown().any {
                it.name in
                    setOf("descent2.sow", "DESCENT2.DEM", "setup.exe", "readme.txt", "descent.cfg")
            },
        )
        val projection = manager.buildProjection("d2")
        assertTrue(File(projection, "missions/d2data/D2CHAOS.MN2").isFile)
        assertEquals("Mission briefing", File(projection, "missions/d2data/intro.txt").readText())

        manager.setEnabled(disc.id, false)
        assertFalse(File(manager.buildProjection("d2"), "missions/d2data/D2CHAOS.MN2").exists())
        assertTrue(importDisc().succeeded)
        val reimported = FileSetContentManager(setDir).reconcile().entries
        assertEquals(setOf(personal.id, disc.id), reimported.map { it.id }.toSet())
        assertFalse(reimported.single { it.id == disc.id }.enabled)
        manager.setEnabled(disc.id, true)
        assertTrue(File(manager.buildProjection("d2"), "missions/d2data/D2-2PLYR.MN2").isFile)
        assertTrue(manager.deleteEntry(disc.id))
        assertEquals(
            personal.id,
            manager
                .reconcile()
                .entries
                .single()
                .id,
        )
        assertTrue(File(setDir, "DESCENT2.HOG").isFile)
        assertFalse(File(manager.buildProjection("d2"), "missions/d2data/D2CHAOS.MN2").exists())
    }

    @Test
    fun preservesMissionDirectoriesAndCompanionsInOneMixedGameBundle() {
        val setDir = temporaryFolder.newFolder("set")
        val staging = temporaryFolder.newFolder("staging")

        fun write(
            path: String,
            text: String,
        ) = File(staging, path).apply {
            checkNotNull(parentFile).mkdirs()
            writeText(text)
        }
        write("descent/descent.hog", "Base D1")
        write("d2demo/d2demo.hog", "Base D2")
        write("newlevel/mad.msn", "name = Mad\nnum_levels = 1\nmad.rdl\n")
        write("newlevel/mad.rdl", "Loose level")
        write("newlevel/readme.txt", "Mission documentation")
        write("newlevel/intro.pcx", "Implicit briefing image")
        write("levels/mad.msn", "name = Other Mad\nnum_levels = 1\nmad.rdl\n")
        write("levels/mad.hog", "Different mission with the same filename")
        write("missions/bonus.mn2", "name = Bonus\nnum_levels = 1\nbonus.rl2\n")
        write("missions/bonus.rl2", "D2 loose level")
        write("setup.exe", "Installer")
        write("text/readme.txt", "Unrelated documentation")
        val manager = FileSetContentManager(setDir)
        manager.publishDiscImport(staging, "Descent_Anniversary.iso")
        val entry = manager.reconcile().entries.single()
        assertEquals("Descent Anniversary", entry.displayName)
        assertEquals(GameFileFormats.GAME_BOTH, entry.game)
        assertEquals(8, entry.files.size)
        val projection = manager.buildProjection("d1")
        assertEquals("Loose level", File(projection, "missions/newlevel/mad.rdl").readText())
        assertTrue(File(projection, "missions/newlevel/mad.msn").isFile)
        assertTrue(File(projection, "missions/newlevel/readme.txt").isFile)
        assertTrue(File(projection, "missions/newlevel/intro.pcx").isFile)
        assertTrue(File(projection, "missions/levels/mad.hog").isFile)
        assertTrue(File(projection, "missions/bonus.rl2").isFile)
        assertTrue(File(setDir, "descent.hog").isFile)
        assertTrue(File(setDir, "d2demo.hog").isFile)
        assertFalse(setDir.walkTopDown().any { it.name == "setup.exe" })
        manager.setEnabled(entry.id, false)
        assertFalse(manager.buildProjection("d1").walkTopDown().any { it.isFile })
        assertTrue(manager.deleteEntry(entry.id))
        assertTrue(File(setDir, "descent.hog").isFile)
    }

    @Test
    fun installerAudioRemainsOutsideTheExtrasOwner() {
        val filesDir = temporaryFolder.newFolder("files")
        val setDir = File(filesDir, "sets/default").apply { mkdirs() }
        val staging = temporaryFolder.newFolder("installer")
        File(staging, "descent2.hog").writeText("Base")
        File(staging, "bonus.mn2").writeText("name = Bonus\nnum_levels = 1\nbonus.rl2\n")
        File(staging, "bonus.rl2").writeText("Level")
        val stagedAudio = File(staging, "custom.gog").apply { writeText("Audio") }
        val audioFileKey = Files.readAttributes(stagedAudio.toPath(), BasicFileAttributes::class.java).fileKey()
        File(staging, "custom.inst").writeText("FILE \"custom.gog\" BINARY\n  TRACK 02 AUDIO\n    INDEX 01 00:00:00\n")
        val manager = FileSetContentManager(setDir)
        manager.publishDiscImport(staging, "Installer.exe")
        assertFalse(stagedAudio.exists())
        assertEquals(
            audioFileKey,
            Files.readAttributes(File(setDir, "custom.gog").toPath(), BasicFileAttributes::class.java).fileKey(),
        )
        val audio = checkNotNull(buildGogAudioSource(filesDir, setDir))
        assertEquals(listOf("sets/default/custom.gog"), audio.binPaths.map { it.replace('\\', '/') })
        File(setDir, ".content/audio/audio_sources.json").apply {
            checkNotNull(parentFile).mkdirs()
            writeText("""{"sources":[{"cue":"custom.inst","bins":["custom.gog"]}]}""")
        }
        val entry = manager.reconcile().entries.single()
        assertEquals(setOf("missions/bonus.mn2", "missions/bonus.rl2"), entry.virtualPaths.toSet())
        assertTrue(manager.deleteEntry(entry.id))
        assertTrue(File(setDir, "custom.gog").isFile)
        assertTrue(File(setDir, "custom.inst").isFile)
        assertTrue(File(setDir, "descent2.hog").isFile)
    }

    @Test
    fun anniversaryExtractedMediaRetainsEveryMissionAndLooseLevel() {
        val fixturePath = System.getenv("DXX_DISC_CONTENT_FIXTURE")
        assumeTrue("Set DXX_DISC_CONTENT_FIXTURE to native Anniversary extraction output", fixturePath != null)
        val fixture = File(checkNotNull(fixturePath))
        assertTrue(fixture.isDirectory)
        val staging = temporaryFolder.newFolder("anniversary")
        fixture.copyRecursively(staging, overwrite = true)
        val descriptors =
            staging
                .walkTopDown()
                .filter {
                    it.isFile &&
                        GameFileFormats.isMissionDescriptor(
                            it.name,
                        )
                }.toList()
        assertTrue(descriptors.size >= 19)
        val expected =
            descriptors.associate {
                "missions/${it.relativeTo(staging).invariantSeparatorsPath}" to
                    it.readBytes().toList()
            }
        val setDir = temporaryFolder.newFolder("anniversary-set")
        val manager = FileSetContentManager(setDir)
        manager.publishDiscImport(staging, "Descent Anniversary.iso")
        val entry = manager.reconcile().entries.single()
        val projection = manager.buildProjection("d1")
        expected.forEach { (path, bytes) -> assertEquals(path, bytes, File(projection, path).readBytes().toList()) }
        for (stem in listOf("mad", "newest", "retrib10", "retrib11")) {
            assertTrue(File(projection, "missions/newlevel/$stem.msn").isFile)
            assertEquals(
                File(fixture, "newlevel/$stem.rdl").readBytes().toList(),
                File(projection, "missions/newlevel/$stem.rdl").readBytes().toList(),
            )
        }
        manager.setEnabled(entry.id, false)
        val repeat = temporaryFolder.newFolder("repeat")
        fixture.copyRecursively(repeat, overwrite = true)
        manager.publishDiscImport(repeat, "Descent Anniversary.iso")
        assertEquals(
            entry.id,
            manager
                .reconcile()
                .entries
                .single()
                .id,
        )
        assertFalse(manager.listEntries().single().enabled)
    }

    @Test
    fun baseOnlyDiscDoesNotCreateAnEmptyMod() {
        val staging = temporaryFolder.newFolder("staging")
        File(staging, "descent2.hog").writeText("Base game")
        File(staging, "descent2.sow").writeText("Installer archive")
        prepareDiscContent(staging, "Base.cue")
        assertTrue(FileSetContentManager(staging).reconcile().entries.isEmpty())
        assertTrue(File(staging, "descent2.hog").isFile)
        assertFalse(File(staging, "descent2.sow").exists())
    }
}
