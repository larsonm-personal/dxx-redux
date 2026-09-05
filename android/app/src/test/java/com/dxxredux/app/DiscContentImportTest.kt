package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class DiscContentImportTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    @Test
    fun discImportFiltersInstallerFilesAndOwnsMissionsAsOnePersistentItem() {
        val setDir = temporaryFolder.newFolder("set")
        File(setDir, "personal.txt").writeText("User content")
        val manager = FileSetContentManager(setDir)
        val personal = manager.reconcile().entries.single()
        manager.setEnabled(personal.id, false)

        fun importDisc() = extractCueDataTracks(
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
                File(staging, "descent.cfg").writeText("DOS configuration")
                File(staging, "intro.txt").writeText("Mission briefing")
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
            setOf("missions/D2-2PLYR.HOG", "missions/D2-2PLYR.MN2", "missions/D2CHAOS.HOG",
                "missions/D2CHAOS.MN2", "missions/intro.txt"),
            disc.virtualPaths.toSet(),
        )
        assertFalse(entries.single { it.id == personal.id }.enabled)
        assertTrue(File(setDir, "DESCENT2.HOG").isFile)
        assertFalse(setDir.walkTopDown().any { it.name in setOf("descent2.sow", "DESCENT2.DEM", "setup.exe", "readme.txt", "descent.cfg") })
        val projection = manager.buildProjection("d2")
        assertTrue(File(projection, "missions/D2CHAOS.MN2").isFile)
        assertEquals("Mission briefing", File(projection, "missions/intro.txt").readText())

        manager.setEnabled(disc.id, false)
        assertFalse(File(manager.buildProjection("d2"), "missions/D2CHAOS.MN2").exists())
        assertTrue(importDisc().succeeded)
        val reimported = FileSetContentManager(setDir).reconcile().entries
        assertEquals(setOf(personal.id, disc.id), reimported.map { it.id }.toSet())
        assertFalse(reimported.single { it.id == disc.id }.enabled)
        manager.setEnabled(disc.id, true)
        assertTrue(File(manager.buildProjection("d2"), "missions/D2-2PLYR.MN2").isFile)
        assertTrue(manager.deleteEntry(disc.id))
        assertEquals(personal.id, manager.reconcile().entries.single().id)
        assertTrue(File(setDir, "DESCENT2.HOG").isFile)
        assertFalse(File(manager.buildProjection("d2"), "missions/D2CHAOS.MN2").exists())
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
