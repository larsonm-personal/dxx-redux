package com.dxxredux.app

import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

/**
 * Tests for [ImportLocationManager.migrate] -- the byte-verified copy/sweep
 * path used when the user changes the imported-files location.  These tests
 * exercise just the file-IO logic; the [Context]-dependent volume listing
 * is covered manually on-device.
 */
class ImportLocationMigrateTest {
    @get:Rule
    val tmp = TemporaryFolder()

    private lateinit var filesDir: File
    private lateinit var src: File
    private lateinit var dst: File
    private lateinit var mgr: ImportLocationManager

    @Before
    fun setUp() {
        filesDir = tmp.newFolder("filesDir")
        src = tmp.newFolder("src")
        dst = File(tmp.newFolder("dstParent"), "dst")
        mgr = ImportLocationManager(filesDir)
    }

    @After
    fun tearDown() {
        // TemporaryFolder cleans up automatically.
    }

    private fun writeFile(
        root: File,
        relative: String,
        contents: ByteArray,
    ): File {
        val f = File(root, relative)
        f.parentFile?.mkdirs()
        f.writeBytes(contents)
        return f
    }

    private fun totalBytes(root: File): Long =
        if (!root.exists()) {
            0
        } else {
            root.walkTopDown().filter { it.isFile }.sumOf { it.length() }
        }

    @Test
    fun migrate_copiesAllFilesAndDeletesSource() {
        writeFile(src, "sets/default/descent2.hog", ByteArray(1024) { it.toByte() })
        writeFile(src, "sets/default/descent2.pig", ByteArray(2048) { (it + 1).toByte() })
        writeFile(src, "sets/mod/missions/mod.mn2", ByteArray(64) { it.toByte() })
        val expectedBytes = totalBytes(src)

        val result = mgr.migrate(src, dst)

        assertEquals(ImportLocationManager.MigrateResult.Success, result)
        assertFalse("source should be gone", src.exists())
        assertEquals(expectedBytes, totalBytes(dst))
        assertTrue(File(dst, "sets/default/descent2.hog").isFile)
        assertTrue(File(dst, "sets/default/descent2.pig").isFile)
        assertTrue(File(dst, "sets/mod/missions/mod.mn2").isFile)
    }

    @Test
    fun migrate_emptySourceIsSuccess() {
        val result = mgr.migrate(src, dst)
        assertEquals(ImportLocationManager.MigrateResult.Success, result)
        assertTrue(dst.exists())
    }

    @Test
    fun migrate_sameSrcAndDstIsNoop() {
        writeFile(src, "sets/default/a.hog", ByteArray(16))
        val result = mgr.migrate(src, src)
        assertEquals(ImportLocationManager.MigrateResult.Success, result)
        assertTrue(File(src, "sets/default/a.hog").isFile)
    }

    @Test
    fun migrate_clearsInProgressMarkerOnSuccess() {
        writeFile(src, "sets/default/a.hog", ByteArray(8))
        mgr.migrate(src, dst)
        assertFalse(File(dst, ImportLocationManager.IN_PROGRESS_MARKER).exists())
    }

    @Test
    fun overrideRoundTrip() {
        assertFalse(mgr.isOverrideActive())
        assertEquals(File(filesDir, "imported").absolutePath, mgr.getActiveRoot().absolutePath)

        val newRoot = tmp.newFolder("alt")
        mgr.setOverride(newRoot)

        val reloaded = ImportLocationManager(filesDir)
        assertTrue(reloaded.isOverrideActive())
        assertEquals(newRoot.absolutePath, reloaded.getActiveRoot().absolutePath)

        reloaded.clearOverride()
        assertFalse(ImportLocationManager(filesDir).isOverrideActive())
    }

    @Test
    fun progressCallbackReportsAdvancingByteCounts() {
        writeFile(src, "sets/default/a.hog", ByteArray(128 * 1024))
        writeFile(src, "sets/default/b.hog", ByteArray(64 * 1024))
        val total = totalBytes(src)

        val seen = mutableListOf<Long>()
        var lastTotal = -1L
        val result =
            mgr.migrate(src, dst) { copied, t ->
                seen.add(copied)
                lastTotal = t
            }

        assertEquals(ImportLocationManager.MigrateResult.Success, result)
        assertTrue("progress was reported", seen.isNotEmpty())
        assertEquals(total, lastTotal)
        assertTrue(seen.last() <= total)
        // Counts should be non-decreasing.
        for (i in 1 until seen.size) {
            assertTrue("non-decreasing: ${seen[i - 1]} -> ${seen[i]}", seen[i] >= seen[i - 1])
        }
    }

    @Test
    fun migrate_preservesDestinationOwnedFilesAndAcceptsIdenticalCollisions() {
        writeFile(dst, "destination-only.txt", "keep".toByteArray())
        writeFile(dst, "sets/default/same.hog", "same".toByteArray())
        writeFile(src, "sets/default/same.hog", "same".toByteArray())
        writeFile(src, "sets/default/new.pig", "new".toByteArray())

        assertEquals(ImportLocationManager.MigrateResult.Success, mgr.migrate(src, dst))
        assertEquals("keep", File(dst, "destination-only.txt").readText())
        assertEquals("same", File(dst, "sets/default/same.hog").readText())
        assertEquals("new", File(dst, "sets/default/new.pig").readText())
        assertFalse(src.exists())
    }

    @Test
    fun migrate_rejectsDifferingCollisionBeforeChangingEitherTree() {
        writeFile(dst, "destination-only.txt", "keep".toByteArray())
        writeFile(dst, "sets/default/game.hog", "destination".toByteArray())
        writeFile(src, "sets/default/game.hog", "source".toByteArray())
        writeFile(src, "sets/default/new.pig", "new".toByteArray())

        val result = mgr.migrate(src, dst)

        assertTrue(result is ImportLocationManager.MigrateResult.Failure)
        assertEquals("destination", File(dst, "sets/default/game.hog").readText())
        assertEquals("keep", File(dst, "destination-only.txt").readText())
        assertFalse(File(dst, "sets/default/new.pig").exists())
        assertTrue(File(src, "sets/default/game.hog").exists())
    }

    @Test
    fun migrate_activationFailureRollsBackOnlyAttemptOwnedFiles() {
        writeFile(dst, "destination-only.txt", "keep".toByteArray())
        writeFile(src, "sets/default/game.hog", "source".toByteArray())
        var sawCommittedCopy = false

        val result =
            mgr.migrate(
                src,
                dst,
                beforeSourceRetire = {
                    sawCommittedCopy = File(dst, "sets/default/game.hog").readText() == "source" && src.exists()
                    error("injected activation failure")
                },
            )

        assertTrue(result is ImportLocationManager.MigrateResult.Failure)
        assertTrue(sawCommittedCopy)
        assertTrue(src.exists())
        assertEquals("keep", File(dst, "destination-only.txt").readText())
        assertFalse(File(dst, "sets/default/game.hog").exists())
    }

    @Test
    fun fileSetManager_usesImportRootForSetsDir() {
        // FileSetManager(filesDir) defaults to ImportLocationManager(filesDir).getActiveRoot()
        // which is filesDir/imported when no override is set.
        val fsm = FileSetManager(filesDir)
        val setDir = fsm.getSetDir(FileSetManager.DEFAULT_SET)
        assertTrue(setDir.absolutePath.endsWith("imported${File.separator}sets${File.separator}default"))
    }

    @Test
    fun fileSetManager_followsOverride() {
        val alt = tmp.newFolder("alt-import")
        mgr.setOverride(alt)

        val fsm = FileSetManager(filesDir)
        val setDir = fsm.getSetDir(FileSetManager.DEFAULT_SET)
        assertEquals(File(alt, "sets/default").absolutePath, setDir.absolutePath)
    }
}
