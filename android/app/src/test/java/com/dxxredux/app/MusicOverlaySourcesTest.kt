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
        File(filesDir, "${CustomAudioSetManager.MUSIC_DIR}/custom/track.ogg").apply {
            parentFile?.mkdirs()
            writeText("audio")
        }
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
        File(filesDir, "disc.cue").writeText("FILE \"disc.bin\" BINARY\n")
        File(filesDir, "disc.bin").writeText("audio")

        assertEquals(
            listOf(
                MusicOverlaySourceOption("files", "Files"),
                MusicOverlaySourceOption("cd", "CD"),
                MusicOverlaySourceOption("midi", "Base game MIDI"),
            ),
            musicOverlaySourceOptions(filesDir, "d2"),
        )
    }

    @Test
    fun cdOptionRequiresCompleteAccessiblePlaylist() {
        val filesDir = freshDir("build/test-music-overlay-invalid-cd")
        File(filesDir, "audio_sources.json").writeText(
            """
            {"sources":[{"id":"cd","cue":"missing.cue","bins":["missing.bin"],"label":"Disc",
            "disc_id":"unknown","track_count":101,"audio_track_count":100,"legacy_disc_id":0,"enabled":true}]}
            """.trimIndent(),
        )

        assertEquals(
            listOf(MusicOverlaySourceOption("midi", "Base game MIDI")),
            musicOverlaySourceOptions(filesDir, "d2"),
        )
    }

    @Test
    fun missingCustomTrackIsNotAdvertised() {
        val filesDir = freshDir("build/test-music-overlay-missing-custom")
        CustomAudioSetManager(filesDir).addSet(
            CustomAudioSetManager.AudioSet("custom", "Custom", listOf("missing.ogg")),
        )

        assertEquals(
            listOf(MusicOverlaySourceOption("midi", "Base game MIDI")),
            musicOverlaySourceOptions(filesDir, "d2"),
        )
    }

    private fun freshDir(path: String): File =
        File(path).absoluteFile.also {
            it.deleteRecursively()
            it.mkdirs()
        }
}
