package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class LevelMetadataCheckpointProgressTest {
    @Test
    fun parsesActiveMissionLevelProgress() {
        val progress =
            LevelMetadataAnalyzer.parseLevelMetadataCheckpointProgress(
                """{"stage":"level","detail":"missions/kcxf2_n4.rl2","completed":3,"total":8}""",
            )

        assertEquals("Scanning kcxf2_n4.rl2", progress?.label)
        assertEquals(3, progress?.completed)
        assertEquals(8, progress?.total)
    }

    @Test
    fun parsesCompletedMissionLevelProgress() {
        val progress =
            LevelMetadataAnalyzer.parseLevelMetadataCheckpointProgress(
                """{"stage":"level_done","detail":"kcxf2_n6.rl2","completed":6,"total":8}""",
            )

        assertEquals("Scanning levels", progress?.label)
        assertEquals(6, progress?.completed)
        assertEquals(8, progress?.total)
    }

    @Test
    fun ignoresNonLevelCheckpoint() {
        assertNull(
            LevelMetadataAnalyzer.parseLevelMetadataCheckpointProgress(
                """{"stage":"mount","detail":"KCXF2RM.hog"}""",
            ),
        )
    }

    @Test
    fun parsesProgressWithinOneLargeLevel() {
        val update =
            LevelMetadataAnalyzer.parseLevelMetadataCheckpointUpdate(
                """{"stage":"level_progress","phase":"route_visibility","detail":"Uneasy4.rl2","task_id":7,"completed":4500,"total":9000,"level_completed":3,"level_total":8}""",
            )

        assertEquals("Checking switch firing paths in Uneasy4.rl2", update?.progress?.label)
        assertEquals(4500, update?.progress?.completed)
        assertEquals(9000, update?.progress?.total)
        assertEquals("Uneasy4.rl2:7", update?.activityId)
        assertEquals("Overall analysis", update?.analysisProgress?.overall?.label)
        assertEquals(3, update?.analysisProgress?.overall?.completed)
        assertEquals(8, update?.analysisProgress?.overall?.total)
        assertEquals(4500, update?.analysisProgress?.currentLevel?.completed)
        assertEquals(9000, update?.analysisProgress?.currentLevel?.total)
    }

    @Test
    fun overallProgressDoesNotResetWhenCurrentLevelTaskChanges() {
        val first =
            LevelMetadataAnalyzer.parseLevelMetadataCheckpointUpdate(
                """{"stage":"level_progress","phase":"level_topology","detail":"level04.rl2","task_id":2,"completed":95,"total":100,"level_completed":3,"level_total":8}""",
            )
        val second =
            LevelMetadataAnalyzer.parseLevelMetadataCheckpointUpdate(
                """{"stage":"level_progress","phase":"route_planning","detail":"level04.rl2","task_id":3,"completed":1,"total":100,"level_completed":3,"level_total":8}""",
            )

        assertEquals(first?.analysisProgress?.overall, second?.analysisProgress?.overall)
        assertEquals(95, first?.analysisProgress?.currentLevel?.completed)
        assertEquals(1, second?.analysisProgress?.currentLevel?.completed)
    }
}
