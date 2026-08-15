package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder

class RouteMetadataSchedulingTest {
    @get:Rule
    val temporaryFolder = TemporaryFolder()

    @Test
    fun completeCurrentLevelSkipsDirectlyToNextWork() {
        val complete = RouteMetadataCurrentWork.forReadiness("complete")
        val partial = RouteMetadataCurrentWork.forReadiness("next_ready")
        val missing = RouteMetadataCurrentWork.forReadiness("calculating")

        assertTrue(complete.skip)
        assertTrue(complete.ready)
        assertEquals(RouteMetadataPriority.NEXT, complete.priority)
        assertFalse(partial.skip)
        assertTrue(partial.ready)
        assertEquals(RouteMetadataPriority.NEXT, partial.priority)
        assertFalse(missing.skip)
        assertFalse(missing.ready)
        assertEquals(RouteMetadataPriority.ACTIVE, missing.priority)
    }

    @Test
    fun higherPriorityAndReplacementFocusPreemptButLowerPriorityDoesNot() {
        assertTrue(
            RouteMetadataPreemption.shouldPreempt(
                RouteMetadataPriority.FILL,
                RouteMetadataPriority.NEXT,
            ),
        )
        assertTrue(
            RouteMetadataPreemption.shouldPreempt(
                RouteMetadataPriority.NEXT,
                RouteMetadataPriority.NEXT,
                replacesFocus = true,
            ),
        )
        assertFalse(
            RouteMetadataPreemption.shouldPreempt(
                RouteMetadataPriority.ACTIVE,
                RouteMetadataPriority.FILL,
                replacesFocus = true,
            ),
        )
        assertFalse(RouteMetadataPreemption.shouldPreempt(null, RouteMetadataPriority.ACTIVE))
    }

    @Test
    fun startingAtLevelFiveRunsForwardBeforeWrapping() {
        val levels =
            RouteMetadataMissionOrdering.order(
                currentLevelNum = 5,
                currentLevelFile = "level05.rl2",
                normalLevelFiles = (1..7).map { "level%02d.rl2".format(it) },
                secretLevelFiles = emptyList(),
                secretEntryLevels = emptyList(),
            )

        assertEquals(listOf(5, 6, 7, 1, 2, 3, 4), levels.map { it.levelNum })
        assertEquals(RouteMetadataPriority.ACTIVE, levels[0].priority)
        assertEquals(RouteMetadataPriority.NEXT, levels[1].priority)
        assertTrue(levels.drop(2).all { it.priority == RouteMetadataPriority.FILL })
    }

    @Test
    fun ordinaryNextLevelPrecedesPossibleSecretSuccessor() {
        val levels =
            RouteMetadataMissionOrdering.order(
                currentLevelNum = 5,
                currentLevelFile = "level05.rl2",
                normalLevelFiles = (1..7).map { "level%02d.rl2".format(it) },
                secretLevelFiles = listOf("secret01.rl2"),
                secretEntryLevels = listOf(5),
            )

        assertEquals(listOf(5, 6, -1, 7, 1, 2, 3, 4), levels.map { it.levelNum })
        assertEquals(RouteMetadataPriority.NEXT, levels[1].priority)
        assertEquals(RouteMetadataPriority.NEXT, levels[2].priority)
    }

    @Test
    fun secretLevelContinuesAfterItsEntryLevelBeforeWrapping() {
        val levels =
            RouteMetadataMissionOrdering.order(
                currentLevelNum = -2,
                currentLevelFile = "secret02.rl2",
                normalLevelFiles = (1..7).map { "level%02d.rl2".format(it) },
                secretLevelFiles = listOf("secret01.rl2", "secret02.rl2"),
                secretEntryLevels = listOf(3, 5),
            )

        assertEquals(listOf(-2, 6, 7, 1, 2, 3, 4, 5, -1), levels.map { it.levelNum })
    }

    @Test
    fun deterministicFailureStopsAfterOneAttempt() {
        val assessment =
            RouteMetadataAttemptClassifier.classify(
                result(failureKind = "unroutable"),
                cachePublished = false,
                previous = null,
            )

        assertEquals(RouteMetadataLedgerStatus.FAILED, assessment.status)
        assertFalse(assessment.shouldRetry)
    }

    @Test
    fun identicalInternalFailureStopsAfterSecondAttempt() {
        val result = result(failureKind = "internal_error")
        val first = RouteMetadataAttemptClassifier.classify(result, false, null)
        val prior =
            RouteMetadataLedgerEntry(
                status = first.status,
                progressToken = first.progressToken,
                failureKind = first.failureKind,
                failureFingerprint = first.failureFingerprint,
                failureCount = 1,
            )
        val second = RouteMetadataAttemptClassifier.classify(result, false, prior)

        assertTrue(first.shouldRetry)
        assertEquals(RouteMetadataLedgerStatus.FAILED, second.status)
        assertFalse(second.shouldRetry)
    }

    @Test
    fun durableLedgerPreservesFailureAndProgressState() {
        val root = temporaryFolder.newFolder()
        val ledger = RouteMetadataLedger(root)
        ledger.record(
            "job",
            RouteMetadataAttemptAssessment(
                status = RouteMetadataLedgerStatus.PARTIAL,
                cacheFile = "route-cache/g6/example.bin",
                progressToken = "partial|12",
                failureKind = "",
                failureFingerprint = "",
                shouldRetry = true,
            ),
            nowMs = 42L,
        )

        val restored = RouteMetadataLedger(root).read("job")

        assertEquals(RouteMetadataLedgerStatus.PARTIAL, restored?.status)
        assertEquals("partial|12", restored?.progressToken)
        assertEquals(42L, restored?.updatedAtMs)
    }

    private fun result(failureKind: String): LevelMetadataResult =
        LevelMetadataResult.fromJson(
            """
            {
              "status": "failed",
              "failure_kind": "$failureKind",
              "source": "test",
              "game": "d2",
              "levels": [],
              "problems": ["failed"]
            }
            """.trimIndent(),
        )
}
