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
        assertEquals(1, lines.count { " MISSION " in it })
        assertTrue(lines.any { "duration_ms=15" in it })
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
