package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class PreviewTransportTest {
    @Test
    fun singleTrackSeeksClampAndPreservePlaybackState() {
        for (status in listOf(PreviewStatus.PAUSED, PreviewStatus.PLAYING)) {
            var state = PreviewSnapshot(status, 5_000, 24_000)
            val transport = PreviewTransport(
                snapshot = { state },
                start = { error("Unexpected start") },
                pause = { error("Unexpected pause") },
                stop = { error("Unexpected stop") },
                seek = { state = state.copy(positionMs = it) },
            )
            transport.skip(-1)
            assertEquals(PreviewSnapshot(status, 0, 24_000), state)
            repeat(3) { transport.skip(1) }
            assertEquals(PreviewSnapshot(status, 24_000, 24_000), state)
        }
    }

    @Test
    fun playlistSkipsNeverFallBackToSeekingAtBoundaries() {
        val queue = listOf("first", "second", "third")
        var index = 0
        val transport = PreviewTransport(
            snapshot = { PreviewSnapshot(PreviewStatus.PLAYING, 5_000, 24_000) },
            start = {},
            pause = {},
            stop = {},
            seek = { error("Playlist commands must not seek") },
            navigate = { direction ->
                if (index + direction in queue.indices) index += direction
            },
        )
        transport.skip(-1)
        assertEquals(0, index)
        repeat(4) { transport.skip(1) }
        assertEquals(2, index)
        transport.skip(-1)
        assertEquals(1, index)
    }
}
