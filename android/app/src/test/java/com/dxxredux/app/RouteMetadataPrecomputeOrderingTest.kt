package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

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

    @Test
    fun partialLevelYieldsToUnattemptedPeerAtTheSamePriority() {
        val partial = job("d2", "d2", 2)
        val unattempted = job("d2", "d2", 3)

        val ordered =
            RouteMetadataPrecomputeOrdering.order(listOf(partial, unattempted), null) {
                if (it == partial) 100L else 0L
            }

        assertEquals(unattempted, ordered.first())
    }

    @Test
    fun partialBaseLevelYieldsToUnattemptedMission() {
        val partialBase =
            job("d2", "d2", 2).copy(
                target = job("d2", "d2", 2).target.copy(sourcePath = "/game/descent2.hog"),
            )
        val unattemptedMission = job("d2", "custom", 1)

        val ordered =
            RouteMetadataPrecomputeOrdering.order(listOf(partialBase, unattemptedMission), null) {
                if (it == partialBase) 100L else 0L
            }

        assertEquals(unattemptedMission, ordered.first())
    }

    @Test
    fun metadataViewerFocusBrokerDispatchesOnlyWhileAttached() {
        var focused: LevelMetadataTarget? = null
        val handler: (LevelMetadataTarget?) -> Unit = { focused = it }
        val target = job("d2", "focused", 1).target

        RouteMetadataPrecomputeFocusBroker.attach(handler)
        RouteMetadataPrecomputeFocusBroker.focus(target)
        assertEquals(target, focused)

        RouteMetadataPrecomputeFocusBroker.detach(handler)
        RouteMetadataPrecomputeFocusBroker.focus(null)
        assertEquals(target, focused)
    }

    @Test
    fun missionMusicWaitsForEveryRouteInTheSameMission() {
        val routes = listOf(job("d2", "kcxf2", 1), job("d2", "kcxf2", 2))
        val music = musicJob(routes.first().sourceIdentity)

        assertFalse(
            MissionMusicPrecomputeScheduling.isEligible(music, routes) { it.target.levelNum == 1 },
        )
        assertTrue(MissionMusicPrecomputeScheduling.isEligible(music, routes) { true })
    }

    @Test
    fun missionMusicYieldsToNextPriorityRoutesButPrecedesFillWork() {
        assertFalse(MissionMusicPrecomputeScheduling.shouldRunBeforeRoute(RouteMetadataPriority.NEXT))
        assertTrue(MissionMusicPrecomputeScheduling.shouldRunBeforeRoute(RouteMetadataPriority.FILL))
        assertTrue(MissionMusicPrecomputeScheduling.shouldRunBeforeRoute(null))
    }

    private fun musicJob(routeSourceIdentity: String): MissionMusicPrecomputeJob =
        MissionMusicPrecomputeJob(
            displayName = "KCXF2",
            routeSourceIdentity = routeSourceIdentity,
            catalog = MissionZipMusicCatalog("kcxf2.7z", emptyList(), "music-source"),
            outputFile = File("mission_music_names.json"),
            enabled = true,
        )
}
