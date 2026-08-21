package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors

class FileSetContentManagerTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    @Test
    fun reconcileDoesNotRecreateDeletedSet() {
        val setDir = temporaryFolder.newFolder("deleted")
        val manager = FileSetContentManager(setDir)
        assertTrue(setDir.deleteRecursively())

        val result = manager.reconcile()

        assertTrue(result.entries.isEmpty())
        assertFalse(setDir.exists())
    }

    @Test
    fun reconcileAdoptsLooseMissionAndProjectionPreservesVirtualPaths() {
        val setDir = temporaryFolder.newFolder("default")
        File(setDir, "descent2.hog").writeText("base")
        File(setDir, "panic.mn2").writeText(
            "name = Vertigo Series\nbriefing = panic.tex\nnum_levels = 1\npanic01.rl2\n",
        )
        File(setDir, "panic.hog").writeText("mission")
        File(setDir, "panic.tex").writeText("briefing")
        File(setDir, "panic01.rl2").writeText("level")
        val manager = FileSetContentManager(setDir)

        val result = manager.reconcile()
        val entry = result.entries.single()

        assertEquals(listOf(entry.id), result.adoptedIds)
        assertTrue(result.conflicts.isEmpty())
        assertEquals(
            setOf("missions/panic.hog", "missions/panic.mn2", "missions/panic.tex", "missions/panic01.rl2"),
            entry.virtualPaths.toSet(),
        )
        assertTrue(File(setDir, "descent2.hog").isFile)
        assertFalse(File(setDir, "panic.hog").exists())
        assertTrue(File(setDir, ".content/entries/${entry.id}/entry.json").isFile)
        assertTrue(File(setDir, "content_state.json").isFile)

        val projection = manager.buildProjection("d2")
        assertTrue(File(projection, "missions/panic.hog").isFile)
        assertTrue(File(projection, "missions/panic.mn2").isFile)
        assertFalse(File(projection, "descent2.hog").exists())

        val second = manager.reconcile()
        assertTrue(second.adoptedIds.isEmpty())
        assertEquals(entry.id, second.entries.single().id)
    }

    @Test
    fun enableStateControlsProjectionAndRepairsMissingState() {
        val setDir = temporaryFolder.newFolder("toggle")
        File(setDir, "extra.hog").writeText("extra")
        val manager = FileSetContentManager(setDir)
        val id = manager.reconcile().entries.single().id

        manager.setEnabled(id, false)
        assertFalse(manager.listEntries().single().enabled)
        assertFalse(manager.buildProjection("d2").walkTopDown().any { it.isFile })

        manager.setEnabled(id, true)
        assertTrue(manager.listEntries().single().enabled)
        assertTrue(File(manager.buildProjection("d2"), "extra.hog").isFile)

        File(setDir, "content_state.json").delete()
        val repaired = manager.listEntries().single()
        assertTrue(repaired.enabled)
        assertEquals(0, repaired.order)
        assertTrue(File(setDir, "content_state.json").isFile)
    }

    @Test
    fun reconciliationRemovesMatchingDuplicateButPreservesConflict() {
        val setDir = temporaryFolder.newFolder("duplicates")
        File(setDir, "extra.hog").writeText("owned")
        val manager = FileSetContentManager(setDir)
        val entry = manager.reconcile().entries.single()
        val payload = entry.files.single()

        payload.copyTo(File(setDir, "extra.hog"))
        val duplicateResult = manager.reconcile()
        assertEquals(listOf("extra.hog"), duplicateResult.removedDuplicatePaths)
        assertFalse(File(setDir, "extra.hog").exists())

        File(setDir, "extra.hog").writeText("different")
        val conflictResult = manager.reconcile()
        assertTrue(File(setDir, "extra.hog").isFile)
        assertTrue(conflictResult.conflicts.any { "differs" in it || "collide" in it })
        assertEquals(entry.id, conflictResult.entries.single().id)
    }

    @Test
    fun deleteRetiresPayloadStateAndProjectionTogether() {
        val setDir = temporaryFolder.newFolder("delete")
        File(setDir, "extra.hog").writeText("extra")
        val manager = FileSetContentManager(setDir)
        val id = manager.reconcile().entries.single().id
        assertTrue(File(manager.buildProjection("d2"), "extra.hog").isFile)

        assertTrue(manager.deleteEntry(id))

        assertTrue(manager.listEntries().isEmpty())
        assertFalse(File(setDir, ".content/entries/$id").exists())
        assertFalse(File(setDir, ".content_projection/d2/extra.hog").exists())
        assertFalse(manager.deleteEntry(id))
    }

    @Test
    fun launchPathsCombineProjectionAndDirectDxaMounts() {
        val filesDir = temporaryFolder.newFolder("launch")
        val setDir = File(filesDir, "sets/default").apply { mkdirs() }
        File(setDir, "extra.hog").writeText("mission")
        File(setDir, "overlay.dxa").writeText("archive")
        val manager = FileSetContentManager(setDir)
        val result = manager.reconcile()

        val paths = manager.buildLaunchPaths("d2")
        ModManager(filesDir).writeEnabledModPaths("d2", contentPaths = paths)
        val written = File(filesDir, "d2x-redux/.active_mod_paths").readLines()

        assertEquals(2, result.entries.size)
        assertEquals(2, paths.size)
        assertTrue(paths.first().endsWith("overlay.dxa"))
        assertEquals(paths, written)
        assertTrue(File(paths.last(), "extra.hog").isFile)
    }

    @Test
    fun looseMusicIsOwnedAndResolvedOnlyWhileEnabled() {
        val setDir = temporaryFolder.newFolder("music")
        File(setDir, "custom.ogg").writeText("music")
        val manager = FileSetContentManager(setDir)

        val result = manager.reconcile()
        val entry = result.entries.single()

        assertEquals(FileSetContentCatalog.KIND_MUSIC, entry.kind)
        assertFalse(File(setDir, "custom.ogg").exists())
        assertTrue(manager.resolveFile("custom.ogg")?.isFile == true)
        manager.setEnabled(entry.id, false)
        assertEquals(null, manager.resolveFile("custom.ogg"))
        assertTrue(manager.resolveFile("custom.ogg", enabledOnly = false)?.isFile == true)
    }

    @Test
    fun separateManagersSerializeReconciliationForTheSameSet() {
        val setDir = temporaryFolder.newFolder("concurrent")
        File(setDir, "extra.hog").writeText("extra")
        val start = CountDownLatch(1)
        val executor = Executors.newFixedThreadPool(2)
        try {
            val results =
                listOf(FileSetContentManager(setDir), FileSetContentManager(setDir)).map { manager ->
                    executor.submit<FileSetContentReconcileResult> {
                        start.await()
                        manager.reconcile()
                    }
                }
            start.countDown()
            val completed = results.map { it.get() }

            assertEquals(1, completed.sumOf { it.adoptedIds.size })
            assertTrue(completed.all { it.conflicts.isEmpty() })
            assertEquals(1, FileSetContentManager(setDir).listEntries().size)
            assertFalse(File(setDir, "extra.hog").exists())
        } finally {
            executor.shutdownNow()
        }
    }

    @Test
    fun invalidOwnerManifestIsRecoveredAsVisibleDeletableContent() {
        val setDir = temporaryFolder.newFolder("recovery")
        val owner = File(setDir, ".content/entries/not-a-valid-id")
        File(owner, "payload/missions").mkdirs()
        File(owner, "payload/missions/recovered.hog").writeText("payload")
        File(owner, "entry.json").writeText("not json")
        val manager = FileSetContentManager(setDir)

        val result = manager.reconcile()
        val entry = result.entries.single()

        assertEquals(FileSetContentCatalog.KIND_OTHER, entry.kind)
        assertEquals(listOf("missions/recovered.hog"), entry.virtualPaths)
        assertTrue(entry.problem?.startsWith("Recovered after invalid content manifest") == true)
        assertTrue(result.conflicts.any { "Recovered content" in it })
        assertTrue(manager.deleteEntry(entry.id))
        assertTrue(manager.listEntries().isEmpty())
    }
}
