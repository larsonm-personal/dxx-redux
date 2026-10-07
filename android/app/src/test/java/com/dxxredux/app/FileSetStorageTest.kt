package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class FileSetStorageTest {
    @get:Rule
    val temporaryFolder = TemporaryFolder()

    @Test
    fun deletingAnUnregisteredSetRemovesItsContentBeforeRecreation() {
        val filesDir = temporaryFolder.newFolder("files")
        val importRoot = temporaryFolder.newFolder("import")
        val manager = FileSetManager(filesDir, importRoot)
        val defaultDir = manager.getSetDir(FileSetManager.DEFAULT_SET)
        defaultDir.resolve("descent.hog").writeText("retained game data")
        val customDir = manager.createSet("custom")
        customDir.resolve("demos/old.dem").apply {
            parentFile?.mkdirs()
            writeText("old demo")
        }
        FileSetContentManager(customDir).reconcile()
        assertEquals(1, FileSetContentManager(customDir).listEntries().size)

        // Match the automation reset that clears the registry but retains imported content
        assertTrue(filesDir.resolve("file_sets.json").delete())
        manager.deleteSet("custom")

        assertFalse(customDir.exists())
        assertEquals("retained game data", defaultDir.resolve("descent.hog").readText())
        val recreated = manager.createSet("custom")
        assertTrue(FileSetContentManager(recreated).listEntries().isEmpty())
    }

    @Test
    fun currentSetsPersistAndPublishBothGamePathsUnderTheSelectedRoot() {
        val filesDir = temporaryFolder.newFolder("files")
        val importRoot = temporaryFolder.newFolder("import")
        val manager = FileSetManager(filesDir, importRoot)
        assertEquals(FileSetManager.DEFAULT_SET, manager.getActive())
        assertEquals(listOf(FileSetManager.DEFAULT_SET), manager.listSets().map { it.name })
        val defaultDir = manager.getSetDir(FileSetManager.DEFAULT_SET)
        assertEquals(File(importRoot, "sets/default"), defaultDir)
        defaultDir.resolve("descent.hog").writeText("default game data")

        val customDir = manager.createSet("custom", "test import")
        customDir.resolve("descent.hog").writeText("custom game data")
        manager.setActive("custom")

        val reopened = FileSetManager(filesDir, importRoot)
        assertEquals("custom", reopened.getActive())
        assertEquals("test import", reopened.listSets().single { it.name == "custom" }.source)
        reopened.writeActiveSetPath()
        for (game in listOf("d1x-redux", "d2x-redux")) {
            assertEquals(customDir.absolutePath, filesDir.resolve("$game/.active_set_path").readText())
        }
        assertEquals("default game data", defaultDir.resolve("descent.hog").readText())
        assertEquals("custom game data", customDir.resolve("descent.hog").readText())
        assertFalse(filesDir.resolve("descent.hog").exists())
        assertFalse(filesDir.resolve("sets").exists())

        reopened.setActive(FileSetManager.DEFAULT_SET)
        reopened.writeActiveSetPath()
        for (game in listOf("d1x-redux", "d2x-redux")) {
            assertEquals(defaultDir.absolutePath, filesDir.resolve("$game/.active_set_path").readText())
        }
        assertTrue(customDir.resolve("descent.hog").exists())
    }
}
