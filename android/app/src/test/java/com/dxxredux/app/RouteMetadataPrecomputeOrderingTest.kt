package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class RouteMetadataPrecomputeOrderingTest {
    private fun job(
        game: String,
        mission: String,
        level: Int,
        enabled: Boolean = true,
    ) =
        RouteMetadataPrecomputeJob(
            LevelMetadataTarget(
                displayName = mission.ifBlank { game },
                game = game,
                sourceType = "hog",
                missionName = mission,
                levelFile = "level$level.rl2",
                levelNum = level,
            ),
            sourceIdentity = "$game-$mission",
            enabled = enabled,
        )

    @Test
    fun exactRecentSaveLevelWinsBeforeItsGame() {
        val jobs = listOf(job("d1", "", 1), job("d2", "custom", 2), job("d2", "custom", 7))
        val recent =
            ResumeSaveBridge.ResumeSaveCandidate(
                path = "save.sg0",
                relativePath = "save.sg0",
                game = "d2",
                saveKind = "manual",
                saveTimeUnixSeconds = 1,
                callsign = "pilot",
                description = "save",
                missionName = "custom",
                levelNum = 7,
                levelName = "Level 7",
                levelSeconds = 0,
                totalSeconds = 0,
                difficultyChanged = false,
                difficultyMin = 2,
                difficultyMax = 2,
                musicType = 0,
                slot = 0,
                hasThumbnail = false,
                thumbnailWidth = 0,
                thumbnailHeight = 0,
                metadataBacked = true,
                thumbnailRgb6 = null,
            )

        val ordered = RouteMetadataPrecomputeOrdering.order(jobs, recent)

        assertEquals(7, ordered[0].target.levelNum)
        assertEquals("d2", ordered[1].target.game)
    }

    @Test
    fun enabledModsWinWithinSameGamePriority() {
        val disabled = job("d2", "disabled", 1, enabled = false)
        val enabled = job("d2", "enabled", 2, enabled = true)

        assertEquals(enabled, RouteMetadataPrecomputeOrdering.order(listOf(disabled, enabled), null).first())
    }

    @Test
    fun d2BaseHogWinsWithoutBlankMissionName() {
        val base =
            job("d2", "d2", 1).copy(
                target = job("d2", "d2", 1).target.copy(sourcePath = "/game/descent2.hog"),
            )
        val expansion =
            job("d2", "d2-2plyr", 1).copy(
                target = job("d2", "d2-2plyr", 1).target.copy(sourcePath = "/game/d2-2plyr.hog"),
            )

        assertEquals(base, RouteMetadataPrecomputeOrdering.order(listOf(expansion, base), null).first())
    }

    @Test
    fun newlyImportedSourceWinsNormalOrdering() {
        val recentJob = job("d2", "recent", 7)
        val imported = job("d1", "new", 1)

        val ordered =
            RouteMetadataPrecomputeOrdering.order(
                listOf(recentJob, imported),
                recent = null,
                focusedSourceIdentity = imported.sourceIdentity,
            )

        assertEquals(imported, ordered.first())
    }
}
