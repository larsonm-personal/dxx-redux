package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class LevelMetadataResultCacheTest {
    @get:Rule
    val temporaryFolder = TemporaryFolder()

    @Test
    fun `published result is reused and source changes invalidate its identity`() {
        val root = temporaryFolder.newFolder("cache")
        val data = temporaryFolder.newFolder("data")
        File(data, "descent2.hog").writeText("base hog")
        File(data, "descent2.ham").writeText("base ham")
        File(data, "groupa.pig").writeText("base pig")
        val mission = File(data, "uneasy4.hog").apply { writeText("large level") }
        val target =
            LevelMetadataTarget(
                displayName = "Uneasy 4",
                game = "d2",
                sourceType = "hog",
                sourcePath = mission.absolutePath,
                archivePath = mission.absolutePath,
                dataDir = data.absolutePath,
                normalLevelFiles = listOf("uneasy4.rl2"),
            )
        val firstIdentity = checkNotNull(LevelMetadataResultCache.identify(target))
        val resultText = resultJson("d2")
        val result = LevelMetadataResult.fromJson(resultText)
        val routeFile = File(checkNotNull(root.parentFile), "d2x-redux/route-cache/g6/test.bin").apply {
            checkNotNull(parentFile).mkdirs()
            writeText("route")
        }

        assertEquals(
            true,
            LevelMetadataResultCache.publish(root, firstIdentity, target, 1, resultText, result),
        )
        assertNotNull(LevelMetadataResultCache.read(root, firstIdentity, target, 1))
        routeFile.delete()
        assertNull(LevelMetadataResultCache.read(root, firstIdentity, target, 1))

        mission.appendText(" changed")
        val changedIdentity = checkNotNull(LevelMetadataResultCache.identify(target))
        assertNotEquals(firstIdentity.key, changedIdentity.key)
        assertNull(LevelMetadataResultCache.read(root, changedIdentity, target, 1))

        File(data, "descent2.ham").appendText(" changed")
        val changedBaseIdentity = checkNotNull(LevelMetadataResultCache.identify(target))
        assertNotEquals(changedIdentity.key, changedBaseIdentity.key)
    }

    @Test
    fun `wrong game and incomplete results are not published`() {
        val root = temporaryFolder.newFolder("cache")
        val source = temporaryFolder.newFile("level.hog").apply { writeText("level") }
        val target =
            LevelMetadataTarget(
                displayName = "Level",
                game = "d2",
                sourceType = "hog",
                sourcePath = source.absolutePath,
                archivePath = source.absolutePath,
                normalLevelFiles = listOf("level.rl2"),
            )
        val identity = checkNotNull(LevelMetadataResultCache.identify(target))
        val wrongGame = resultJson("d1")

        assertEquals(
            false,
            LevelMetadataResultCache.publish(
                root,
                identity,
                target,
                1,
                wrongGame,
                LevelMetadataResult.fromJson(wrongGame),
            ),
        )
        assertEquals(
            false,
            LevelMetadataResultCache.publish(
                root,
                identity,
                target,
                2,
                resultJson("d2"),
                LevelMetadataResult.fromJson(resultJson("d2")),
            ),
        )
    }

    @Test
    fun `v1 result cache envelope is rejected`() {
        val root = temporaryFolder.newFolder("cache-v1")
        val source = temporaryFolder.newFile("legacy.hog").apply { writeText("level") }
        val target =
            LevelMetadataTarget(
                displayName = "Legacy",
                game = "d2",
                sourceType = "hog",
                sourcePath = source.absolutePath,
                archivePath = source.absolutePath,
                normalLevelFiles = listOf("legacy.rl2"),
            )
        val identity = checkNotNull(LevelMetadataResultCache.identify(target))
        val cacheFile = LevelMetadataResultCache.cacheFile(root, identity)
        checkNotNull(cacheFile.parentFile).mkdirs()
        cacheFile.writeText(
            JSONObject()
                .put("schema", "dxx-level-metadata-result-cache-v1")
                .put("route_cache_generation", ROUTE_METADATA_CACHE_GENERATION)
                .put("identity", identity.key)
                .put("result", JSONObject(resultJson("d2")))
                .toString(),
        )

        assertNull(LevelMetadataResultCache.read(root, identity, target, 1))
        assertEquals(false, cacheFile.exists())
    }

    @Test
    fun `loose sources bypass cache and all palette assets affect identity`() {
        val data = temporaryFolder.newFolder("liquid-assets")
        val archive = File(data, "mission.zip").apply { writeText("mission") }
        val loose = LevelMetadataTarget(
            displayName = "Loose", game = "d2", sourceType = "directory",
            sourcePath = data.absolutePath, dataDir = data.absolutePath,
        )
        assertNull(LevelMetadataResultCache.identify(loose))
        val target = loose.copy(archivePath = archive.absolutePath, sourcePath = null)
        val before = checkNotNull(LevelMetadataResultCache.identify(target))
        val pig = File(data, "WATER.PIG").apply { writeText("opaque") }
        val added = checkNotNull(LevelMetadataResultCache.identify(target))
        assertNotEquals(before.key, added.key)
        pig.writeText("transparent")
        assertNotEquals(added.key, checkNotNull(LevelMetadataResultCache.identify(target)).key)
        pig.delete()
        assertEquals(before.key, checkNotNull(LevelMetadataResultCache.identify(target)).key)
    }

    @Test
    fun `extracted mission results are reused and nested sidecars invalidate them`() {
        val root = temporaryFolder.newFolder("extracted-cache")
        val bundle = temporaryFolder.newFolder("extracted-mission")
        File(bundle, "mission.mn2").writeText("name = Extracted\nnum_levels = 1\nlevel.rl2\n")
        File(bundle, "mission.hog").writeText("level geometry")
        val sidecar = File(bundle, "nested/level.hxm").apply {
            checkNotNull(parentFile).mkdirs()
            writeText("robot definitions")
        }
        val target = LevelMetadataTarget(
            displayName = "Extracted", game = "d2", sourceType = "mission_files",
            sourcePath = bundle.absolutePath, missionFilename = "mission.mn2",
            hogFiles = listOf("mission.hog"), normalLevelFiles = listOf("level.rl2"),
        )
        val identity = checkNotNull(LevelMetadataResultCache.identify(target))
        val text = resultJson("d2")
        File(checkNotNull(root.parentFile), "d2x-redux/route-cache/g6/test.bin").apply {
            checkNotNull(parentFile).mkdirs()
            writeText("route")
        }
        assertEquals(true, LevelMetadataResultCache.publish(root, identity, target, 1, text, LevelMetadataResult.fromJson(text)))
        assertNotNull(LevelMetadataResultCache.read(root, checkNotNull(LevelMetadataResultCache.identify(target)), target, 1))
        sidecar.writeText("different robots")
        val changed = checkNotNull(LevelMetadataResultCache.identify(target))
        assertNotEquals(identity.key, changed.key)
        assertNull(LevelMetadataResultCache.read(root, changed, target, 1))
        val added = File(bundle, "level.pog").apply { writeText("replacement textures") }
        assertNotEquals(changed.key, checkNotNull(LevelMetadataResultCache.identify(target)).key)
        added.delete()
        assertEquals(changed.key, checkNotNull(LevelMetadataResultCache.identify(target)).key)
    }

    @Test
    fun `unknown liquid texture metadata is never published`() {
        val root = temporaryFolder.newFolder("incomplete-cache")
        val archive = temporaryFolder.newFile("unknown.zip").apply { writeText("mission") }
        val target = LevelMetadataTarget(
            displayName = "Unknown", game = "d2", sourceType = "zip", archivePath = archive.absolutePath,
        )
        val identity = checkNotNull(LevelMetadataResultCache.identify(target))
        val text = resultJson("d2").replace("\"secret_areas_complete\":true", "\"secret_areas_complete\":false")
        assertEquals(false, LevelMetadataResultCache.publish(
            root, identity, target, 1, text, LevelMetadataResult.fromJson(text),
        ))
    }

    @Test
    fun `completed scans preserve partial routes in the result cache`() {
        val root = temporaryFolder.newFolder("partial-route-cache")
        val source = temporaryFolder.newFile("partial.hog").apply { writeText("mission") }
        val target = LevelMetadataTarget(
            displayName = "Partial route", game = "d2", sourceType = "hog",
            sourcePath = source.absolutePath, archivePath = source.absolutePath,
            normalLevelFiles = listOf("level.rl2"),
        )
        val identity = checkNotNull(LevelMetadataResultCache.identify(target))
        for (readiness in listOf("next_ready", "partial")) {
            val document = JSONObject(resultJson("d2"))
            document.getJSONArray("levels").getJSONObject(0)
                .put("route_status", "partial")
                .put("route_problem", "gold key unreachable")
                .put("route_readiness", readiness)
                .put("route_cache_file", "")
            val text = document.toString()
            assertEquals(true, LevelMetadataResultCache.publish(root, identity, target, 1, text, LevelMetadataResult.fromJson(text)))
            val cached = checkNotNull(LevelMetadataResultCache.read(root, identity, target, 1))
            assertEquals("partial", cached.levels.single().routeStatus)
            assertEquals("gold key unreachable", cached.levels.single().routeProblem)
            assertEquals(readiness, cached.levels.single().routeReadiness)
        }
    }

    @Test
    fun `unfinished and interrupted route scans are not cached`() {
        val root = temporaryFolder.newFolder("unfinished-route-cache")
        val source = temporaryFolder.newFile("unfinished.hog").apply { writeText("mission") }
        val target = LevelMetadataTarget(
            displayName = "Unfinished route", game = "d2", sourceType = "hog",
            archivePath = source.absolutePath, normalLevelFiles = listOf("level.rl2"),
        )
        val identity = checkNotNull(LevelMetadataResultCache.identify(target))
        for ((readiness, failure) in listOf("calculating" to "", "failed" to "", "next_ready" to "preempted", "partial" to "budget_exhausted")) {
            val document = JSONObject(resultJson("d2"))
            document.getJSONArray("levels").getJSONObject(0)
                .put("route_readiness", readiness)
                .put("failure_kind", failure)
            val text = document.toString()
            assertEquals(false, LevelMetadataResultCache.publish(root, identity, target, 1, text, LevelMetadataResult.fromJson(text)))
            assertNull(LevelMetadataResultCache.read(root, identity, target, 1))
        }
    }

    private fun resultJson(game: String): String =
        """{"status":"ok","source":"Level","game":"$game","levels":[{"status":"ok","route_readiness":"complete","secret_areas_complete":true,"route_cache_file":"route-cache/g6/test.bin"}]}"""
}
