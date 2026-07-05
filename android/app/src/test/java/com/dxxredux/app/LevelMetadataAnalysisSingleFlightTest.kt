package com.dxxredux.app

import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class LevelMetadataAnalysisSingleFlightTest {
    @Before
    fun setUp() {
        LevelMetadataAnalysisSingleFlight.resetForTest()
    }

    @After
    fun tearDown() {
        LevelMetadataAnalysisSingleFlight.resetForTest()
    }

    @Test
    fun tryEnterRejectsConcurrentAnalysis() {
        assertTrue(LevelMetadataAnalysisSingleFlight.tryEnter())
        assertFalse(LevelMetadataAnalysisSingleFlight.tryEnter())
    }

    @Test
    fun exitAllowsNextAnalysis() {
        assertTrue(LevelMetadataAnalysisSingleFlight.tryEnter())
        LevelMetadataAnalysisSingleFlight.exit()
        assertTrue(LevelMetadataAnalysisSingleFlight.tryEnter())
    }
}
