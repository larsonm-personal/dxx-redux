package com.dxxredux.app

internal data class MusicEditorArchiveSource(
    val info: MidiEnumerationBridge.SourceInfo,
    val catalog: MissionZipMusicCatalog,
    val tracks: Map<String, MissionZipMusicTrack>,
)

internal fun musicEditorArchiveSources(
    mod: ModManager.ModInfo,
    catalog: MissionZipMusicCatalog,
): List<MusicEditorArchiveSource> =
    catalog.sources.mapNotNull { source ->
        val tracks = source.tracks.filter { it.playable && it.kind == MissionZipMusic.KIND_MIDI }
        if (tracks.isEmpty()) return@mapNotNull null
        MusicEditorArchiveSource(
            info =
                MidiEnumerationBridge.SourceInfo(
                    id = "mod:${mod.filename}/${source.id}",
                    label = "${mod.missionTitle ?: mod.displayName} - ${source.label}",
                    game = mod.game,
                    tracks = tracks.map { MidiEnumerationBridge.TrackInfo(filename = it.id) },
                ),
            catalog = catalog,
            tracks = tracks.associateBy { it.id },
        )
    }

internal fun loadMusicEditorArchiveSources(manager: ModManager): List<MusicEditorArchiveSource> =
    manager.listMods().flatMap { mod ->
        val archive = manager.modFile(mod.filename)
        val record = manager.extractionStore().reusableRecord(mod.filename, archive)
        val catalog = record?.let { MissionZipMusic.inspectExtracted(it) } ?: MissionZipMusic.inspect(archive)
        catalog?.let { musicEditorArchiveSources(mod, it) }.orEmpty()
    }

internal fun orderedMidiEditorSources(
    sources: List<MidiEnumerationBridge.SourceInfo>,
): List<MidiEnumerationBridge.SourceInfo> =
    sources.sortedWith(
        compareBy<MidiEnumerationBridge.SourceInfo> {
            when (it.id) {
                "d1-builtin" -> 0
                "d2-builtin" -> 1
                else -> 2
            }
        }.thenBy(String.CASE_INSENSITIVE_ORDER) { it.label }.thenBy { it.id },
    )
