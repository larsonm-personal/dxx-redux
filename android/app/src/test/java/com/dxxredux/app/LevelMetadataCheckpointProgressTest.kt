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
                """{"stage":"level_progress","phase":"route_visibility","detail":"Uneasy4.rl2","task_id":7,"completed":4500,"total":9000}""",
            )

        assertEquals("Checking switch firing paths in Uneasy4.rl2", update?.progress?.label)
        assertEquals(4500, update?.progress?.completed)
        assertEquals(9000, update?.progress?.total)
        assertEquals("Uneasy4.rl2:7", update?.activityId)
    }
}
