package com.dxxredux.app

import java.io.File

internal data class MusicOverlaySourceOption(
    val id: String,
    val label: String,
)

internal fun musicOverlaySourceOptions(
    filesDir: File,
    game: String,
    activeSource: String? = null,
    hasAddonMission: Boolean = false,
    canAccessUri: (uri: String, useFileDescriptor: Boolean) -> Boolean = { _, _ -> true },
): List<MusicOverlaySourceOption> =
    buildList {
        if (hasAddonMission && ModManager.forActiveSet(filesDir).hasEnabledMissionZipSoundtrack(game)) {
            add(MusicOverlaySourceOption("mission", "Mission zip"))
        }
        if (CustomAudioSetManager.forActiveSet(filesDir).hasUsableTrack { uri -> canAccessUri(uri, true) }) {
            add(MusicOverlaySourceOption("files", "Files"))
        }
        val audioSourceManager = AudioSourceManager.forActiveSet(filesDir)
        val cdSources = audioSourceManager.getEnabledSources()
        val cdAvailable =
            runCatching {
                val availableSources =
                    cdSources.filter { source ->
                        audioSourceManager.sourceFilesAvailable(source) &&
                            source
                                .binContentUris
                                .filterNot(::isLocalCdContentPath)
                                .all { canAccessUri(it, true) }
                    }
                requireAudioPlaylistCapacity(availableSources)
                availableSources.isNotEmpty()
            }.getOrDefault(false)
        if (cdAvailable || activeSource == "cd") {
            add(MusicOverlaySourceOption("cd", "CD"))
        }
        add(MusicOverlaySourceOption("midi", "Base game MIDI"))
    }
