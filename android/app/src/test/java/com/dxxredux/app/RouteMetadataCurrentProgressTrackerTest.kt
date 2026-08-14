package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class RouteMetadataCurrentProgressTrackerTest {
    private fun progress(value: Int) =
        LevelMetadataAnalysisProgress(
            overall = MetadataLoadProgress("Overall analysis", 0, 1),
            estimatedLevel =
                MetadataLoadProgress(
                    "Estimated level progress",
                    value,
                    LevelMetadataLevelProgressEstimator.TOTAL,
                ),
        )

    @Test
    fun ignoresNextAndFillLevelProgress() {
        val tracker = RouteMetadataCurrentProgressTracker(5)

        assertNull(tracker.onAnalysisProgress(6, progress(800)))
        assertNull(tracker.onReadiness(6, "complete"))
        assertNull(tracker.onFailure(4))
    }

    @Test
    fun clampsCalculatingBelowCompleteAndStaysMonotonicAcrossRetries() {
        val tracker = RouteMetadataCurrentProgressTracker(5)

        assertEquals(700, tracker.onAnalysisProgress(5, progress(700))?.permille)
        assertEquals(700, tracker.onAnalysisProgress(5, progress(100))?.permille)
        assertEquals(999, tracker.onAnalysisProgress(5, progress(1_000))?.permille)
    }

    @Test
    fun reportsUsefulBeforeExactCompletion() {
        val tracker = RouteMetadataCurrentProgressTracker(5)
        tracker.onAnalysisProgress(5, progress(620))

        val useful = tracker.onReadiness(5, "next_ready")
        val complete = tracker.onReadiness(5, "complete")

        assertEquals(RouteMetadataProgressState.USEFUL, useful?.state)
        assertEquals(620, useful?.permille)
        assertEquals(RouteMetadataProgressState.COMPLETE, complete?.state)
        assertEquals(LevelMetadataLevelProgressEstimator.TOTAL, complete?.permille)
    }

    @Test
    fun terminalFailurePreservesLastEstimate() {
        val tracker = RouteMetadataCurrentProgressTracker(5)
        tracker.onAnalysisProgress(5, progress(420))

        val failed = tracker.onFailure(5)

        assertEquals(RouteMetadataProgressState.FAILED, failed?.state)
        assertEquals(420, failed?.permille)
    }
}
