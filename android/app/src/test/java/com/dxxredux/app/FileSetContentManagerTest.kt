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
    fun realVertigoInventory() {
        val setDir = temporaryFolder.newFolder("real-vertigo")
        val source = File("../../game_data/extracted/VERTIGO")
        org.junit.Assume.assumeTrue("Local retail Vertigo fixture is unavailable", source.isDirectory)
        for (name in listOf("D2X.HOG", "D2X.MN2")) source.resolve("MISSIONS/$name").copyTo(File(setDir, name))
        for (name in listOf("D2X-H.MVL", "D2X-L.MVL")) source.resolve(name).copyTo(File(setDir, name))
        val manager = FileSetContentManager(setDir)
        val entry = manager.reconcile().entries.single()
        assertEquals(4, entry.files.size)
        val catalog = manager.buildMissionLaunchCatalog("d2")
        val mission = catalog.missions.single()
        val resources = catalog.resourcesFor(mission.key)
        for (name in listOf("D2X-H.MVL", "D2X-L.MVL")) {
            val movie = resources.single { it.virtualPath.endsWith(name, ignoreCase = true) }
            assertTrue(source.resolve(name).readBytes().contentEquals(movie.source.readBytes()))
        }
        assertTrue(manager.buildLaunchPaths("d2").isEmpty())
        assertTrue(catalog.resourcesFor(null).isEmpty())
        manager.setEnabled(entry.id, false)
        assertTrue(manager.buildMissionLaunchCatalog("d2").missions.isEmpty())
        assertTrue(manager.buildLaunchPaths("d2").isEmpty())
    }

    @Test
    fun laterMoviesJoinTheirMissionAndPreserveItsIdentityAndState() {
        val setDir = temporaryFolder.newFolder("later-movies")
        File(setDir, "other.txt").writeText("unrelated")
        File(setDir, "d2x.mn2").writeText("zname = Vertigo\nnum_levels = 1\nfirst.rl2\n")
        File(setDir, "first.rl2").writeText("level")
        val manager = FileSetContentManager(setDir)
        val mission = manager.reconcile().entries.single { it.displayName == "Vertigo" }
        manager.move(mission.id, 0)
        manager.setEnabled(mission.id, false)

        for (name in listOf("D2X-L.MVL", "d2x-h.mvl")) {
            File(setDir, name).writeText(name)
            val result = manager.reconcile()
            assertTrue(result.conflicts.isEmpty())
            assertEquals(listOf(mission.id), result.adoptedIds)
            assertEquals(2, result.entries.size)
            val updated = result.entries.first()
            assertEquals(mission.id, updated.id)
            assertFalse(updated.enabled)
            assertEquals(name, updated.files.single { it.name == name }.readText())
            assertTrue(manager.buildMissionLaunchCatalog("d2").missions.isEmpty())
            assertFalse(File(manager.buildProjection("d2"), "missions/$name").exists())
        }
        assertEquals(
            4,
            manager
                .reconcile()
                .entries
                .first()
                .files.size,
        )
        manager.setEnabled(mission.id, true)
        val catalog = manager.buildMissionLaunchCatalog("d2")
        val key = catalog.missions.single().key
        assertEquals(2, catalog.resourcesFor(key).count { it.virtualPath.endsWith(".mvl", true) })
        assertTrue(catalog.resourcesFor(null).isEmpty())
        assertTrue(manager.deleteEntry(mission.id))
        assertEquals(
            "other",
            manager
                .listEntries()
                .single()
                .displayName
                .lowercase(),
        )
        assertFalse(File(manager.buildProjection("d2"), "missions/D2X-L.MVL").exists())
    }

    @Test
    fun previouslyImportedMoviesJoinWhenTheirMissionArrives() {
        val setDir = temporaryFolder.newFolder("movies-first")
        File(setDir, "missions").mkdirs()
        File(setDir, "missions/d2x-l.mvl").writeText("low")
        File(setDir, "d2x-h.mvl").writeText("high")
        File(setDir, "different-h.mvl").writeText("unrelated")
        val manager = FileSetContentManager(setDir)
        assertEquals(3, manager.reconcile().entries.size)
        File(setDir, "d2x.mn2").writeText("zname = Vertigo\nnum_levels = 1\nfirst.rl2\n")
        File(setDir, "d2x.hog").writeText("mission")
        val result = manager.reconcile()
        assertTrue(result.conflicts.isEmpty())
        assertEquals(2, result.entries.size)
        val mission = result.entries.single { it.displayName == "Vertigo" }
        assertEquals(4, mission.files.size)
        assertTrue(mission.virtualPaths.all { it.startsWith("missions/") })
        assertEquals(2, FileSetContentManager(setDir).reconcile().entries.size)
    }

    @Test
    fun reconciliationFinishesRetiringAMovieAlreadyPublishedIntoItsMission() {
        val setDir = temporaryFolder.newFolder("retry-movie")
        File(setDir, "d2x-h.mvl").writeText("high")
        val manager = FileSetContentManager(setDir)
        val movie = manager.reconcile().entries.single()
        val movieDirectory = File(setDir, ".content/entries/${movie.id}")
        val retainedSource = temporaryFolder.newFolder("retained-movie-source")
        movieDirectory.copyRecursively(retainedSource, overwrite = true)
        File(setDir, "d2x.mn2").writeText("zname = Vertigo\nnum_levels = 1\nfirst.rl2\n")
        val mission = manager.reconcile().entries.single()
        manager.setEnabled(mission.id, false)
        // Recreate interruption after the mission manifest commit, before source retirement
        retainedSource.copyRecursively(movieDirectory)
        val result = FileSetContentManager(setDir).reconcile()
        assertTrue(result.conflicts.isEmpty())
        val recovered = result.entries.single()
        assertEquals(mission.id, recovered.id)
        assertFalse(recovered.enabled)
        assertEquals(2, recovered.files.size)
        assertEquals("high", recovered.files.single { it.extension == "mvl" }.readText())
        assertFalse(movieDirectory.exists())
    }

    @Test
    fun conflictingMovieImportIsKeptSeparateWithoutChangingMissionBytes() {
        val setDir = temporaryFolder.newFolder("conflicting-movie")
        File(setDir, "d2x.mn2").writeText("zname = Vertigo\nnum_levels = 1\nfirst.rl2\n")
        File(setDir, "d2x-h.mvl").writeText("original")
        val manager = FileSetContentManager(setDir)
        val mission = manager.reconcile().entries.single()
        File(setDir, "d2x-h.mvl").writeText("replacement")
        val result = manager.reconcile()
        assertEquals(2, result.entries.size)
        assertTrue(result.conflicts.any { it.contains("movie content conflicts") })
        assertEquals(
            "original",
            result.entries
                .single {
                    it.id == mission.id
                }.files
                .single { it.extension == "mvl" }
                .readText(),
        )
        assertEquals(
            "replacement",
            result.entries
                .single { it.id != mission.id }
                .files
                .single()
                .readText(),
        )
        assertEquals(2, manager.reconcile().entries.size)
    }

    @Test
    fun ambiguousMovieCompanionStaysSeparate() {
        val setDir = temporaryFolder.newFolder("ambiguous-movie")
        File(setDir, "d2x.mn2").writeText("zname = First\nnum_levels = 1\nfirst.rl2\n")
        val manager = FileSetContentManager(setDir)
        manager.reconcile()
        File(setDir, "d2x.mn2").writeText("zname = Second\nnum_levels = 1\nsecond.rl2\n")
        File(setDir, "d2x.hog").writeText("second mission")
        assertEquals(2, manager.reconcile().entries.size)
        File(setDir, "d2x-h.mvl").writeText("ambiguous")
        val result = manager.reconcile()
        assertEquals(3, result.entries.size)
        assertTrue(result.conflicts.any { it.contains("matches more than one mission") })
    }

    @Test
    fun descriptorOnlyVertigoDoesNotBlockOtherMissionsOrPublishAssetsGlobally() {
        val setDir = temporaryFolder.newFolder("incomplete-vertigo")
        File(setDir, "d2x.mn2").writeText("zname = Descent 2: Vertigo\nnum_levels = 1\nd2xlvl01.rl2\n")
        File(setDir, "playable.mn2").writeText("name = Playable\nnum_levels = 1\nfirst.rl2\n")
        File(setDir, "first.rl2").writeText("level")
        val manager = FileSetContentManager(setDir)
        manager.reconcile()
        val catalog = manager.buildMissionLaunchCatalog("d2")
        val vertigo = catalog.missions.single { it.title.contains("Vertigo") }
        val playable = catalog.missions.single { it.title == "Playable" }
        assertTrue(vertigo.activationError.contains("Missing levels: d2xlvl01.rl2"))
        assertEquals("", playable.activationError)
        assertTrue(manager.buildLaunchPaths("d2").isEmpty())
        assertTrue(catalog.resourcesFor(null).isEmpty())
        val publication =
            catalog.publish(
                temporaryFolder.newFolder("incomplete-game"),
                "global",
                mapOf(vertigo.key to ""),
            )
        val entries = org.json.JSONObject(publication.manifest).getJSONArray("entries")
        val error =
            (0 until entries.length())
                .map { entries.getJSONObject(it) }
                .single { it.getString("descriptor").endsWith("d2x.mn2") }
        assertEquals(vertigo.activationError, error.getString("activation_error"))
        assertTrue(File(publication.discoveryDir, error.getString("alias")).isFile)
    }

    @Test
    fun unreadableMissionHogRemainsScopedAndBlocksOnlyItsMission() {
        val setDir = temporaryFolder.newFolder("unreadable-vertigo")
        File(setDir, "d2x.mn2").writeText("zname = Descent 2: Vertigo\nnum_levels = 1\nd2xlvl01.rl2\n")
        File(setDir, "D2X.HOG").writeText("unreadable archive")
        val manager = FileSetContentManager(setDir)
        manager.reconcile()
        assertTrue(
            manager
                .buildMissionLaunchCatalog("d2")
                .missions
                .single()
                .activationError
                .isNotEmpty(),
        )
        assertTrue(manager.buildLaunchPaths("d2").isEmpty())
    }

    @Test
    fun looseMissionIsDiscoverableWithoutPublishingItsAssetsGlobally() {
        val setDir = temporaryFolder.newFolder("scoped-loose")
        File(setDir, "panic.mn2").writeText("name = Panic\nbriefing = panic.tex\nnum_levels = 1\npanic01.rl2\n")
        File(setDir, "panic01.rl2").writeText("level")
        File(setDir, "panic.tex").writeText("briefing")
        val manager = FileSetContentManager(setDir)
        val entry = manager.reconcile().entries.single()
        assertTrue(manager.buildLaunchPaths("d2").isEmpty())
        val catalog = manager.buildMissionLaunchCatalog("d2")
        val mission = catalog.missions.single()
        assertEquals("content/${entry.id}", mission.key.owner)
        assertTrue(catalog.resourcesFor(mission.key).any { it.virtualPath.endsWith("panic01.rl2") })
        assertTrue(catalog.resourcesFor(null).isEmpty())
        manager.setEnabled(entry.id, false)
        assertTrue(manager.buildMissionLaunchCatalog("d2").missions.isEmpty())
    }

    @Test
    fun dxaContainingMissionsIsScopedEvenWithoutAnExternalDescriptor() {
        val setDir = temporaryFolder.newFolder("embedded-mission")
        java.util.zip.ZipOutputStream(File(setDir, "campaign.dxa").outputStream()).use { zip ->
            for ((path, text) in mapOf(
                "missions/campaign.mn2" to "name = Campaign\nnum_levels = 1\nfirst.rl2\n",
                "missions/first.rl2" to "level",
                "descent2.s22" to "campaign sound bank",
            )) {
                zip.putNextEntry(java.util.zip.ZipEntry(path))
                zip.write(text.toByteArray())
                zip.closeEntry()
            }
        }
        val manager = FileSetContentManager(setDir)
        val entry = manager.reconcile().entries.single()
        assertTrue(manager.buildLaunchPaths("d2").isEmpty())
        val catalog = manager.buildMissionLaunchCatalog("d2")
        val key = catalog.missions.single().key
        assertEquals("content/${entry.id}/campaign.dxa", key.owner)
        assertTrue(catalog.resourcesFor(key).any { it.virtualPath == "descent2.s22" })
    }

    @Test
    fun vertigoDescriptorAndHogStayTogetherInLaunchProjection() {
        val setDir = temporaryFolder.newFolder("vertigo")
        File(setDir, "descent2.hog").writeText("base")
        File(setDir, "D2X.HOG").writeText("expansion")
        File(setDir, "d2x.mn2").writeText("zname = Descent 2: Vertigo\nnum_levels = 1\nd2xlvl01.rl2\n")
        val manager = FileSetContentManager(setDir)
        val result = manager.reconcile()
        val entry = result.entries.single()
        assertEquals(setOf("missions/D2X.HOG", "missions/d2x.mn2"), entry.virtualPaths.toSet())
        assertTrue(result.conflicts.isEmpty())
        val projection = manager.buildProjection("d2")
        assertEquals("expansion", File(projection, "missions/D2X.HOG").readText())
        assertTrue(File(projection, "missions/d2x.mn2").isFile)
        assertTrue(File(setDir, "descent2.hog").isFile)
        assertFalse(File(setDir, "D2X.HOG").exists())
        assertEquals(
            entry.id,
            manager
                .reconcile()
                .entries
                .single()
                .id,
        )
        manager.setEnabled(entry.id, false)
        val disabled = manager.buildProjection("d2")
        assertFalse(File(disabled, "missions/D2X.HOG").exists())
        assertFalse(File(disabled, "missions/d2x.mn2").exists())
    }

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
    fun groupedSourceWriteKeepsRelatedMissionFilesUnderOneOwner() {
        val setDir = temporaryFolder.newFolder("grouped-write")
        val manager = FileSetContentManager(setDir)

        manager.writeSourceFiles(
            listOf(
                "panic.mn2" to "name = Panic\nnum_levels = 1\npanic01.rl2\n",
                "panic.hog" to "mission",
            ),
        )
        val entry = manager.reconcile().entries.single()

        assertEquals(FileSetContentCatalog.KIND_LOOSE_MISSION, entry.kind)
        assertEquals(listOf("missions/panic.hog", "missions/panic.mn2"), entry.virtualPaths)
    }

    @Test
    fun reconcileRemovesMatchingRootAliasForOwnedMissionFile() {
        val setDir = temporaryFolder.newFolder("mission-alias")
        val manager = FileSetContentManager(setDir)
        manager.writeSourceFiles(
            listOf(
                "panic.mn2" to "name = Panic\nnum_levels = 1\npanic01.rl2\n",
                "panic.hog" to "mission",
            ),
        )
        val entry = manager.reconcile().entries.single()
        entry.files.single { it.name == "panic.mn2" }.copyTo(File(setDir, "panic.mn2"))

        val result = manager.reconcile()

        assertEquals(listOf("panic.mn2"), result.removedDuplicatePaths)
        assertFalse(File(setDir, "panic.mn2").exists())
        assertEquals(entry.id, result.entries.single().id)
    }

    @Test
    fun enableStateControlsProjectionAndRepairsMissingState() {
        val setDir = temporaryFolder.newFolder("toggle")
        File(setDir, "extra.hog").writeText("extra")
        val manager = FileSetContentManager(setDir)
        val id =
            manager
                .reconcile()
                .entries
                .single()
                .id

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
    fun demoGroupToggleResetsEveryDemoWithoutChangingOtherContent() {
        val setDir = temporaryFolder.newFolder("demo-toggle")
        File(setDir, "demos/one.dem").apply {
            parentFile?.mkdirs()
            writeText("one")
        }
        File(setDir, "demos/two.dem").writeText("two")
        File(setDir, "extra.hog").writeText("other")
        val manager = FileSetContentManager(setDir)
        manager.reconcile()

        manager.setKindEnabled(FileSetContentCatalog.KIND_DEMO, false)
        assertTrue(manager.listEntries().filter { it.kind == FileSetContentCatalog.KIND_DEMO }.none { it.enabled })
        assertTrue(manager.listEntries().single { it.kind != FileSetContentCatalog.KIND_DEMO }.enabled)

        manager.setKindEnabled(FileSetContentCatalog.KIND_DEMO, true)
        assertTrue(manager.listEntries().filter { it.kind == FileSetContentCatalog.KIND_DEMO }.all { it.enabled })
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
        val id =
            manager
                .reconcile()
                .entries
                .single()
                .id
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
