package com.dxxredux.app

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.IOException
import java.security.MessageDigest
import java.util.Locale

class MissionZipAudioFingerprintCache private constructor(
    private val filesDir: File,
    private val beforePublish: (File, File) -> Unit,
) {
    constructor(filesDir: File) : this(filesDir, { _, _ -> })

    internal constructor(
        filesDir: File,
        beforePublish: (File, File) -> Unit,
        testOnly: Unit = Unit,
    ) : this(filesDir, beforePublish)

    data class Entry(
        val archiveName: String,
        val archiveSize: Long,
        val archiveMtime: Long,
        val sourceIdentity: String,
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
        val localMatchDbIdentity: String?,
        val acoustIdName: String?,
        val acoustIdLookupStatus: String?,
        val acoustIdLookupAt: Long?,
        val lookupAt: Long,
        val acoustIdScore: Double? = null,
        val acoustIdRecordingId: String? = null,
    ) {
        val hasLocalMatch: Boolean get() = !localMatchName.isNullOrBlank()
        val hasAuthoritativeAcoustIdLookup: Boolean
            get() = acoustIdLookupStatus == ACOUSTID_STATUS_OK || acoustIdLookupStatus == ACOUSTID_STATUS_NO_MATCH
    }

    private val cacheFile = File(filesDir, CACHE_FILE)

    fun cachedEntries(catalog: MissionZipMusicCatalog): Map<String, Entry> =
        AtomicFilePublication.transaction {
            loadEntries()
                .filter { it.sourceIdentity == catalog.sourceIdentity }
                .groupBy { it.trackId }
                .mapValues { (_, entries) -> entries.maxBy { it.lookupAt } }
        }

    fun get(
        catalog: MissionZipMusicCatalog,
        track: MissionZipMusicTrack,
        contentSha256: String,
    ): Entry? =
        AtomicFilePublication.transaction {
            loadEntries()
                .filter {
                    it.sourceIdentity == catalog.sourceIdentity &&
                        it.matchesTrack(track) &&
                        it.contentSha256 == contentSha256
                }.maxByOrNull { it.lookupAt }
        }

    fun record(
        catalog: MissionZipMusicCatalog,
        track: MissionZipMusicTrack,
        stagedAudio: File,
        fingerprint: FingerprintBridge.FingerprintResult,
        match: FingerprintBridge.MatchResult?,
        localMatchDbIdentity: String? = null,
    ): Entry =
        AtomicFilePublication.transaction {
            val archive = File(catalog.archivePath)
            val contentSha256 = sha256(stagedAudio)
            val entry =
                Entry(
                    archiveName = archive.name,
                    archiveSize = archive.length(),
                    archiveMtime = archive.lastModified(),
                    sourceIdentity = catalog.sourceIdentity,
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
                    localMatchDbIdentity = localMatchDbIdentity,
                    acoustIdName = null,
                    acoustIdScore = null,
                    acoustIdRecordingId = null,
                    acoustIdLookupStatus = null,
                    acoustIdLookupAt = null,
                    lookupAt = System.currentTimeMillis(),
                )
            val entries =
                loadEntries()
                    .filterNot {
                        it.sourceIdentity == catalog.sourceIdentity &&
                            it.matchesTrack(track) &&
                            it.contentSha256 == contentSha256
                    } + entry
            saveEntries(entries)
            entry
        }

    fun identifyLocal(
        context: Context,
        catalog: MissionZipMusicCatalog,
        track: MissionZipMusicTrack,
        stagedAudio: File,
    ): Entry? {
        if (!isFingerprintSupported(track)) return null
        val contentSha256 = sha256(stagedAudio)
        FingerprintBridge.ensureDbLoaded(context)
        val dbIdentity = FingerprintBridge.databaseIdentity(context)
        get(catalog, track, contentSha256)?.let { cached ->
            if (cached.localMatchDbIdentity == dbIdentity) return cached
            val match = FingerprintBridge.matchFingerprint(cached.chromaprint, cached.durationMs)
            return recordLocalMatchResult(cached, match, dbIdentity)
        }
        val fingerprint = FingerprintBridge.fingerprintAudioFile(stagedAudio.absolutePath) ?: return null
        val match = FingerprintBridge.matchFingerprint(fingerprint.encoded, fingerprint.durationMs)
        return record(catalog, track, stagedAudio, fingerprint, match, dbIdentity)
    }

    fun recordLocalMatchResult(
        entry: Entry,
        match: FingerprintBridge.MatchResult?,
        localMatchDbIdentity: String,
    ): Entry =
        AtomicFilePublication.transaction {
            val entries = loadEntries()
            val current = entries.filter { it.sameCacheRecord(entry) }.maxByOrNull { it.lookupAt } ?: entry
            val updated =
                current.copy(
                    localMatchName = match?.name,
                    localMatchConfidence = match?.confidence,
                    localMatchDiscId = match?.discId,
                    localMatchTrack = match?.trackNum,
                    localMatchDbIdentity = localMatchDbIdentity,
                    lookupAt = System.currentTimeMillis(),
                )
            saveEntries(entries.filterNot { it.sameCacheRecord(entry) } + updated)
            updated
        }

    fun recordAcoustIdResult(
        entry: Entry,
        name: String?,
        status: String,
        score: Double? = null,
        recordingId: String? = null,
    ): Entry =
        AtomicFilePublication.transaction {
            val entries = loadEntries()
            val current = entries.filter { it.sameCacheRecord(entry) }.maxByOrNull { it.lookupAt } ?: entry
            val updated =
                current.copy(
                    acoustIdName = name,
                    acoustIdScore = score,
                    acoustIdRecordingId = recordingId,
                    acoustIdLookupStatus = status,
                    acoustIdLookupAt = System.currentTimeMillis(),
                )
            saveEntries(entries.filterNot { it.sameCacheRecord(entry) } + updated)
            updated
        }

    internal fun recordAcoustIdResult(
        entry: Entry,
        result: AcoustIdLookupResult,
    ): Entry =
        when (result) {
            is AcoustIdLookupResult.Match -> {
                recordAcoustIdResult(
                    entry,
                    result.match.name,
                    ACOUSTID_STATUS_OK,
                    result.match.score,
                    result.match.recordingId,
                )
            }

            AcoustIdLookupResult.NoMatch -> {
                recordAcoustIdResult(entry, null, ACOUSTID_STATUS_NO_MATCH)
            }

            is AcoustIdLookupResult.RetryableFailure -> {
                recordAcoustIdResult(entry, null, ACOUSTID_STATUS_RETRYABLE_FAILURE)
            }

            is AcoustIdLookupResult.ConfigurationFailure -> {
                recordAcoustIdResult(entry, null, ACOUSTID_STATUS_CONFIGURATION_FAILURE)
            }
        }

    private fun loadEntries(): List<Entry> {
        if (!cacheFile.isFile) return emptyList()
        return try {
            val root = JSONObject(cacheFile.readText())
            if (root.optString("schema") != SCHEMA) return emptyList()
            val entries = root.optJSONArray("entries") ?: JSONArray()
            (0 until entries.length()).map { index ->
                entries.optJSONObject(index)?.toEntry()
                    ?: throw IOException("Invalid fingerprint cache entry $index")
            }
        } catch (failure: Exception) {
            val quarantine = AtomicFilePublication.uniqueSibling(cacheFile, "corrupt")
            if (!cacheFile.renameTo(quarantine)) throw failure
            emptyList()
        }
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
        AtomicFilePublication.writeUtf8(cacheFile, root.toString(2), beforePublish)
    }

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
            .put("source_identity", sourceIdentity)
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
                localMatchDbIdentity?.let { put("local_match_db_identity", it) }
                acoustIdName?.let { put("acoustid_name", it) }
                acoustIdScore?.let { put("acoustid_score", it) }
                acoustIdRecordingId?.let { put("acoustid_recording_id", it) }
                acoustIdLookupStatus?.let { put("acoustid_lookup_status", it) }
                acoustIdLookupAt?.let { put("acoustid_lookup_at", it) }
            }

    private fun JSONObject.toEntry(): Entry? =
        runCatching {
            Entry(
                archiveName = getString("archive_name"),
                archiveSize = getLong("archive_size"),
                archiveMtime = getLong("archive_mtime"),
                sourceIdentity = getString("source_identity"),
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
                localMatchDbIdentity = optString("local_match_db_identity").takeIf { it.isNotBlank() },
                acoustIdName = optString("acoustid_name").takeIf { it.isNotBlank() },
                acoustIdScore = if (has("acoustid_score")) getDouble("acoustid_score") else null,
                acoustIdRecordingId = optString("acoustid_recording_id").takeIf { it.isNotBlank() },
                acoustIdLookupStatus = optString("acoustid_lookup_status").takeIf { it.isNotBlank() },
                acoustIdLookupAt = if (has("acoustid_lookup_at")) getLong("acoustid_lookup_at") else null,
                lookupAt = optLong("lookup_at", 0L),
            )
        }.getOrNull()

    private fun Entry.sameCacheRecord(other: Entry): Boolean =
        sourceIdentity == other.sourceIdentity &&
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
        private const val SCHEMA = "dxx-mission-zip-audio-fingerprints-v2"
        const val ACOUSTID_STATUS_OK = "ok"
        const val ACOUSTID_STATUS_NO_MATCH = "no_match"
        const val ACOUSTID_STATUS_RETRYABLE_FAILURE = "retryable_failure"
        const val ACOUSTID_STATUS_CONFIGURATION_FAILURE = "configuration_failure"

        // Retained only to ensure v1 cache entries written before typed outcomes remain retryable.
        const val ACOUSTID_STATUS_FAILED = "failed"
        private val FINGERPRINT_EXTENSIONS = setOf("flac", "mp3", "ogg")

        fun isFingerprintSupported(track: MissionZipMusicTrack): Boolean =
            track.playable &&
                track.kind == MissionZipMusic.KIND_COMPRESSED_AUDIO &&
                track.extension.lowercase(Locale.US) in FINGERPRINT_EXTENSIONS
    }
}
