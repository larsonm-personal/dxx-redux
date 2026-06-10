package com.dxxredux.app

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.security.MessageDigest
import java.util.Locale

class MissionZipAudioFingerprintCache(
    private val filesDir: File,
) {
    data class Entry(
        val archiveName: String,
        val archiveSize: Long,
        val archiveMtime: Long,
        val trackId: String,
        val entryPath: String,
        val nestedPath: String,
        val hogEntryName: String,
        val contentSha256: String,
        val durationMs: Int,
        val chromaprint: String,
        val localMatchName: String?,
        val localMatchConfidence: Float?,
        val localMatchDiscId: String?,
        val localMatchTrack: Int?,
        val acoustIdName: String?,
        val acoustIdLookupStatus: String?,
        val acoustIdLookupAt: Long?,
        val lookupAt: Long,
    ) {
        val hasLocalMatch: Boolean get() = !localMatchName.isNullOrBlank()
        val hasAcoustIdLookup: Boolean get() = !acoustIdLookupStatus.isNullOrBlank()
    }

    private val cacheFile = File(filesDir, CACHE_FILE)

    fun cachedEntries(catalog: MissionZipMusicCatalog): Map<String, Entry> {
        val archive = File(catalog.archivePath)
        return loadEntries()
            .filter { it.matchesArchive(archive) }
            .groupBy { it.trackId }
            .mapValues { (_, entries) -> entries.maxBy { it.lookupAt } }
    }

    fun get(
        catalog: MissionZipMusicCatalog,
        track: MissionZipMusicTrack,
        contentSha256: String,
    ): Entry? {
        val archive = File(catalog.archivePath)
        return loadEntries()
            .filter { it.matchesArchive(archive) && it.matchesTrack(track) && it.contentSha256 == contentSha256 }
            .maxByOrNull { it.lookupAt }
    }

    fun record(
        catalog: MissionZipMusicCatalog,
        track: MissionZipMusicTrack,
        stagedAudio: File,
        fingerprint: FingerprintBridge.FingerprintResult,
        match: FingerprintBridge.MatchResult?,
    ): Entry {
        val archive = File(catalog.archivePath)
        val contentSha256 = sha256(stagedAudio)
        val entry =
            Entry(
                archiveName = archive.name,
                archiveSize = archive.length(),
                archiveMtime = archive.lastModified(),
                trackId = track.id,
                entryPath = track.archiveEntryPath,
                nestedPath = track.nestedEntryPath.orEmpty(),
                hogEntryName = track.hogEntryName.orEmpty(),
                contentSha256 = contentSha256,
                durationMs = fingerprint.durationMs,
                chromaprint = fingerprint.encoded,
                localMatchName = match?.name,
                localMatchConfidence = match?.confidence,
                localMatchDiscId = match?.discId,
                localMatchTrack = match?.trackNum,
                acoustIdName = null,
                acoustIdLookupStatus = null,
                acoustIdLookupAt = null,
                lookupAt = System.currentTimeMillis(),
            )
        val entries =
            loadEntries()
                .filterNot {
                    it.matchesArchive(archive) &&
                        it.matchesTrack(track) &&
                        it.contentSha256 == contentSha256
                } + entry
        saveEntries(entries)
        return entry
    }

    fun identifyLocal(
        context: Context,
        catalog: MissionZipMusicCatalog,
        track: MissionZipMusicTrack,
        stagedAudio: File,
    ): Entry? {
        if (!isFingerprintSupported(track)) return null
        val contentSha256 = sha256(stagedAudio)
        get(catalog, track, contentSha256)?.let { return it }
        FingerprintBridge.ensureDbLoaded(context)
        val fingerprint = FingerprintBridge.fingerprintAudioFile(stagedAudio.absolutePath) ?: return null
        val match = FingerprintBridge.matchFingerprint(fingerprint.encoded, fingerprint.durationMs)
        return record(catalog, track, stagedAudio, fingerprint, match)
    }

    fun recordAcoustIdResult(
        entry: Entry,
        name: String?,
        status: String,
    ): Entry {
        val updated =
            entry.copy(
                acoustIdName = name,
                acoustIdLookupStatus = status,
                acoustIdLookupAt = System.currentTimeMillis(),
            )
        val entries = loadEntries().filterNot { it.sameCacheRecord(entry) } + updated
        saveEntries(entries)
        return updated
    }

    private fun loadEntries(): List<Entry> {
        if (!cacheFile.isFile) return emptyList()
        return runCatching {
            val root = JSONObject(cacheFile.readText())
            val entries = root.optJSONArray("entries") ?: JSONArray()
            (0 until entries.length()).mapNotNull { index ->
                entries.optJSONObject(index)?.toEntry()
            }
        }.getOrDefault(emptyList())
    }

    private fun saveEntries(entries: List<Entry>) {
        cacheFile.parentFile?.mkdirs()
        val root = JSONObject()
        root.put("schema", SCHEMA)
        val array = JSONArray()
        entries
            .sortedWith(
                compareBy<Entry> { it.archiveName.lowercase(Locale.US) }
                    .thenBy { it.entryPath.lowercase(Locale.US) }
                    .thenBy { it.nestedPath.lowercase(Locale.US) }
                    .thenBy { it.hogEntryName.lowercase(Locale.US) }
                    .thenBy { it.contentSha256 },
            ).forEach { array.put(it.toJson()) }
        root.put("entries", array)
        cacheFile.writeText(root.toString(2))
    }

    private fun Entry.matchesArchive(archive: File): Boolean =
        archiveName == archive.name && archiveSize == archive.length() && archiveMtime == archive.lastModified()

    private fun Entry.matchesTrack(track: MissionZipMusicTrack): Boolean =
        trackId == track.id &&
            entryPath == track.archiveEntryPath &&
            nestedPath == track.nestedEntryPath.orEmpty() &&
            hogEntryName == track.hogEntryName.orEmpty()

    private fun Entry.toJson(): JSONObject =
        JSONObject()
            .put("archive_name", archiveName)
            .put("archive_size", archiveSize)
            .put("archive_mtime", archiveMtime)
            .put("track_id", trackId)
            .put("entry_path", entryPath)
            .put("nested_path", nestedPath)
            .put("hog_entry_name", hogEntryName)
            .put("sha256", contentSha256)
            .put("duration_ms", durationMs)
            .put("chromaprint", chromaprint)
            .put("lookup_at", lookupAt)
            .apply {
                localMatchName?.let { put("local_match_name", it) }
                localMatchConfidence?.let { put("local_match_confidence", it.toDouble()) }
                localMatchDiscId?.let { put("local_match_disc_id", it) }
                localMatchTrack?.let { put("local_match_track", it) }
                acoustIdName?.let { put("acoustid_name", it) }
                acoustIdLookupStatus?.let { put("acoustid_lookup_status", it) }
                acoustIdLookupAt?.let { put("acoustid_lookup_at", it) }
            }

    private fun JSONObject.toEntry(): Entry? =
        runCatching {
            Entry(
                archiveName = getString("archive_name"),
                archiveSize = getLong("archive_size"),
                archiveMtime = getLong("archive_mtime"),
                trackId = getString("track_id"),
                entryPath = getString("entry_path"),
                nestedPath = optString("nested_path"),
                hogEntryName = optString("hog_entry_name"),
                contentSha256 = getString("sha256"),
                durationMs = getInt("duration_ms"),
                chromaprint = getString("chromaprint"),
                localMatchName = optString("local_match_name").takeIf { it.isNotBlank() },
                localMatchConfidence =
                    if (has("local_match_confidence")) {
                        getDouble("local_match_confidence").toFloat()
                    } else {
                        null
                    },
                localMatchDiscId = optString("local_match_disc_id").takeIf { it.isNotBlank() },
                localMatchTrack = if (has("local_match_track")) getInt("local_match_track") else null,
                acoustIdName = optString("acoustid_name").takeIf { it.isNotBlank() },
                acoustIdLookupStatus = optString("acoustid_lookup_status").takeIf { it.isNotBlank() },
                acoustIdLookupAt = if (has("acoustid_lookup_at")) getLong("acoustid_lookup_at") else null,
                lookupAt = optLong("lookup_at", 0L),
            )
        }.getOrNull()

    private fun Entry.sameCacheRecord(other: Entry): Boolean =
        archiveName == other.archiveName &&
            archiveSize == other.archiveSize &&
            archiveMtime == other.archiveMtime &&
            trackId == other.trackId &&
            entryPath == other.entryPath &&
            nestedPath == other.nestedPath &&
            hogEntryName == other.hogEntryName &&
            contentSha256 == other.contentSha256

    private fun sha256(file: File): String =
        MessageDigest
            .getInstance("SHA-256")
            .also { digest ->
                file.inputStream().use { input ->
                    val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
                    while (true) {
                        val read = input.read(buffer)
                        if (read < 0) break
                        digest.update(buffer, 0, read)
                    }
                }
            }.digest()
            .joinToString("") { "%02x".format(it) }

    companion object {
        private const val CACHE_FILE = "mission_zip_audio_fingerprints.json"
        private const val SCHEMA = "dxx-mission-zip-audio-fingerprints-v1"
        const val ACOUSTID_STATUS_OK = "ok"
        const val ACOUSTID_STATUS_NO_MATCH = "no_match"
        const val ACOUSTID_STATUS_FAILED = "failed"
        private val FINGERPRINT_EXTENSIONS = setOf("flac", "mp3", "ogg")

        fun isFingerprintSupported(track: MissionZipMusicTrack): Boolean =
            track.playable &&
                track.kind == MissionZipMusic.KIND_COMPRESSED_AUDIO &&
                track.extension.lowercase(Locale.US) in FINGERPRINT_EXTENSIONS
    }
}
