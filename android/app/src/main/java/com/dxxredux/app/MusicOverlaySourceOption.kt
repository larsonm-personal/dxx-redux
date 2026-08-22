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
    canAccessUri: (uri: String, useFileDescriptor: Boolean) -> Boolean = { _, _ -> true },
): List<MusicOverlaySourceOption> =
    buildList {
        if (ModManager.forActiveSet(filesDir).hasEnabledMissionZipSoundtrack(game)) {
            add(MusicOverlaySourceOption("mission", "Mission zip"))
        }
        if (CustomAudioSetManager.forActiveSet(filesDir).hasUsableTrack { uri -> canAccessUri(uri, true) }) {
            add(MusicOverlaySourceOption("files", "Files"))
        }
        val audioSourceManager = AudioSourceManager.forActiveSet(filesDir)
        val cdSources = audioSourceManager.getEnabledSources()
        val cdAvailable =
            runCatching {
                requireAudioPlaylistCapacity(cdSources)
                cdSources.isNotEmpty() &&
                    cdSources.all { source ->
                        audioSourceManager.sourceFilesAvailable(source) &&
                            source.cueContentUri
                                ?.takeUnless(::isLocalCdContentPath)
                                ?.let { canAccessUri(it, false) } != false &&
                            source
                                .binContentUriList()
                                .filterNot(::isLocalCdContentPath)
                                .all { canAccessUri(it, true) }
                    }
            }.getOrDefault(false)
        if (cdAvailable || activeSource == "cd") {
            add(MusicOverlaySourceOption("cd", "CD"))
        }
        add(MusicOverlaySourceOption("midi", "Base game MIDI"))
    }
