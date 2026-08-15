package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class RobotPreviewRequestStoreTest {
    @Test
    fun requestRetainsRobotAndSourceLevelContext() {
        val root = testRoot("create")
        val cacheDir = File(root, "cache").apply { mkdirs() }
        val dataDir = File(root, "data").apply { mkdirs() }
        val level = File(dataDir, "preview.rl2").apply { writeBytes(byteArrayOf(1, 2, 3)) }
        val item = robotItem(60)
        val row = levelRow(level.name, item)
        val target =
            LevelMetadataTarget(
                displayName = level.name,
                game = GameFileFormats.GAME_D2,
                sourceType = "level",
                sourcePath = level.path,
                dataDir = dataDir.path,
                levelFile = level.name,
            )

        val launch = RobotPreviewRequestStore.create(cacheDir, target, row, item, "Lou Guard (Robot 60)")
        val request = JSONObject(launch.requestFile.readText())
        val runtime = RobotPreviewRequestStore.validateForLaunch(cacheDir, launch.requestFile.absolutePath, "d2")

        assertEquals("dxx-robot-preview-request-v1", request.getString("schema"))
        assertEquals(level.name, request.getString("level_file"))
        assertEquals(60, request.getInt("robot_number"))
        assertEquals("Lou Guard (Robot 60)", runtime.robotLabel)
        assertEquals(dataDir.canonicalFile, runtime.dataDir)
        assertTrue(RobotPreviewRequestStore.delete(cacheDir, launch.requestFile.absolutePath))
        assertFalse(launch.requestFile.parentFile!!.exists())
    }

    @Test
    fun sourceLevelIsTheLevelThatContainsTheReplacement() {
        val wanted = robotItem(60)
        val unrelated = levelRow("level1.rl2", robotItem(4))
        val source = levelRow("level4.rl2", wanted)

        assertEquals(source, RobotPreviewRequestStore.findSourceLevel(listOf(unrelated, source), wanted))
    }

    private fun robotItem(number: Int) =
        LevelMetadataReplacementItem(
            kind = "robot",
            number = number,
            label = "Robot $number",
            summary = "1 change",
            fields =
                listOf(
                    LevelMetadataReplacement(
                        kind = "strength",
                        label = "Strength",
                        baseGame = 100,
                        mod = 200,
                    ),
                ),
        )

    private fun levelRow(
        levelFile: String,
        item: LevelMetadataReplacementItem,
    ) =
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
            replacementGroups =
                listOf(
                    LevelMetadataReplacementGroup(
                        kind = "robot_changes",
                        label = "Robot changes",
                        summary = "1 change",
                        items = listOf(item),
                    ),
                ),
        )

    private fun testRoot(name: String): File =
        File("build/test-robot-preview-request/$name").absoluteFile.apply {
            deleteRecursively()
            mkdirs()
        }
}
