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
}
