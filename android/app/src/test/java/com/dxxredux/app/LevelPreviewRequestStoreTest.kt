package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class LevelPreviewRequestStoreTest {
    @Test
    fun directHogRequestMountsAdjacentMissionDescriptor() {
        val root = testRoot("adjacent-descriptor")
        val cacheDir = File(root, "cache").apply { mkdirs() }
        val dataDir = File(root, "data").apply { mkdirs() }
        val hog = File(dataDir, "chaos.hog").apply { writeBytes(byteArrayOf(1, 2, 3)) }
        File(dataDir, "chaos.msn").writeText(
            "name = Total Chaos\ntype = anarchy\nnum_levels = 1\nchaos1.rdl\n",
        )
        val metadata =
            GameFileMetadata.Summary(
                format = "HOG",
                scope = "Mission archive",
                game = "D1",
                detailRows = emptyList(),
                categories = emptyList(),
                contents = listOf(GameFileMetadata.EntrySummary("chaos1.rdl", 1, "D1 level")),
            )
        val target = requireNotNull(LevelMetadataTargets.directFile(hog, dataDir, metadata))

        val launch = LevelPreviewRequestStore.create(cacheDir, target, levelRow("chaos1.rdl"))
        val request = JSONObject(launch.requestFile.readText())

        assertEquals(hog.canonicalPath, request.getString("hog_path"))
        assertEquals(dataDir.canonicalPath, request.getString("extra_data_dir"))
        assertEquals("chaos.msn", request.getString("mission_filename"))
        assertEquals("anarchy", request.getString("mission_type"))
    }

    @Test
    fun requestUsesCanonicalInputsAndIsolatedWriteDirectory() {
        val root = testRoot("create")
        val cacheDir = File(root, "cache").apply { mkdirs() }
        val dataDir = File(root, "data").apply { mkdirs() }
        val level = File(dataDir, "preview.rl2").apply { writeBytes(byteArrayOf(1, 2, 3)) }
        val target =
            LevelMetadataTarget(
                displayName = level.name,
                game = GameFileFormats.GAME_D2,
                sourceType = "level",
                sourcePath = level.path,
                dataDir = dataDir.path,
                levelFile = level.name,
            )
        val row = levelRow(level.name)

        val launch = LevelPreviewRequestStore.create(cacheDir, target, row)
        val request = JSONObject(launch.requestFile.readText())
        val writeDir = File(request.getString("preview_write_dir"))

        assertEquals("dxx-level-preview-request-v1", request.getString("schema"))
        assertEquals(level.canonicalPath, request.getString("source_path"))
        assertEquals(dataDir.canonicalPath, request.getString("data_dir"))
        assertEquals(level.name, request.getString("level_file"))
        assertEquals(1, request.getInt("level_num"))
        assertFalse(request.getBoolean("secret_level"))
        assertTrue(writeDir.isDirectory)
        assertEquals(launch.requestFile.parentFile!!.canonicalFile, writeDir.parentFile!!.canonicalFile)
        val runtime = LevelPreviewRequestStore.validateForLaunch(cacheDir, launch.requestFile.absolutePath, "d2")
        assertEquals(dataDir.canonicalFile, runtime.dataDir)
        assertTrue(LevelPreviewRequestStore.delete(cacheDir, launch.requestFile.absolutePath))
        assertFalse(launch.requestFile.parentFile!!.exists())
    }

    @Test
    fun requestMountsManagedDescriptorDirectoryAlongsideRootHog() {
        val root = testRoot("managed-descriptor")
        val cacheDir = File(root, "cache").apply { mkdirs() }
        val dataDir = File(root, "data").apply { mkdirs() }
        val descriptorDir = File(dataDir, ".content/vertigo/payload/missions").apply { mkdirs() }
        val descriptor = File(descriptorDir, "d2x.mn2").apply { writeText("name = Vertigo") }
        val hog = File(dataDir, "d2x.hog").apply { writeBytes(byteArrayOf(1, 2, 3)) }
        val target =
            LevelMetadataTarget(
                displayName = descriptor.name,
                game = GameFileFormats.GAME_D2,
                sourceType = "hog",
                sourcePath = hog.path,
                dataDir = dataDir.path,
                extraDataDir = descriptorDir.path,
                missionName = "d2x",
                missionFilename = descriptor.name,
                levelFile = "d2xlvl01.rl2",
            )

        val launch = LevelPreviewRequestStore.create(cacheDir, target, levelRow("d2xlvl01.rl2"))
        val request = JSONObject(launch.requestFile.readText())

        assertEquals(hog.canonicalPath, request.getString("hog_path"))
        assertEquals(descriptorDir.canonicalPath, request.getString("extra_data_dir"))
        assertEquals("d2x.mn2", request.getString("mission_filename"))
    }

    @Test
    fun cleanupRefusesARequestOutsidePreviewCache() {
        val root = testRoot("cleanup-scope")
        val cacheDir = File(root, "cache").apply { mkdirs() }
        val outside = File(root, "request.json").apply { writeText("keep") }

        assertFalse(LevelPreviewRequestStore.delete(cacheDir, outside.absolutePath))
        assertTrue(outside.isFile)
    }

    private fun levelRow(levelFile: String) =
        LevelMetadataLevelRow(
            levelNum = 1,
            secret = false,
            levelName = "Preview",
            levelFile = levelFile,
            robots = 0,
            hostages = 0,
            secrets = 0,
            matcens = 0,
            energyCenters = 0,
            mineVolume = 0.0,
            mineVolumeNormalized = 0.0,
            mineVolumeText = "",
            travelDistance = 0.0,
            travelTimeSeconds = 0,
            travelTimeText = "",
            guidebotCount = 0,
            guidebotPlaced = false,
            guidebotAccessible = false,
            guidebotPlacementNote = "",
            guidebotNote = "",
            routeStatus = "ok",
            routeProblem = "",
            routeNote = "",
            routeSteps = emptyList(),
            status = "ok",
            problems = emptyList(),
            notes = emptyList(),
        )

    private fun testRoot(name: String): File =
        File("build/test-level-preview-request/$name").absoluteFile.apply {
            deleteRecursively()
            mkdirs()
        }
}
