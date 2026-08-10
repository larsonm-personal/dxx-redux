package com.dxxredux.app

import java.io.File

internal data class MusicOverlaySourceOption(
    val id: String,
    val label: String,
)

internal fun musicOverlaySourceOptions(
    filesDir: File,
    game: String,
    canAccessUri: (uri: String, useFileDescriptor: Boolean) -> Boolean = { _, _ -> true },
): List<MusicOverlaySourceOption> =
    buildList {
        if (ModManager(filesDir).hasEnabledMissionZipSoundtrack(game)) {
            add(MusicOverlaySourceOption("mission", "Mission zip"))
        }
        if (CustomAudioSetManager(filesDir).getEnabledSets().any { it.files.isNotEmpty() }) {
            add(MusicOverlaySourceOption("files", "Files"))
        }
        val cdSources = AudioSourceManager(filesDir).getEnabledSources()
        val cdAvailable =
            runCatching {
                requireAudioPlaylistCapacity(cdSources)
                cdSources.isNotEmpty() &&
                    cdSources.all { source ->
                        val cueOk =
                            source.cueContentUri?.let { uri ->
                                if (isLocalCdContentPath(uri)) File(uri).isFile else canAccessUri(uri, false)
                            } ?: resolveCdAudioSourceFile(filesDir, source.cuePath).isFile
                        val binUris = source.binContentUriList()
                        val binsOk =
                            if (binUris.isNotEmpty()) {
                                binUris.all { uri ->
                                    if (isLocalCdContentPath(uri)) File(uri).isFile else canAccessUri(uri, true)
                                }
                            } else {
                                source.binPaths.all { resolveCdAudioSourceFile(filesDir, it).isFile }
                            }
                        cueOk && binsOk
                    }
            }.getOrDefault(false)
        if (cdAvailable) {
            add(MusicOverlaySourceOption("cd", "CD"))
        }
        add(MusicOverlaySourceOption("midi", "Base game MIDI"))
    }
