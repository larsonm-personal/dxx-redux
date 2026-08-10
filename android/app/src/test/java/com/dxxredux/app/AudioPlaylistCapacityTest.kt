package com.dxxredux.app

import java.io.IOException
import org.junit.Assert.assertThrows
import org.junit.Test

class AudioPlaylistCapacityTest {
    private fun source(
        id: Int,
        tracks: Int,
        audioTracks: Int = tracks,
    ) =
        AudioSourceManager.AudioSource(
            id = "source-$id",
            cuePath = "source-$id.cue",
            binPaths = listOf("source-$id.bin"),
            discLabel = "Source $id",
            discId = "disc-$id",
            trackCount = tracks,
            audioTrackCount = audioTracks,
            legacyDiscId = id.toLong(),
        )

    @Test
    fun capacity_acceptsExactLimitsAndOneTrackPlaylist() {
        requireAudioPlaylistCapacity(listOf(source(0, 1)))
        requireAudioPlaylistCapacity(List(AUDIO_PLAYLIST_MAX_SOURCES - 1) { source(it, 12) } + source(7, 16))
    }

    @Test
    fun capacity_rejectsExcessSourcesTracksAndUnplayableSources() {
        assertThrows(IOException::class.java) {
            requireAudioPlaylistCapacity(List(AUDIO_PLAYLIST_MAX_SOURCES + 1) { source(it, 1) })
        }
        assertThrows(IOException::class.java) {
            requireAudioPlaylistCapacity(listOf(source(0, AUDIO_PLAYLIST_MAX_TRACKS), source(1, 1)))
        }
        assertThrows(IOException::class.java) { requireAudioPlaylistCapacity(listOf(source(0, 1, 0))) }
        assertThrows(IOException::class.java) { requireAudioPlaylistCapacity(listOf(source(0, 1, 2))) }
    }
}
