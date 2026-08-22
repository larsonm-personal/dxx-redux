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
    fun activeCdSourceRemainsSelectableWithoutLauncherRegistry() {
        val filesDir = freshDir("build/test-music-overlay-active-cd-no-registry")

        assertEquals(
            listOf(
                MusicOverlaySourceOption("cd", "CD"),
                MusicOverlaySourceOption("midi", "Base game MIDI"),
            ),
            musicOverlaySourceOptions(filesDir, "d1", activeSource = "cd"),
        )
    }

    @Test
    fun launcherFilesAndCdAppearOnlyWhenEnabled() {
        val filesDir = freshDir("build/test-music-overlay-sources")
        val setDir = FileSetManager(filesDir).getSetDir(FileSetManager.DEFAULT_SET)
        val customAudioManager = CustomAudioSetManager(filesDir, setDir)
        customAudioManager.addSet(
            CustomAudioSetManager.AudioSet(
                id = "custom",
                label = "Custom",
                files = listOf("track.ogg"),
                enabled = true,
            ),
        )
        File(customAudioManager.setDir("custom"), "track.ogg").apply {
            parentFile?.mkdirs()
            writeText("audio")
        }
        File(setDir, ".content/audio/audio_sources.json").apply {
            parentFile?.mkdirs()
        }.writeText(
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
        File(setDir, "disc.cue").writeText("FILE \"disc.bin\" BINARY\n")
        File(setDir, "disc.bin").writeText("audio")

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
    fun cdInActiveFileSetAppearsInOverlay() {
        val filesDir = freshDir("build/test-music-overlay-active-set-cd")
        val setDir = FileSetManager(filesDir).getSetDir(FileSetManager.DEFAULT_SET)
        File(setDir, ".content/audio/audio_sources.json").apply {
            parentFile?.mkdirs()
        }.writeText(
            """
            {"sources":[{"id":"macplay","cue":"macplay.cue","bins":["macplay.bin"],
            "label":"MacPlay","disc_id":"descent-mac-macplay","track_count":14,
            "audio_track_count":13,"legacy_disc_id":0,"enabled":true}]}
            """.trimIndent(),
        )
        File(setDir, "macplay.cue").writeText("FILE \"macplay.bin\" BINARY\n")
        File(setDir, "macplay.bin").writeText("audio")

        assertEquals(
            listOf(
                MusicOverlaySourceOption("cd", "CD"),
                MusicOverlaySourceOption("midi", "Base game MIDI"),
            ),
            musicOverlaySourceOptions(filesDir, "d1"),
        )
    }

    @Test
    fun cdOptionRequiresCompleteAccessiblePlaylist() {
        val filesDir = freshDir("build/test-music-overlay-invalid-cd")
        val setDir = FileSetManager(filesDir).getSetDir(FileSetManager.DEFAULT_SET)
        File(setDir, ".content/audio/audio_sources.json").apply {
            parentFile?.mkdirs()
        }.writeText(
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
