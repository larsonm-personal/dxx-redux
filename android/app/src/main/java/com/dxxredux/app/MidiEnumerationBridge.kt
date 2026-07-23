package com.dxxredux.app

import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json

/**
 * JNI bridge for enumerating MIDI/HMP tracks from game HOG files
 * and mission directories.
 */
object MidiEnumerationBridge {
    // HMP conversion and TML parsing share legacy native state and must not overlap.
    internal val nativeDataLock = Any()

    init {
        System.loadLibrary("dxx-redux-d2")
    }

    @Serializable
    data class TrackInfo(
        val filename: String,
        val duration_ms: Int = -1,
    )

    @Serializable
    data class SourceInfo(
        val id: String,
        val label: String,
        val game: String,
        val hog: String = "",
        val tracks: List<TrackInfo> = emptyList(),
    )

    @Serializable
    data class EnumerationResult(
        val sources: List<SourceInfo> = emptyList(),
    )

    private val json = Json { ignoreUnknownKeys = true }

    fun enumerateTracks(filesDir: String): EnumerationResult {
        val raw = synchronized(nativeDataLock) { nativeEnumerateTracks(filesDir) }
        return try {
            json.decodeFromString<EnumerationResult>(raw)
        } catch (e: Exception) {
            android.util.Log.w("DXX-MidiEnum", "JSON parse failed: ${e.message}")
            EnumerationResult()
        }
    }

    // -- JNI declaration --

    @JvmStatic private external fun nativeEnumerateTracks(filesDir: String): String
}
