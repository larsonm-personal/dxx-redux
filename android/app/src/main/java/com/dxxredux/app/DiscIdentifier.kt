package com.dxxredux.app

import android.content.Context
import org.json.JSONObject
import java.io.InputStream
import java.security.MessageDigest

/**
 * Identifies BIN/CUE disc images by matching per-track SHA1 hashes against
 * the known_discs.json5 database bundled in APK assets.
 *
 * Matching algorithm: for each known disc, compare SHA1s in order starting
 * from track 1. The disc with the most consecutive in-order matches from
 * the beginning wins. Ties broken by total match count.
 */
class DiscIdentifier(
    context: Context,
) {
    data class KnownTrack(
        val track: Int,
        val type: String, // "data" or "audio"
        val sha1: String,
        val name: String?,
    )

    data class KnownDisc(
        val id: String,
        val label: String,
        val game: String, // "d1" or "d2"
        val legacyDiscId: String?, // e.g. "0x7d0ff809"
        val trackMapping: Map<String, Int>, // title, credits, first_level
        val tracks: List<KnownTrack>,
    )

    data class MatchResult(
        val disc: KnownDisc?,
        // consecutive from beginning
        val matchedTracks: Int,
        val totalKnownTracks: Int,
        val allMatched: Boolean,
        // total matching tracks (not necessarily consecutive)
        val totalMatchCount: Int,
    ) {
        val matched get() = disc != null
        val label get() = disc?.label ?: "Unknown disc"
        val matchDescription: String get() =
            when {
                disc == null -> "No match"
                allMatched -> "all track hashes matched"
                else -> "$matchedTracks/$totalKnownTracks track hashes matched"
            }
    }

    private val knownDiscs: List<KnownDisc>

    init {
        knownDiscs = loadDatabase(context)
    }

    private fun loadDatabase(context: Context): List<KnownDisc> =
        try {
            val raw =
                context.assets
                    .open("known_discs.json5")
                    .bufferedReader()
                    .readText()
            val root = JSONObject(Json5.strip(raw))
            val discsArray = root.getJSONArray("discs")
            (0 until discsArray.length()).map { i ->
                val d = discsArray.getJSONObject(i)
                val tracksArray = d.getJSONArray("tracks")
                val tracks =
                    (0 until tracksArray.length()).map { j ->
                        val t = tracksArray.getJSONObject(j)
                        KnownTrack(
                            track = t.getInt("track"),
                            type = t.getString("type"),
                            sha1 = t.getString("sha1"),
                            name = t.optString("name").takeIf { it.isNotEmpty() },
                        )
                    }
                val mapping = mutableMapOf<String, Int>()
                d.optJSONObject("track_mapping")?.let { tm ->
                    tm.keys().forEach { key -> mapping[key] = tm.getInt(key) }
                }
                KnownDisc(
                    id = d.getString("id"),
                    label = d.getString("label"),
                    game = d.getString("game"),
                    legacyDiscId = d.optString("legacy_disc_id").takeIf { it.isNotEmpty() },
                    trackMapping = mapping,
                    tracks = tracks,
                )
            }
        } catch (e: Exception) {
            android.util.Log.e("DiscIdentifier", "Failed to load known_discs.json5: ${e.message}")
            emptyList()
        }

    /**
     * Identify a disc from its per-track SHA1 hashes.
     * @param trackSha1s map of 1-based track number → SHA1 hex string
     */
    fun identify(trackSha1s: Map<Int, String>): MatchResult {
        var bestDisc: KnownDisc? = null
        var bestConsecutive = 0
        var bestTotal = 0
        var bestKnownCount = 0

        for (disc in knownDiscs) {
            var consecutive = 0
            var total = 0
            var broken = false

            for (kt in disc.tracks.sortedBy { it.track }) {
                val actual = trackSha1s[kt.track]
                if (actual != null && actual.equals(kt.sha1, ignoreCase = true)) {
                    total++
                    if (!broken) consecutive++
                } else {
                    broken = true
                }
            }

            // Better match: more consecutive from beginning, then total as tiebreaker
            if (consecutive > bestConsecutive ||
                (consecutive == bestConsecutive && total > bestTotal)
            ) {
                bestDisc = disc
                bestConsecutive = consecutive
                bestTotal = total
                bestKnownCount = disc.tracks.size
            }
        }

        return MatchResult(
            disc = bestDisc,
            matchedTracks = bestConsecutive,
            totalKnownTracks = bestKnownCount,
            allMatched = bestDisc != null && bestConsecutive == bestKnownCount,
            totalMatchCount = bestTotal,
        )
    }

    /**
     * Get all known discs (for UI listing).
     */
    fun allDiscs(): List<KnownDisc> = knownDiscs

    companion object {
        /**
         * Compute SHA1 hash of a section of raw data from an InputStream.
         * Used to hash individual tracks from BIN files.
         * @param input stream positioned at the start of the track data
         * @param length number of bytes to hash
         * @param progressCallback optional (bytesHashed, totalBytes) -> shouldCancel
         */
        fun sha1Hash(
            input: InputStream,
            length: Long,
            progressCallback: ((Long, Long) -> Boolean)? = null,
        ): String {
            val digest = MessageDigest.getInstance("SHA-1")
            val buf = ByteArray(65536)
            var remaining = length
            var hashed = 0L
            while (remaining > 0) {
                val toRead = minOf(buf.size.toLong(), remaining).toInt()
                val n = input.read(buf, 0, toRead)
                if (n <= 0) break
                digest.update(buf, 0, n)
                remaining -= n
                hashed += n
                if (progressCallback?.invoke(hashed, length) == true) break
            }
            return digest.digest().joinToString("") { "%02x".format(it) }
        }
    }
}
