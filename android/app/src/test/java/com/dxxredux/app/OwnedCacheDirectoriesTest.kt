package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class OwnedCacheDirectoriesTest {
    @Test
    fun deleteRefusesDirectoriesOutsideTheOwnedRoot() {
        val base = testRoot("delete-scope")
        val root = File(base, "preview")
        val owned = OwnedCacheDirectories.create(root)
        val outside = File(base, owned.name).apply { mkdirs() }
        File(outside, "keep.txt").writeText("keep")

        assertFalse(OwnedCacheDirectories.delete(root, outside))
        assertTrue(File(outside, "keep.txt").isFile)
        assertTrue(OwnedCacheDirectories.delete(root, owned))
        assertFalse(owned.exists())
    }

    @Test
    fun pruneDeletesOnlyOldUuidChildren() {
        val base = testRoot("prune")
        val root = File(base, "preview")
        val old = OwnedCacheDirectories.create(root)
        val fresh = OwnedCacheDirectories.create(root)
        val unrelated = File(root, "do-not-delete").apply { mkdirs() }
        check(old.setLastModified(1_000L))
        check(fresh.setLastModified(9_000L))
        check(unrelated.setLastModified(1_000L))

        assertEquals(1, OwnedCacheDirectories.prune(root, maxAgeMs = 5_000L, nowMs = 10_000L))
        assertFalse(old.exists())
        assertTrue(fresh.isDirectory)
        assertTrue(unrelated.isDirectory)
    }

    @Test
    fun atomicWritePublishesOnlyTheFinalFile() {
        val root = File(testRoot("atomic-write"), "preview")
        val directory = OwnedCacheDirectories.create(root)

        val target = OwnedCacheDirectories.writeUtf8Atomically(directory, "request.json", "{\"ready\":true}\n")

        assertEquals("{\"ready\":true}\n", target.readText())
        assertFalse(File(directory, "request.json.tmp").exists())
    }

    private fun testRoot(name: String): File =
        File("build/test-owned-cache-directories/$name").absoluteFile.apply {
            deleteRecursively()
            mkdirs()
        }
}
