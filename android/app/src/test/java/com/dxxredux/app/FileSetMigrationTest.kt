package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class FileSetMigrationTest {
    @get:Rule
    val temporaryFolder = TemporaryFolder()

    @Test
    fun `v0 migration copies verifies and commits before sweep`() {
        val filesDir = temporaryFolder.newFolder("files")
        val importRoot = temporaryFolder.newFolder("import")
        val source = filesDir.resolve("descent.hog").apply { writeBytes(byteArrayOf(1, 2, 3)) }
        val manager = FileSetManager(filesDir, importRoot)

        manager.migrateDefaultSetIfNeeded()

        val destination = importRoot.resolve("sets/default/descent.hog")
        assertFalse(source.exists())
        assertArrayEquals(byteArrayOf(1, 2, 3), destination.readBytes())
        assertEquals(2, migrationVersion(filesDir))
        manager.sweepRootGameFiles()
        assertTrue(destination.exists())
    }

    @Test
    fun `differing v0 collision remains retryable and sweep preserves source`() {
        val filesDir = temporaryFolder.newFolder("files-collision")
        val importRoot = temporaryFolder.newFolder("import-collision")
        val source = filesDir.resolve("descent.hog").apply { writeText("source") }
        val destination = importRoot.resolve("sets/default/descent.hog")
        requireNotNull(destination.parentFile).mkdirs()
        destination.writeText("destination")
        val manager = FileSetManager(filesDir, importRoot)

        manager.migrateDefaultSetIfNeeded()
        manager.sweepRootGameFiles()

        assertEquals("source", source.readText())
        assertEquals("destination", destination.readText())
        assertEquals(0, migrationVersion(filesDir))
    }

    @Test
    fun `v1 tree merge stops at differing file without hiding legacy set`() {
        val filesDir = temporaryFolder.newFolder("files-v1")
        val importRoot = temporaryFolder.newFolder("import-v1")
        filesDir.resolve("file_sets.json").writeText("{\"migration_version\":1}")
        val legacy = filesDir.resolve("sets/custom/mission.mn2").apply {
            requireNotNull(parentFile).mkdirs()
            writeText("legacy")
        }
        val destination = importRoot.resolve("sets/custom/mission.mn2").apply {
            requireNotNull(parentFile).mkdirs()
            writeText("current")
        }

        FileSetManager(filesDir, importRoot).migrateDefaultSetIfNeeded()

        assertEquals("legacy", legacy.readText())
        assertEquals("current", destination.readText())
        assertEquals(1, migrationVersion(filesDir))
    }

    @Test
    fun `v1 tree merge accepts identical collisions and transfers remaining files`() {
        val filesDir = temporaryFolder.newFolder("files-v1-identical")
        val importRoot = temporaryFolder.newFolder("import-v1-identical")
        filesDir.resolve("file_sets.json").writeText("{\"migration_version\":1}")
        val legacySet = filesDir.resolve("sets/custom").apply { mkdirs() }
        legacySet.resolve("same.hog").writeText("same")
        legacySet.resolve("new.mn2").writeText("new")
        val targetSet = importRoot.resolve("sets/custom").apply { mkdirs() }
        targetSet.resolve("same.hog").writeText("same")

        FileSetManager(filesDir, importRoot).migrateDefaultSetIfNeeded()

        assertFalse(filesDir.resolve("sets").exists())
        assertEquals("same", targetSet.resolve("same.hog").readText())
        assertEquals("new", targetSet.resolve("new.mn2").readText())
        assertEquals(2, migrationVersion(filesDir))
    }

    private fun migrationVersion(filesDir: java.io.File): Int =
        filesDir
            .resolve("file_sets.json")
            .takeIf { it.exists() }
            ?.let { JSONObject(it.readText()).optInt("migration_version", 0) }
            ?: 0
}
