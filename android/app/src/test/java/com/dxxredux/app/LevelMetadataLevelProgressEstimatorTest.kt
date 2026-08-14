package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class LevelMetadataLevelProgressEstimatorTest {
    private fun update(
        phase: String,
        taskId: Int,
        completed: Int,
        total: Int = 100,
        level: String = "level01.rl2",
        stage: String = "level_progress",
    ): LevelMetadataCheckpointUpdate =
        checkNotNull(
            LevelMetadataAnalyzer.parseLevelMetadataCheckpointUpdate(
                """{"stage":"$stage","phase":"$phase","detail":"$level","task_id":$taskId,"completed":$completed,"total":$total,"level_completed":0,"level_total":1}""",
            ),
        )

    @Test
    fun assignsFixedBandsToKnownOuterPhases() {
        val estimator = LevelMetadataLevelProgressEstimator()

        assertEquals(50, estimator.observe(update("secret_areas", 1, 50)).completed)
        assertEquals(150, estimator.observe(update("level_topology", 2, 50)).completed)
        assertEquals(250, estimator.observe(update("level_summary", 3, 50)).completed)
        assertEquals(300, estimator.observe(update("route_planning", 4, 0, 1)).completed)
    }

    @Test
    fun repeatedInnerTasksAdvanceWithoutResetting() {
        val estimator = LevelMetadataLevelProgressEstimator()
        estimator.observe(update("route_planning", 4, 0, 1))
        val firstHalf = estimator.observe(update("route_visibility", 5, 50)).completed
        val firstDone = estimator.observe(update("route_visibility", 5, 100)).completed
        val secondStart = estimator.observe(update("route_target_visibility", 6, 0)).completed
        val secondHalf = estimator.observe(update("route_target_visibility", 6, 50)).completed

        assertTrue(firstHalf > 300)
        assertTrue(firstDone > firstHalf)
        assertEquals(firstDone, secondStart)
        assertTrue(secondHalf > secondStart)
        assertTrue(secondHalf < LevelMetadataLevelProgressEstimator.TOTAL)
    }

    @Test
    fun skippedTaskIdsReceiveCreditAndUnknownPhasesStayMonotonic() {
        val estimator = LevelMetadataLevelProgressEstimator()
        val first = estimator.observe(update("route_visibility", 5, 20)).completed
        val skipped = estimator.observe(update("future_route_phase", 9, 10)).completed
        val backwardRaw = estimator.observe(update("future_route_phase", 9, 0)).completed

        assertTrue(skipped > first)
        assertEquals(skipped, backwardRaw)
    }

    @Test
    fun unfinishedProgressNeverReachesCompleteAndLevelDoneDoes() {
        val estimator = LevelMetadataLevelProgressEstimator()
        val unfinished = estimator.observe(update("route_visibility", 100, 100)).completed
        val done = estimator.observe(update("", 0, 1, 1, stage = "level_done")).completed

        assertEquals(LevelMetadataLevelProgressEstimator.TOTAL - 1, unfinished)
        assertEquals(LevelMetadataLevelProgressEstimator.TOTAL, done)
    }

    @Test
    fun cancellationLikeLevelCheckpointPreservesProgressAndNewLevelResetsIt() {
        val estimator = LevelMetadataLevelProgressEstimator()
        val active = estimator.observe(update("route_visibility", 7, 50)).completed
        val checkpoint = estimator.observe(update("", 0, 0, 1, stage = "level")).completed
        val reset = estimator.observe(update("secret_areas", 1, 0, level = "level02.rl2")).completed

        assertEquals(active, checkpoint)
        assertEquals(0, reset)
    }
}
