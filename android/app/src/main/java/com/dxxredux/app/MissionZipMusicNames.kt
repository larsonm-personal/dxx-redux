package com.dxxredux.app

import android.content.Context
import org.json.JSONObject
import java.io.File
import java.util.Locale

internal const val MISSION_ZIP_MUSIC_NAMES_FILE = "mission_music_names.json"

internal object MissionZipMusicNames {
    private const val CACHE_DIR = ".mission_zip_music_names"
    private const val PLACEHOLDER_NAME = "[unknown] - [untitled]"

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
        outputFile.isFile &&
            identityFile(outputFile).let {
                it.isFile && it.readText(Charsets.US_ASCII) == catalog.sourceIdentity
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
    ): Int {
        val names = nameMap(catalog, entries)
        if (names.isEmpty()) {
            outputFile.delete()
            identityFile(outputFile).delete()
            return 0
        }
        outputFile.parentFile?.mkdirs()
        val identityFile = identityFile(outputFile)
        identityFile.delete()
        val root = JSONObject()
        names.toSortedMap(String.CASE_INSENSITIVE_ORDER).forEach { (key, value) -> root.put(key, value) }
        outputFile.writeText(root.toString(2), Charsets.UTF_8)
        identityFile.writeText(catalog.sourceIdentity, Charsets.US_ASCII)
        return names.size
    }

    private fun nameMap(
        catalog: MissionZipMusicCatalog,
        entries: Map<String, MissionZipAudioFingerprintCache.Entry>,
    ): Map<String, String> =
        buildMap {
            for (track in catalog.sources.flatMap { it.tracks }) {
                val name = entries[track.id]?.bestName() ?: continue
                for (key in track.lookupKeys()) {
                    if (key.isNotBlank()) putIfAbsent(key, name)
                }
            }
        }

    private fun MissionZipAudioFingerprintCache.Entry.bestName(): String? =
        (localMatchName ?: acoustIdName)
            ?.trim()
            ?.takeIf { it.isNotBlank() && it != PLACEHOLDER_NAME }

    private fun MissionZipMusicTrack.lookupKeys(): List<String> =
        listOf(
            displayName,
            leafName(displayName),
            archiveEntryPath,
            leafName(archiveEntryPath),
            nestedEntryPath.orEmpty(),
            leafName(nestedEntryPath.orEmpty()),
            hogEntryName.orEmpty(),
            leafName(hogEntryName.orEmpty()),
        ).map { it.replace('\\', '/').trim('/') }
            .filter { it.isNotBlank() }
            .distinctBy { it.lowercase(Locale.US) }

    private fun leafName(path: String): String = path.replace('\\', '/').substringAfterLast('/')

    private fun identityFile(outputFile: File): File = File(outputFile.parentFile, "${outputFile.name}.identity")
}
