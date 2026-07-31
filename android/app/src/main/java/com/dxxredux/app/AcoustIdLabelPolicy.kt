package com.dxxredux.app

import org.json.JSONObject
import java.io.File
import java.util.Locale

internal data class AcoustIdLabelMatch(
    val name: String,
    val album: String?,
    val score: Double,
    val recordingId: String,
)

internal object AcoustIdLabelPolicy {
    // Keep this threshold and normalization contract aligned with fingerprint_music_packs.ps1.
    const val MIN_RESULT_SCORE = 0.8

    private fun normalizedWords(value: String): String {
        var key =
            value
                .lowercase(Locale.ROOT)
                .replace(Regex("[^\\p{L}\\p{N}]+"), " ")
                .trim()
                .replace(Regex("^\\d+\\s+"), "")
        key = key.replace(Regex("^(level|stage)\\s+0*\\d+\\s+"), "")
        key = key.replace(Regex("\\s+mn\\d+$"), "")
        return key.replace(Regex("\\s+(psx|macplay)\\s+(maximum\\s+)?(mix|remix)$"), "")
    }

    private fun sourceTitle(value: String): String {
        val basename = File(value).name.substringBeforeLast('.', File(value).name)
        return basename
    }

    private fun webTitle(value: String): String = value.substringAfter(" - ", value)

    fun labelsAgree(
        maintainedLabel: String,
        webLabel: String,
    ): Boolean = normalizedWords(sourceTitle(maintainedLabel)) == normalizedWords(webTitle(webLabel))

    fun select(
        response: JSONObject,
        maintainedLabel: String,
    ): AcoustIdLabelMatch? {
        val candidates = mutableListOf<AcoustIdLabelMatch>()
        val results = response.optJSONArray("results") ?: return null
        for (resultIndex in 0 until results.length()) {
            val result = results.optJSONObject(resultIndex) ?: continue
            val score = result.optDouble("score", -1.0)
            if (score < MIN_RESULT_SCORE) continue
            val recordings = result.optJSONArray("recordings") ?: continue
            for (recordingIndex in 0 until recordings.length()) {
                val recording = recordings.optJSONObject(recordingIndex) ?: continue
                val recordingId = recording.optString("id").takeIf { it.isNotBlank() } ?: continue
                val title = recording.optString("title").takeIf { it.isNotBlank() } ?: continue
                val artists = recording.optJSONArray("artists")
                val artist =
                    if (artists == null) {
                        ""
                    } else {
                        (0 until artists.length())
                            .mapNotNull { artists.optJSONObject(it)?.optString("name")?.takeIf(String::isNotBlank) }
                            .joinToString(", ")
                    }
                val name = if (artist.isBlank()) title else "$artist - $title"
                if (!labelsAgree(maintainedLabel, name)) continue
                val releases = recording.optJSONArray("releases")
                val album =
                    if (releases == null) {
                        null
                    } else {
                        (0 until releases.length())
                            .mapNotNull { releases.optJSONObject(it)?.optString("title")?.takeIf(String::isNotBlank) }
                            .sorted()
                            .firstOrNull()
                    }
                candidates.add(AcoustIdLabelMatch(name, album, score, recordingId))
            }
        }
        val bestScore = candidates.maxOfOrNull { it.score } ?: return null
        val best = candidates.filter { it.score == bestScore }
        if (best.map { normalizedWords(it.name) }.distinct().size != 1) return null
        return best.minBy { it.recordingId }
    }
}
