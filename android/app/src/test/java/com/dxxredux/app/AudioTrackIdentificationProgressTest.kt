package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.fail
import org.junit.Test

class AudioTrackIdentificationProgressTest {
    @Test
    fun reportsEachTrackAgainstKnownTotal() {
        assertEquals(
            AudioTrackIdentificationProgress(1, 12),
            audioTrackIdentificationProgress(1, 12),
        )
        assertEquals(
            AudioTrackIdentificationProgress(7, 12),
            audioTrackIdentificationProgress(7, 12),
        )
        assertEquals(
            "Identifying audio track 7 of 12...",
            audioTrackIdentificationProgress(7, 12).status,
        )
    }

    @Test
    fun clampsCallbackIndexAndRejectsMissingTotal() {
        assertEquals(1, audioTrackIdentificationProgress(0, 12).current)
        assertEquals(12, audioTrackIdentificationProgress(13, 12).current)
        try {
            audioTrackIdentificationProgress(1, 0)
            fail("zero total should fail")
        } catch (_: IllegalArgumentException) {
        }
    }
}
