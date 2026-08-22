package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class RouteMetadataPrecomputeMonitorTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    @Test
    fun persistsOverallProgressAndOneMissionCompletionLine() {
        val filesDir = temporaryFolder.newFolder("files")
        val monitor = RouteMetadataPrecomputeMonitor(filesDir)
        val jobs = listOf(job(1), job(2))
        val entries =
            jobs.associate { job ->
                job.id to RouteMetadataLedgerEntry(RouteMetadataLedgerStatus.COMPLETE)
            }

        monitor.update(jobs, entries)
        monitor.levelFinished(jobs[0], RouteMetadataLedgerStatus.COMPLETE, 15L, jobs, entries)
        monitor.levelFinished(jobs[1], RouteMetadataLedgerStatus.COMPLETE, 20L, jobs, entries)

        val snapshot = RouteMetadataPrecomputeMonitor(filesDir).readSnapshot()
        val lines = monitor.readRecentLines()
        assertEquals(2, snapshot.totalLevels)
        assertEquals(2, snapshot.finishedLevels)
        assertEquals(ROUTE_METADATA_CACHE_GENERATION, snapshot.cacheGeneration)
        assertEquals(1, lines.count { " MISSION " in it })
        assertTrue(lines.any { "duration_ms=15" in it })
    }

    @Test
    fun publishesDiscoveryFailureInsteadOfLeavingAnEmptyUnknownState() {
        val filesDir = temporaryFolder.newFolder("discovery-failure")
        val monitor = RouteMetadataPrecomputeMonitor(filesDir)

        monitor.discoveryStarted()
        monitor.discoveryFailed(IllegalStateException("broken mission index"))

        val snapshot = monitor.readSnapshot()
        assertEquals("discovery_failed", snapshot.phase)
        assertEquals("broken mission index", snapshot.statusMessage)
        assertTrue(monitor.readRecentLines().any { "DISCOVERY status=failed" in it })
    }

    @Test
    fun persistsCurrentLevelProgressAndLaunchHandoff() {
        val filesDir = temporaryFolder.newFolder("live-progress")
        val monitor = RouteMetadataPrecomputeMonitor(filesDir)
        val job = job(4)
        monitor.update(listOf(job), emptyMap(), job, RouteMetadataPriority.FILL)

        monitor.analysisProgress(
            job,
            RouteMetadataPriority.FILL,
            LevelMetadataAnalysisProgress(
                overall = MetadataLoadProgress("Overall analysis", 0, 1),
                currentLevel = MetadataLoadProgress("Planning completion route", 2, 5),
                estimatedLevel = MetadataLoadProgress("Estimated level progress", 420, 1_000),
            ),
        )

        val active = monitor.readSnapshot()
        assertEquals("Planning completion route", active.currentDetail)
        assertEquals(420, active.currentProgressCompleted)
        assertEquals(1_000, active.currentProgressTotal)
        assertEquals(0, active.currentMissionFinishedLevels)
        assertEquals(1, active.currentMissionTotalLevels)

        monitor.launchHandoff(
            "timeout",
            8_000L,
            1,
            workerWasActive = true,
            graceMs = 100L,
            cleanupPending = true,
        )
        val paused = monitor.readSnapshot()
        assertEquals("paused_for_game", paused.phase)
        assertTrue(paused.statusMessage.contains("timed out"))
        assertTrue(monitor.readRecentLines().any { "GAME_HANDOFF status=timeout" in it })
        assertTrue(monitor.readRecentLines().any { "worker_was_active=true grace_ms=100" in it })
        assertTrue(monitor.readRecentLines().any { "cleanup_pending=true" in it })
    }

    @Test
    fun persistsLifecycleRetryHeartbeatAndInGameIdleEvents() {
        val filesDir = temporaryFolder.newFolder("scheduler-events")
        val monitor = RouteMetadataPrecomputeMonitor(filesDir)
        val job = job(2)

        monitor.coordinatorEvent("stopped", "launcher paused")
        monitor.retryDeferred(job)
        monitor.heartbeat(job, RouteMetadataPriority.FILL, "Planning route")
        monitor.inGameLevelFinished(
            "Obsidian",
            2,
            "level2.rl2",
            RouteMetadataLedgerStatus.COMPLETE,
            RouteMetadataPriority.NEXT,
            50L,
        )
        monitor.inGameMissionFinished("Obsidian", 4)

        val lines = monitor.readRecentLines()
        assertTrue(lines.any { "STATE status=stopped reason=launcher paused" in it })
        assertTrue(lines.any { "RETRY mission=Obsidian" in it })
        assertTrue(lines.any { "HEARTBEAT mission=Obsidian" in it })
        assertTrue(lines.any { "context=in_game priority=next cpu_duty_percent=10" in it })
        assertTrue(lines.any { "cpu_duty_percent=0" in it })
    }

    @Test
    fun heartbeatDetailIncludesMeasuredProgress() {
        val detail =
            routeMetadataProgressDetail(
                LevelMetadataAnalysisProgress(
                    overall = MetadataLoadProgress("Overall analysis", 0, 1),
                    currentLevel = MetadataLoadProgress("Checking switch firing paths", 2, 5),
                    estimatedLevel = MetadataLoadProgress("Estimated level progress", 431, 1_000),
                ),
            )

        assertEquals("Checking switch firing paths 431/1000", detail)
    }

    @Test
    fun persistsIndependentMusicProgressInTheSharedLog() {
        val filesDir = temporaryFolder.newFolder("music-progress")
        val monitor = RouteMetadataPrecomputeMonitor(filesDir)
        monitor.musicDiscovery(8)
        monitor.updateMusic(
            MissionMusicPrecomputeProgress(
                totalTracks = 8,
                finishedTracks = 2,
                failedTracks = 0,
                waitingTracks = 0,
                currentMission = "KCXF2",
                currentTrack = "game03.ogg",
                phase = "hashing",
            ),
        )
        monitor.musicTrackStarted("KCXF2", "game03.ogg")
        monitor.musicTrackFinished("KCXF2", "game03.ogg", matched = true, elapsedMs = 25L)
        monitor.musicSidecarFinished("KCXF2", 8)
        monitor.musicMissionFinished("KCXF2", 8, 0)

        val snapshot = monitor.readSnapshot()
        val lines = monitor.readRecentLines()
        assertEquals(8, snapshot.musicTotalTracks)
        assertEquals(2, snapshot.musicFinishedTracks)
        assertEquals("KCXF2", snapshot.musicCurrentMission)
        assertTrue(lines.any { "MUSIC TRACK" in it && "matched=true" in it })
        assertTrue(lines.any { "MUSIC SIDECAR" in it && "records=8" in it && "status=complete" in it })
        assertTrue(lines.any { "MUSIC MISSION" in it })
    }

    @Test
    fun recentActivityCanBeLimitedToTheLatestEightItems() {
        val monitor = RouteMetadataPrecomputeMonitor(temporaryFolder.newFolder("recent-eight"))
        repeat(10) { monitor.coordinatorEvent("item", "number=$it") }

        val lines = monitor.readRecentLines(limit = 8)

        assertEquals(8, lines.size)
        assertTrue(lines.first().endsWith("reason=number=2"))
        assertTrue(lines.last().endsWith("reason=number=9"))
    }

    private fun job(level: Int) =
        RouteMetadataPrecomputeJob(
            target =
                LevelMetadataTarget(
                    displayName = "Obsidian",
                    game = "d2",
                    sourceType = "mission_zip",
                    missionName = "obsidian",
                    levelFile = "level$level.rl2",
                    levelNum = level,
                ),
            sourceIdentity = "obsidian-source",
            enabled = true,
        )
}
