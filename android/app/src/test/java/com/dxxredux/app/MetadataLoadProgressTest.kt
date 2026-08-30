package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class MetadataLoadProgressTest {
    @Test
    fun formatsProgressAndFraction() {
        val progress =
            MetadataLoadProgress(
                label = "Reading archive entry metadata",
                completed = 1,
                total = 4,
            )

        assertEquals(0.25f, progress.fraction)
        assertEquals("Reading archive entry metadata 1/4", formatMetadataLoadProgress(progress))
    }

    @Test
    fun formatProgressClampsCounts() {
        val progress =
            MetadataLoadProgress(
                label = "Preparing metadata view",
                completed = 5,
                total = 3,
            )

        assertEquals(1f, progress.fraction)
        assertEquals("Preparing metadata view 3/3", formatMetadataLoadProgress(progress))
    }

    @Test
    fun formatProgressHandlesZeroTotal() {
        val progress =
            MetadataLoadProgress(
                label = "Locating file metadata",
                completed = 0,
                total = 0,
            )

        assertNull(progress.fraction)
        assertEquals("Locating file metadata 0/1", formatMetadataLoadProgress(progress))
    }

    @Test
    fun formatsEstimatedProgressAsWholePercent() {
        assertEquals(
            "Estimated level progress 11%",
            formatMetadataLoadProgressPercent(
                MetadataLoadProgress("Estimated level progress", 119, 1_000),
            ),
        )
    }

    @Test
    fun estimatedPercentDoesNotReachOneHundredEarly() {
        assertEquals(
            "Estimated level progress 99%",
            formatMetadataLoadProgressPercent(
                MetadataLoadProgress("Estimated level progress", 999, 1_000),
            ),
        )
    }
}
