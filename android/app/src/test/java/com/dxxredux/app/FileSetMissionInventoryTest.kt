package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class FileSetMissionInventoryTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    @Test
    fun listsAndRemovesDescriptorBackedMissionWithoutTouchingBaseGame() {
        val setDir = temporaryFolder.newFolder("default")
        File(setDir, "descent2.hog").writeText("base")
        val descriptor =
            File(setDir, "panic.mn2").apply {
                writeText("name = Vertigo Series\nbriefing = panic.tex\nnum_levels = 1\nlevel01.rl2\n")
            }
        val archive = File(setDir, "PANIC.HOG").apply { writeText("mission") }
        val briefing = File(setDir, "panic.tex").apply { writeText("briefing") }
        AssetManifest(setDir).save(
            listOf(
                AssetManifest.AssetEntry(
                    filename = archive.name,
                    sha256 = "00".repeat(32),
                    sizeBytes = archive.length(),
                    importedAt = 1L,
                    versionName = "D2 Vertigo Series",
                ),
            ),
        )

        val entry = FileSetMissionInventory.scan(setDir).single()

        assertEquals("Vertigo Series", entry.displayName)
        assertEquals("d2", entry.game)
        assertEquals("D2 Vertigo Series", entry.versionName)
        assertEquals(3, entry.files.size)
        assertEquals(3, FileSetMissionInventory.remove(setDir, entry))
        assertFalse(descriptor.exists())
        assertFalse(archive.exists())
        assertFalse(briefing.exists())
        assertTrue(File(setDir, "descent2.hog").isFile)
    }

    @Test
    fun removesPersistedMissionThroughItsOwner() {
        val setDir = temporaryFolder.newFolder("managed")
        File(setDir, "descent2.hog").writeText("base")
        File(setDir, "panic.mn2").writeText("name = Vertigo Series\nnum_levels = 1\nlevel01.rl2\n")
        File(setDir, "panic.hog").writeText("mission")
        FileSetContentManager(setDir).reconcile()

        val entry = FileSetMissionInventory.scan(setDir).single()

        assertTrue(entry.contentId != null)
        assertEquals(2, FileSetMissionInventory.remove(setDir, entry))
        assertTrue(FileSetContentManager(setDir).listEntries().isEmpty())
        assertTrue(File(setDir, "descent2.hog").isFile)
    }
}
