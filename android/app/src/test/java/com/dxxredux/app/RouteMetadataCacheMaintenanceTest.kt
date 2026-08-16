package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class RouteMetadataCacheMaintenanceTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    @Test
    fun pruneRemovesObsoleteGenerationsAndCompletedCheckpointsOnly() {
        val filesDir = temporaryFolder.newFolder("files")
        val root = File(filesDir, "d2x-redux/route-cache")
        File(root, "4").mkdirs()
        File(root, "4/old.bin").writeText("old")
        File(root, "g5").mkdirs()
        File(root, "g5/old.bin").writeText("old")
        val current = File(root, "g$ROUTE_METADATA_CACHE_GENERATION").apply { mkdirs() }
        File(current, "done.bin").writeText("record")
        File(current, "done.bin.samples-000000").writeText("checkpoint")
        File(current, "partial.bin.samples-000000").writeText("checkpoint")
        File(current, "write.bin.tmp-123").writeText("temporary")
        File(root, "custom").mkdirs()
        File(root, "custom/keep.bin").writeText("keep")

        val result = RouteMetadataCacheMaintenance.prune(filesDir)

        assertEquals(2, result.removedDirectories)
        assertEquals(2, result.removedFiles)
        assertFalse(File(root, "4").exists())
        assertFalse(File(root, "g5").exists())
        assertFalse(File(current, "done.bin.samples-000000").exists())
        assertFalse(File(current, "write.bin.tmp-123").exists())
        assertTrue(File(current, "done.bin").isFile)
        assertTrue(File(current, "partial.bin.samples-000000").isFile)
        assertTrue(File(root, "custom/keep.bin").isFile)
    }
}
