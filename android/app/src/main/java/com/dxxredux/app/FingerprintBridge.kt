package com.dxxredux.app

import android.content.Context
import android.os.ParcelFileDescriptor
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

// Filter AcoustID placeholder names where both artist and title are unknown
private fun isPlaceholderName(name: String): Boolean = name == "[unknown] - [untitled]"

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
     * Load the fingerprint DB from known_discs.json5 bundled in assets.
     * Flattens all audio tracks with chromaprint data into the JSON array
     * format expected by chromaprint_db_load().
     * Returns number of entries loaded.
     */
    fun ensureDbLoaded(context: Context): Int {
        if (dbLoaded) return nativeGetDbCount()
        loadFingerprintConfig(context)
        val json = flattenFingerprintDb(context) ?: return 0
        val count = nativeLoadFingerprintDb(json)
        dbLoaded = count > 0
        Log.i(TAG, "Fingerprint DB: $count entries loaded")
        return count
    }

    /**
     * Load match_threshold and duration_tolerance from fingerprint_config.json5
     * and push them to the native DB matcher.
     */
    private fun loadFingerprintConfig(context: Context) {
        try {
            val raw =
                context.assets
                    .open("fingerprint_config.json5")
                    .bufferedReader()
                    .readText()
            val cfg = JSONObject(Json5.strip(raw))
            val threshold = cfg.optDouble("match_threshold", 0.4).toFloat()
            val tolerance = cfg.optDouble("duration_tolerance", 0.10).toFloat()
            nativeSetMatchThreshold(threshold)
            nativeSetDurationTolerance(tolerance)
            Log.i(TAG, "Config: threshold=$threshold tolerance=$tolerance")
        } catch (e: Exception) {
            Log.w(TAG, "Failed to load fingerprint_config.json5, using defaults", e)
        }
    }

    /**
     * Look up track names for a known disc by disc ID, directly from
     * known_discs.json5 without fingerprinting.
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
                    .open("known_discs.json5")
                    .bufferedReader()
                    .readText()
            val root = JSONObject(Json5.strip(raw))
            val discs = root.getJSONArray("discs")
            for (i in 0 until discs.length()) {
                val disc = discs.getJSONObject(i)
                if (disc.getString("id") != discId) continue
                val tracks = disc.getJSONArray("tracks")
                for (j in 0 until tracks.length()) {
                    val t = tracks.getJSONObject(j)
                    val acoustidName = t.optString("acoustid_name").takeIf { it.isNotEmpty() }
                    val name =
                        acoustidName
                            ?: t.optString("name").takeIf { it.isNotEmpty() }
                            ?: continue
                    if (!isPlaceholderName(name)) {
                        names[t.getInt("track")] = name
                    }
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
    ): MatchResult? {
        val raw = nativeMatchFingerprint(encoded, durationMs) ?: return null
        return parseMatchResult(raw)
    }

    /**
     * Fingerprint an audio file and match in one call.
     */
    fun fingerprintAndMatch(path: String): MatchResult? {
        val raw = nativeFingerprintAndMatch(path) ?: return null
        return parseMatchResult(raw)
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

    // ── Helpers ───────────────────────────────────────────────────

    private fun parseFingerprintResult(raw: String): FingerprintResult? {
        val parts = raw.split("|", limit = 2)
        if (parts.size != 2) return null
        return FingerprintResult(parts[0], parts[1].toIntOrNull() ?: return null)
    }

    private fun parseMatchResult(raw: String): MatchResult? {
        val parts = raw.split("|", limit = 4)
        if (parts.size != 4) return null
        return MatchResult(
            confidence = parts[0].toFloatOrNull() ?: return null,
            name = parts[1],
            discId = parts[2],
            trackNum = parts[3].toIntOrNull() ?: return null,
        )
    }

    /**
     * Flatten known_discs.json5 audio tracks with chromaprint into a
     * JSON array string for chromaprint_db_load().
     */
    private fun flattenFingerprintDb(context: Context): String? =
        try {
            val raw =
                context.assets
                    .open("known_discs.json5")
                    .bufferedReader()
                    .readText()
            val root = JSONObject(Json5.strip(raw))
            val discs = root.getJSONArray("discs")
            val arr = JSONArray()
            for (i in 0 until discs.length()) {
                val disc = discs.getJSONObject(i)
                val discId = disc.getString("id")
                val tracks = disc.getJSONArray("tracks")
                for (j in 0 until tracks.length()) {
                    val t = tracks.getJSONObject(j)
                    if (t.getString("type") != "audio") continue
                    val chromaprint = t.optString("chromaprint").takeIf { it.isNotEmpty() } ?: continue
                    val durationMs = t.optInt("duration_ms", 0)
                    if (durationMs <= 0) continue
                    val entry = JSONObject()
                    val acoustidName = t.optString("acoustid_name").takeIf { it.isNotEmpty() }
                    val rawName =
                        acoustidName
                            ?: t.optString("name", "Track ${t.getInt("track")}")
                    val trackName =
                        if (isPlaceholderName(rawName)) "Track ${t.getInt("track")}" else rawName
                    entry.put("name", trackName)
                    entry.put("disc_id", discId)
                    entry.put("track", t.getInt("track"))
                    entry.put("duration_ms", durationMs)
                    entry.put("chromaprint", chromaprint)
                    arr.put(entry)
                }
            }
            arr.toString()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to flatten fingerprint DB", e)
            null
        }

    // ── JNI declarations ──────────────────────────────────────────

    private external fun nativeSetMatchThreshold(threshold: Float)

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
    ): String?

    private external fun nativeFingerprintAndMatch(filePath: String): String?

    private external fun nativeGetDbCount(): Int

    private external fun nativeFreeDb()
}
