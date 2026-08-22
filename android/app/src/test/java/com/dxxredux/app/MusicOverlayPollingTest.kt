package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class MusicOverlayPollingTest {
    @Test
    fun currentTrackPollingContinuesWhileMusicPanelIsOpen() {
        assertFalse(shouldPollCurrentTrack(false, false, false))
        assertTrue(shouldPollCurrentTrack(true, false, false))
        assertFalse(shouldPollCurrentTrack(true, true, false))
        assertTrue(shouldPollCurrentTrack(false, false, true))
        assertTrue(shouldPollCurrentTrack(true, true, true))
    }
}
