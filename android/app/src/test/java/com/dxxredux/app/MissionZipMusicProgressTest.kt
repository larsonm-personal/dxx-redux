package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class MissionZipMusicProgressTest {
    @Test
    fun formatsActiveChromaprintProgress() {
        val progress =
            MissionZipMusicAnalysisProgress(
                label = "Generating chromaprints",
                completed = 2,
                total = 5,
            )

        assertEquals(0.4f, progress.fraction)
        assertEquals("Generating chromaprints 2/5", formatMissionZipMusicProgress(progress))
    }

    @Test
    fun formatsCompletedMatchCount() {
        val progress =
            MissionZipMusicAnalysisProgress(
                label = "Bundled database matches",
                completed = 5,
                total = 5,
                resultCount = 3,
            )

        assertEquals(1f, progress.fraction)
        assertEquals("Bundled database matches 5/5, 3 matched", formatMissionZipMusicProgress(progress))
    }
}
