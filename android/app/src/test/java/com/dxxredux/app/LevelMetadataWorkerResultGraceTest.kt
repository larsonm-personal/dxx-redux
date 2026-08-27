package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LevelMetadataWorkerResultGraceTest {
    @Test
    fun waitsForResultPublishedAsWorkerExits() {
        val grace = LevelMetadataWorkerResultGrace(1_000)

        assertFalse(grace.shouldStop(false, false, true, 10_000))
        assertFalse(grace.shouldStop(false, true, true, 10_100))
        assertFalse(grace.shouldStop(false, false, true, 10_200))
    }

    @Test
    fun stopsAfterWorkerRemainsMissingWithoutResult() {
        val grace = LevelMetadataWorkerResultGrace(1_000)

        assertFalse(grace.shouldStop(false, false, true, 10_000))
        assertFalse(grace.shouldStop(false, false, true, 10_999))
        assertTrue(grace.shouldStop(false, false, true, 11_000))
    }

    @Test
    fun ignoresMissingWorkerBeforeStartTimeout() {
        val grace = LevelMetadataWorkerResultGrace(1_000)

        assertFalse(grace.shouldStop(false, false, false, 10_000))
        assertFalse(grace.shouldStop(false, false, true, 10_100))
    }
}
