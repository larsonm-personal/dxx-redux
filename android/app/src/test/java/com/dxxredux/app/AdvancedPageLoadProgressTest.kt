package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class AdvancedPageLoadProgressTest {
    @Test
    fun formatAdvancedPageLoadProgressClampsCompletedCount() {
        val progress =
            AdvancedPageLoadProgress(
                label = "Scanning recorded demos",
                completed = 5,
                total = 3,
            )

        assertEquals("Scanning recorded demos 3/3", formatAdvancedPageLoadProgress(progress))
    }

    @Test
    fun formatAdvancedPageLoadProgressHandlesZeroTotal() {
        val progress =
            AdvancedPageLoadProgress(
                label = "Preparing Advanced Settings",
                completed = 0,
                total = 0,
            )

        assertEquals("Preparing Advanced Settings 0/1", formatAdvancedPageLoadProgress(progress))
    }

    @Test
    fun formatChromaprintProgressReportsMetadataWaitAndFailures() {
        assertEquals(
            "2 / 8 tracks, waiting for level metadata",
            formatChromaprintPrecomputeProgress(
                RouteMetadataPrecomputeSnapshot(
                    musicTotalTracks = 8,
                    musicFinishedTracks = 2,
                    musicWaitingTracks = 6,
                    musicPhase = "waiting_for_metadata",
                ),
            ),
        )
        assertEquals(
            "8 / 8 tracks (1 failed)",
            formatChromaprintPrecomputeProgress(
                RouteMetadataPrecomputeSnapshot(
                    musicTotalTracks = 8,
                    musicFinishedTracks = 8,
                    musicFailedTracks = 1,
                    musicPhase = "complete",
                ),
            ),
        )
    }

    @Test
    fun formatChromaprintProgressUsesUnifiedAudioWording() {
        assertEquals(
            "No audio discovered",
            formatChromaprintPrecomputeProgress(RouteMetadataPrecomputeSnapshot(musicPhase = "empty")),
        )
        assertEquals(
            "Discovering audio",
            formatChromaprintPrecomputeProgress(RouteMetadataPrecomputeSnapshot(musicPhase = "discovering")),
        )
    }
}
