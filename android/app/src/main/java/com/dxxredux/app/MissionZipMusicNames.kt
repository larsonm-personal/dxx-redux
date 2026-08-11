package com.dxxredux.app

import android.content.Context
import java.io.File
import java.util.Locale

internal const val MISSION_ZIP_MUSIC_NAMES_FILE = "mission_music_names.json"

internal object MissionZipMusicNames {
    private const val CACHE_DIR = ".mission_zip_music_names"
    private const val PLACEHOLDER_NAME = "[unknown] - [untitled]"
    private const val SOURCE_IDENTITY_KEY = "sourceIdentity"

    fun cacheFile(
        filesDir: File,
        ownerFilename: String,
    ): File = File(File(File(filesDir, "mods"), CACHE_DIR), "${safeMissionZipDirName(ownerFilename)}.json")

    fun deleteCacheFile(
        filesDir: File,
        ownerFilename: String,
    ) {
        cacheFile(filesDir, ownerFilename).delete()
        identityFile(cacheFile(filesDir, ownerFilename)).delete()
    }

    fun isCurrent(
        outputFile: File,
        catalog: MissionZipMusicCatalog,
    ): Boolean =
        AtomicFilePublication.transaction {
            runCatching {
                val root = org.json.JSONObject(outputFile.readText())
                outputFile.isFile &&
                    root.optInt("version") == MusicNameSidecar.VERSION &&
                    root.optJSONArray("records") != null &&
                    root.optString(SOURCE_IDENTITY_KEY) == catalog.sourceIdentity
            }.getOrDefault(false)
        }

    fun identifyLocalAndWrite(
        context: Context,
        filesDir: File,
        catalog: MissionZipMusicCatalog,
        outputFile: File,
        onProgress: (Int, Int, String) -> Unit = { _, _, _ -> },
    ): Int {
        val tracks =
            catalog.sources
                .flatMap { it.tracks }
                .filter(MissionZipAudioFingerprintCache::isFingerprintSupported)
        val entries = MissionZipAudioFingerprintCache(filesDir).cachedEntries(catalog).toMutableMap()
        if (tracks.isEmpty()) return writeSidecar(outputFile, catalog, entries)

        val stageManager = MissionZipMusicStageManager(context.cacheDir)
        val cache = MissionZipAudioFingerprintCache(filesDir)
        tracks.forEachIndexed { index, track ->
            onProgress(index, tracks.size, track.displayName)
            val entry =
                runCatching {
                    val staged = stageManager.stageCompressedAudioTrack(catalog, track) ?: return@runCatching null
                    cache.identifyLocal(context, catalog, track, staged)
                }.getOrNull()
            if (entry != null) entries[track.id] = entry
            onProgress(index + 1, tracks.size, track.displayName)
        }
        return writeSidecar(outputFile, catalog, entries)
    }

    fun writeSidecar(
        outputFile: File,
        catalog: MissionZipMusicCatalog,
        entries: Map<String, MissionZipAudioFingerprintCache.Entry>,
        beforePublish: (File, File) -> Unit = { _, _ -> },
    ): Int =
        AtomicFilePublication.transaction {
            val records = records(catalog, entries)
            if (records.isEmpty()) {
                outputFile.delete()
                identityFile(outputFile).delete()
                return@transaction 0
            }
            AtomicFilePublication.writeUtf8(
                outputFile,
                MusicNameSidecar.encode(records, catalog.sourceIdentity),
                beforePublish,
            )
            identityFile(outputFile).delete()
            records.size
        }

    private fun records(
        catalog: MissionZipMusicCatalog,
        entries: Map<String, MissionZipAudioFingerprintCache.Entry>,
    ): List<MusicNameSidecar.Record> =
        buildList {
            for (track in catalog.sources.flatMap { it.tracks }) {
                val name = entries[track.id]?.bestName() ?: continue
                val paths = track.exactPaths()
                if (paths.isEmpty()) continue
                add(MusicNameSidecar.Record(paths, paths.map(::leafName), name))
            }
        }

    private fun MissionZipAudioFingerprintCache.Entry.bestName(): String? =
        (localMatchName ?: acoustIdName)
            ?.trim()
            ?.takeIf { it.isNotBlank() && it != PLACEHOLDER_NAME }

    private fun MissionZipMusicTrack.exactPaths(): List<String> =
        listOf(
            archiveEntryPath,
            nestedEntryPath.orEmpty(),
            hogEntryName.orEmpty(),
            displayName.takeIf { '/' in it || '\\' in it }.orEmpty(),
        ).map { it.replace('\\', '/').trim('/') }
            .filter { it.isNotBlank() }
            .distinctBy { it.lowercase(Locale.US) }

    private fun leafName(path: String): String = path.replace('\\', '/').substringAfterLast('/')

    private fun identityFile(outputFile: File): File = File(outputFile.parentFile, "${outputFile.name}.identity")
}
