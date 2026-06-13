package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test
import java.io.File

class MusicOverlaySourcesTest {
    @Test
    fun emptyLauncherMusicShowsOnlyBaseMidi() {
        val filesDir = freshDir("build/test-music-overlay-empty")

        assertEquals(
            listOf(MusicOverlaySourceOption("midi", "Base game MIDI")),
            musicOverlaySourceOptions(filesDir, "d2"),
        )
    }

    @Test
    fun launcherFilesAndCdAppearOnlyWhenEnabled() {
        val filesDir = freshDir("build/test-music-overlay-sources")
        CustomAudioSetManager(filesDir).addSet(
            CustomAudioSetManager.AudioSet(
                id = "custom",
                label = "Custom",
                files = listOf("track.ogg"),
                enabled = true,
            ),
        )
        File(filesDir, "audio_sources.json").writeText(
            """
            {
              "sources": [
                {
                  "id": "cd",
                  "cue": "disc.cue",
                  "bins": ["disc.bin"],
                  "label": "Disc",
                  "disc_id": "unknown",
                  "track_count": 2,
                  "audio_track_count": 1,
                  "legacy_disc_id": 0,
                  "enabled": true
                }
              ]
            }
            """.trimIndent(),
        )

        assertEquals(
            listOf(
                MusicOverlaySourceOption("files", "Files"),
                MusicOverlaySourceOption("cd", "CD"),
                MusicOverlaySourceOption("midi", "Base game MIDI"),
            ),
            musicOverlaySourceOptions(filesDir, "d2"),
        )
    }

    private fun freshDir(path: String): File =
        File(path).absoluteFile.also {
            it.deleteRecursively()
            it.mkdirs()
        }
}
