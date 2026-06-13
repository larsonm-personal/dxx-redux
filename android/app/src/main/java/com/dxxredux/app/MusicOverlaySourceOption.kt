package com.dxxredux.app

import java.io.File

internal data class MusicOverlaySourceOption(
    val id: String,
    val label: String,
)

internal fun musicOverlaySourceOptions(
    filesDir: File,
    game: String,
): List<MusicOverlaySourceOption> =
    buildList {
        if (ModManager(filesDir).hasEnabledMissionZipSoundtrack(game)) {
            add(MusicOverlaySourceOption("mission", "Mission zip"))
        }
        if (CustomAudioSetManager(filesDir).getEnabledSets().any { it.files.isNotEmpty() }) {
            add(MusicOverlaySourceOption("files", "Files"))
        }
        if (AudioSourceManager(filesDir).getEnabledSources().any { it.audioTrackCount > 0 }) {
            add(MusicOverlaySourceOption("cd", "CD"))
        }
        add(MusicOverlaySourceOption("midi", "Base game MIDI"))
    }
