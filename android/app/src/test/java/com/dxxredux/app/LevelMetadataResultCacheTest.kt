package com.dxxredux.app

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

    private fun resultJson(game: String): String =
        """{"status":"ok","source":"Level","game":"$game","levels":[{"status":"ok","route_readiness":"complete","route_cache_file":"route-cache/g6/test.bin"}]}"""
}
