package com.dxxredux.app

import android.content.Context
import kotlinx.coroutines.CancellationException
import org.json.JSONObject

/** Native contract: mission_movie_scan.hpp reports references and availability */
internal data class MissionMovieScan(
    val status: String,
    val required: List<String>,
    val missing: List<String>,
    val unavailableLibraries: List<String>,
    val problems: List<String>,
) {
    val summary: String
        get() =
            when {
                missing.isNotEmpty() -> "Missing movies (${missing.size}): ${missing.joinToString(", ")}"
                status == "not_applicable" -> "This mission uses in-engine presentation rather than movie files"
                status != "ok" -> "Movie scan incomplete"
                required.isEmpty() -> "No movie references found"
                else -> "All ${required.size} referenced movies are available"
            }

    companion object {
        fun fromJson(json: JSONObject): MissionMovieScan {
            fun strings(key: String): List<String> {
                val values = json.optJSONArray(key) ?: return emptyList()
                return (0 until values.length()).map { values.getString(it) }
            }
            return MissionMovieScan(
                json.optString("status", "unavailable"),
                strings("required"),
                strings("missing"),
                strings("unavailable_libraries"),
                strings("problems"),
            )
        }
    }
}

internal suspend fun scanMissionMovies(
    context: Context,
    target: LevelMetadataTarget,
): MissionMovieScan =
    try {
        val result = LevelMetadataAnalyzer.analyze(context, target, moviesOnly = true)
        result.movies ?: MissionMovieScan(
            "unavailable",
            emptyList(),
            emptyList(),
            emptyList(),
            result.problems.ifEmpty { listOf("Movie scan unavailable") },
        )
    } catch (cancelled: CancellationException) {
        throw cancelled
    } catch (failure: Exception) {
        MissionMovieScan(
            "unavailable",
            emptyList(),
            emptyList(),
            emptyList(),
            listOf(failure.message ?: "Movie scan unavailable"),
        )
    }
