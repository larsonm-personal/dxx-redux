package com.dxxredux.app

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import org.json.JSONTokener
import java.io.File
import java.security.MessageDigest

// Filter AcoustID placeholder names where both artist and title are unknown
private fun isPlaceholderName(name: String): Boolean = name == "[unknown] - [untitled]"

internal fun fingerprintTrackName(track: JSONObject): String? {
    val trackNumber = track.optInt("track", 0)
    val fallback = trackNumber.takeIf { it > 0 }?.let { "Track $it" }
    val maintainedName = track.optString("name").trim().takeIf { it.isNotEmpty() }
    val tracklistName =
        track
            .optString("tracklist_name")
            .trim()
            .takeIf { track.optString("name_source") == "tracklist" && it.isNotEmpty() }
    val acoustidName =
        maintainedName?.let { sourceName ->
            track
                .optString("acoustid_name")
                .trim()
                .takeIf { it.isNotEmpty() }
                ?.takeIf { AcoustIdLabelPolicy.labelsAgree(sourceName, it) }
        }
    return (tracklistName ?: acoustidName ?: maintainedName ?: fallback)
        ?.takeUnless(::isPlaceholderName)
        ?: fallback
}

internal data class FingerprintMatchingConfig(
    val matchThreshold: Float,
    val durationTolerance: Float,
)

internal fun parseFingerprintMatchingConfig(raw: String): FingerprintMatchingConfig {
    val tokener = JSONTokener(Jsonc.strip(raw))
    val config = tokener.nextValue()
    require(config is JSONObject && tokener.nextClean().code == 0) {
        "Fingerprint configuration must contain exactly one object"
    }

    fun requiredFraction(name: String): Float {
        require(config.has(name)) { "Missing $name" }
        val value = config.get(name)
        require(value is Number) { "$name must be numeric" }
        val number = value.toDouble()
        require(number.isFinite() && number > 0.0 && number <= 1.0) {
            "$name must be finite and in (0, 1]"
        }
        return number.toFloat()
    }

    return FingerprintMatchingConfig(
        matchThreshold = requiredFraction("match_threshold"),
        durationTolerance = requiredFraction("duration_tolerance"),
    )
}

internal fun flattenFingerprintDatabase(
    discRaw: String,
    albumRaw: String,
): String {
    val physicalDiscs = JSONObject(Jsonc.strip(discRaw)).getJSONArray("discs")
    val albums = JSONObject(Jsonc.strip(albumRaw)).getJSONArray("albums")
    val flattened = JSONArray()

    fun append(records: JSONArray) {
        for (i in 0 until records.length()) {
            val disc = records.getJSONObject(i)
            val discId = disc.getString("id")
            val tracks = disc.getJSONArray("tracks")
            for (j in 0 until tracks.length()) {
                val track = tracks.getJSONObject(j)
                if (track.getString("type") != "audio") continue
                val chromaprint = track.optString("chromaprint").takeIf { it.isNotEmpty() } ?: continue
                val durationMs = track.optInt("duration_ms", 0)
                if (durationMs <= 0) continue
                val entry = JSONObject()
                entry.put("name", fingerprintTrackName(track) ?: continue)
                entry.put("disc_id", discId)
                entry.put("track", track.getInt("track"))
                entry.put("duration_ms", durationMs)
                entry.put("chromaprint", chromaprint)
                flattened.put(entry)
            }
        }
    }

    append(physicalDiscs)
    append(albums)
    return flattened.toString()
}

/**
 * JNI bridge for Chromaprint audio fingerprinting.
 *
 * Wraps the native fingerprint DB, generation, and matching functions
 * exposed by jni_fingerprint.c.  Used during disc import to identify
 * audio tracks automatically.
 */
object FingerprintBridge {
    private const val TAG = "DXX-Fingerprint"

    init {
        System.loadLibrary("dxx-redux-d2")
    }

    private var dbLoaded = false

    data class FingerprintResult(
        val encoded: String,
        val durationMs: Int,
    )

    data class MatchResult(
        val confidence: Float,
        val name: String,
        val discId: String,
        val trackNum: Int,
    )

    // ── Database ──────────────────────────────────────────────────

    /**
     * Load the fingerprint DB from physical discs and fingerprint albums.
     * Flattens all audio tracks with Chromaprint data into the JSON array
     * format expected by chromaprint_db_load().
     * Returns number of entries loaded.
     */
    @Synchronized
    fun ensureDbLoaded(context: Context): Int {
        if (dbLoaded) return nativeGetDbCount()
        loadFingerprintConfig(context)
        val json = flattenFingerprintDb(context) ?: return 0
        val expected = JSONArray(json).length()
        val count = nativeLoadFingerprintDb(json)
        dbLoaded = expected > 0 && count == expected
        if (!dbLoaded) {
            Log.e(TAG, "Fingerprint DB load incomplete: expected $expected, loaded $count")
            return 0
        }
        Log.i(TAG, "Fingerprint DB: $count entries loaded")
        return count
    }

    fun databaseIdentity(context: Context): String =
        MessageDigest
            .getInstance("SHA-256")
            .also { digest ->
                listOf("known_discs.jsonc", "known_albums.jsonc", "fingerprint_config.jsonc").forEach { assetName ->
                    digest.update(assetName.toByteArray(Charsets.UTF_8))
                    runCatching {
                        context.assets.open(assetName).use { input ->
                            val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
                            while (true) {
                                val read = input.read(buffer)
                                if (read < 0) break
                                digest.update(buffer, 0, read)
                            }
                        }
                    }.onFailure {
                        digest.update("missing".toByteArray(Charsets.UTF_8))
                    }
                }
            }.digest()
            .joinToString("") { "%02x".format(it) }

    /**
     * Load match_threshold and duration_tolerance from fingerprint_config.jsonc
     * and push them to the native DB matcher.
     */
    private fun loadFingerprintConfig(context: Context) {
        val raw =
            context.assets
                .open("fingerprint_config.jsonc")
                .bufferedReader()
                .use { it.readText() }
        val config = parseFingerprintMatchingConfig(raw)
        check(nativeSetMatchThreshold(config.matchThreshold)) {
            "Native matcher rejected match_threshold"
        }
        nativeSetDurationTolerance(config.durationTolerance)
        Log.i(
            TAG,
            "Config: threshold=${config.matchThreshold} tolerance=${config.durationTolerance}",
        )
    }

    /**
     * Look up track names for a known disc by disc ID, directly from
     * known_discs.jsonc without fingerprinting.
     * Returns map of 1-based track number -> name, or empty map.
     */
    fun lookupTrackNames(
        context: Context,
        discId: String,
    ): Map<Int, String> {
        val names = mutableMapOf<Int, String>()
        try {
            val raw =
                context.assets
                    .open("known_discs.jsonc")
                    .bufferedReader()
                    .readText()
            val root = JSONObject(Jsonc.strip(raw))
            val discs = root.getJSONArray("discs")
            for (i in 0 until discs.length()) {
                val disc = discs.getJSONObject(i)
                if (disc.getString("id") != discId) continue
                val tracks = disc.getJSONArray("tracks")
                for (j in 0 until tracks.length()) {
                    val t = tracks.getJSONObject(j)
                    fingerprintTrackName(t)?.let { names[t.getInt("track")] = it }
                }
                break
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to look up track names for $discId", e)
        }
        return names
    }

    // ── Fingerprinting ────────────────────────────────────────────

    /**
     * Fingerprint a CD-DA audio track from a BIN file.
     * Opens the file by path, passes the fd to native code.
     */
    fun fingerprintDiscTrack(
        binPath: String,
        startSector: Int,
        numSectors: Int,
    ): FingerprintResult? {
        val pfd =
            ParcelFileDescriptor.open(
                File(binPath),
                ParcelFileDescriptor.MODE_READ_ONLY,
            )
        return try {
            val raw = nativeFingerprintDiscTrack(pfd.fd, startSector, numSectors) ?: return null
            parseFingerprintResult(raw)
        } finally {
            pfd.close()
        }
    }

    /**
     * Fingerprint a CD-DA audio track using an already-open file descriptor.
     * Used for SAF-referenced BIN files where File() access may not work.
     */
    fun fingerprintDiscTrackFd(
        fd: Int,
        startSector: Int,
        numSectors: Int,
    ): FingerprintResult? {
        val raw = nativeFingerprintDiscTrack(fd, startSector, numSectors) ?: return null
        return parseFingerprintResult(raw)
    }

    /**
     * Fingerprint an audio file (MP3/OGG/FLAC).
     */
    fun fingerprintAudioFile(path: String): FingerprintResult? {
        val raw = nativeFingerprintAudioFile(path) ?: return null
        return parseFingerprintResult(raw)
    }

    /**
     * Match a fingerprint against the loaded database.
     */
    fun matchFingerprint(
        encoded: String,
        durationMs: Int,
    ): MatchResult? = nativeMatchFingerprint(encoded, durationMs)

    /**
     * Fingerprint an audio file and match in one call.
     */
    fun fingerprintAndMatch(path: String): MatchResult? = nativeFingerprintAndMatch(path)

    /**
     * Fingerprint an audio file from a SAF content URI and match.
     * Opens a ParcelFileDescriptor and uses /proc/self/fd for native access.
     */
    fun fingerprintAndMatch(
        resolver: ContentResolver,
        uri: Uri,
    ): MatchResult? {
        val pfd = resolver.openFileDescriptor(uri, "r") ?: return null
        return try {
            val fdPath = "/proc/self/fd/${pfd.fd}"
            nativeFingerprintAndMatch(fdPath)
        } finally {
            pfd.close()
        }
    }

    /**
     * Fingerprint all audio tracks in a disc and match them against the DB.
     * Returns map of 1-based track number -> matched name.
     */
    fun fingerprintAndMatchDisc(
        context: Context,
        binPath: String,
        tracks: List<DiscImportBridge.CueTrack>,
        onProgress: ((current: Int, total: Int) -> Unit)? = null,
    ): Map<Int, String> {
        ensureDbLoaded(context)
        val names = mutableMapOf<Int, String>()
        val audioTracks = tracks.filter { it.isAudio }
        audioTracks.forEachIndexed { idx, track ->
            onProgress?.invoke(idx + 1, audioTracks.size)
            val fp = fingerprintDiscTrack(binPath, track.startSector, track.numSectors)
            if (fp != null) {
                val match = matchFingerprint(fp.encoded, fp.durationMs)
                if (match != null) {
                    names[track.trackNum] = match.name
                    Log.i(TAG, "Track ${track.trackNum}: ${match.name} (${match.confidence})")
                }
            }
        }
        return names
    }

    /**
     * Fingerprint all audio tracks in a multi-file local disc image.
     * Opens each referenced image once and picks the fd referenced by track.fileIndex.
     */
    fun fingerprintAndMatchDisc(
        context: Context,
        binPaths: List<String>,
        tracks: List<DiscImportBridge.CueTrack>,
        onProgress: ((current: Int, total: Int) -> Unit)? = null,
    ): Map<Int, String> {
        ensureDbLoaded(context)
        if (binPaths.isEmpty()) return emptyMap()
        val pfds = mutableListOf<ParcelFileDescriptor>()
        return try {
            binPaths.forEach { binPath ->
                val pfd = ParcelFileDescriptor.open(File(binPath), ParcelFileDescriptor.MODE_READ_ONLY)
                pfds.add(pfd)
            }
            val names = mutableMapOf<Int, String>()
            val audioTracks = tracks.filter { it.isAudio }
            audioTracks.forEachIndexed { idx, track ->
                onProgress?.invoke(idx + 1, audioTracks.size)
                val pfd = pfds.getOrNull(track.fileIndex)
                if (pfd == null) {
                    Log.w(TAG, "Track ${track.trackNum}: missing local image for fileIndex=${track.fileIndex}")
                    return@forEachIndexed
                }
                val fp = fingerprintDiscTrackFd(pfd.fd, track.startSector, track.numSectors)
                if (fp != null) {
                    val match = matchFingerprint(fp.encoded, fp.durationMs)
                    if (match != null) {
                        names[track.trackNum] = match.name
                        Log.i(TAG, "Track ${track.trackNum}: ${match.name} (${match.confidence})")
                    }
                }
            }
            names
        } finally {
            pfds.forEach { pfd ->
                pfd.close()
            }
        }
    }

    /**
     * Fingerprint all audio tracks in a disc using a SAF content URI.
     * Opens the BIN file via ContentResolver and uses the fd for each track.
     */
    fun fingerprintAndMatchDisc(
        context: Context,
        resolver: ContentResolver,
        binUri: Uri,
        tracks: List<DiscImportBridge.CueTrack>,
        onProgress: ((current: Int, total: Int) -> Unit)? = null,
    ): Map<Int, String> {
        ensureDbLoaded(context)
        val pfd = resolver.openFileDescriptor(binUri, "r") ?: return emptyMap()
        return try {
            val names = mutableMapOf<Int, String>()
            val audioTracks = tracks.filter { it.isAudio }
            audioTracks.forEachIndexed { idx, track ->
                onProgress?.invoke(idx + 1, audioTracks.size)
                val fp = fingerprintDiscTrackFd(pfd.fd, track.startSector, track.numSectors)
                if (fp != null) {
                    val match = matchFingerprint(fp.encoded, fp.durationMs)
                    if (match != null) {
                        names[track.trackNum] = match.name
                        Log.i(TAG, "Track ${track.trackNum}: ${match.name} (${match.confidence})")
                    }
                }
            }
            names
        } finally {
            pfd.close()
        }
    }

    /**
     * Fingerprint all audio tracks in a multi-BIN disc using SAF content URIs.
     * Opens each BIN once and picks the fd referenced by track.fileIndex.
     */
    fun fingerprintAndMatchDisc(
        context: Context,
        resolver: ContentResolver,
        binUris: List<Uri>,
        tracks: List<DiscImportBridge.CueTrack>,
        onProgress: ((current: Int, total: Int) -> Unit)? = null,
    ): Map<Int, String> {
        ensureDbLoaded(context)
        if (binUris.isEmpty()) return emptyMap()
        val pfds = mutableListOf<ParcelFileDescriptor>()
        return try {
            binUris.forEach { uri ->
                val pfd = resolver.openFileDescriptor(uri, "r") ?: return emptyMap()
                pfds.add(pfd)
            }
            val names = mutableMapOf<Int, String>()
            val audioTracks = tracks.filter { it.isAudio }
            audioTracks.forEachIndexed { idx, track ->
                onProgress?.invoke(idx + 1, audioTracks.size)
                val pfd = pfds.getOrNull(track.fileIndex)
                if (pfd == null) {
                    Log.w(TAG, "Track ${track.trackNum}: missing BIN for fileIndex=${track.fileIndex}")
                    return@forEachIndexed
                }
                val fp = fingerprintDiscTrackFd(pfd.fd, track.startSector, track.numSectors)
                if (fp != null) {
                    val match = matchFingerprint(fp.encoded, fp.durationMs)
                    if (match != null) {
                        names[track.trackNum] = match.name
                        Log.i(TAG, "Track ${track.trackNum}: ${match.name} (${match.confidence})")
                    }
                }
            }
            names
        } finally {
            pfds.forEach { pfd ->
                pfd.close()
            }
        }
    }

    // ── Helpers ───────────────────────────────────────────────────

    private fun parseFingerprintResult(raw: String): FingerprintResult? {
        val parts = raw.split("|", limit = 2)
        if (parts.size != 2) return null
        return FingerprintResult(parts[0], parts[1].toIntOrNull() ?: return null)
    }

    /**
     * Flatten physical-disc and album audio fingerprints into the native DB schema.
     */
    private fun flattenFingerprintDb(context: Context): String? =
        try {
            val discRaw =
                context.assets
                    .open("known_discs.jsonc")
                    .bufferedReader()
                    .use { it.readText() }
            val albumRaw =
                context.assets
                    .open("known_albums.jsonc")
                    .bufferedReader()
                    .use { it.readText() }
            flattenFingerprintDatabase(discRaw, albumRaw)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to flatten fingerprint DB", e)
            null
        }

    // ── JNI declarations ──────────────────────────────────────────

    private external fun nativeSetMatchThreshold(threshold: Float): Boolean

    private external fun nativeSetDurationTolerance(tolerance: Float)

    private external fun nativeLoadFingerprintDb(jsonData: String): Int

    private external fun nativeFingerprintAudioFile(filePath: String): String?

    private external fun nativeFingerprintDiscTrack(
        binFd: Int,
        startSector: Int,
        numSectors: Int,
    ): String?

    private external fun nativeMatchFingerprint(
        encodedFp: String,
        durationMs: Int,
    ): MatchResult?

    private external fun nativeFingerprintAndMatch(filePath: String): MatchResult?

    private external fun nativeGetDbCount(): Int
}
