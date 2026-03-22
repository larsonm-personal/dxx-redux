package com.dxxredux.app

import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Manages custom audio file sets (MP3/OGG/FLAC) for jukebox music playback.
 *
 * Each "set" is a named group of audio files that were imported together.
 * Sets are persisted in `custom_audio_sets.json` in the app's files directory.
 * Before game launch, [writeM3U] generates an M3U playlist from all enabled
 * sets for the C engine's jukebox to read.
 */
class CustomAudioSetManager(
    private val filesDir: File,
) {
    companion object {
        private const val TAG = "DXX-CustomAudio"
        private const val SETS_FILE = "custom_audio_sets.json"
        const val MUSIC_DIR = "custom_music"
        const val PLAYLIST_FILE = "custom_music.m3u"
    }

    data class AudioSet(
        val id: String,
        val label: String,
        // filenames relative to filesDir/custom_music/<id>/
        val files: List<String>,
        val enabled: Boolean = true,
        val order: Int = 0,
    )

    private var sets: MutableList<AudioSet> = mutableListOf()

    init {
        load()
    }

    fun getSets(): List<AudioSet> = sets.sortedBy { it.order }

    fun getEnabledSets(): List<AudioSet> = sets.filter { it.enabled }.sortedBy { it.order }

    fun addSet(set: AudioSet) {
        sets.removeAll { it.id == set.id }
        sets.add(set)
        save()
        Log.i(TAG, "Added set: ${set.label} (${set.files.size} files)")
    }

    fun removeSet(
        id: String,
        deleteFiles: Boolean = false,
    ) {
        if (deleteFiles) {
            val dir = File(File(filesDir, MUSIC_DIR), id)
            if (dir.exists()) dir.deleteRecursively()
        }
        sets.removeAll { it.id == id }
        save()
    }

    fun setEnabled(
        id: String,
        enabled: Boolean,
    ) {
        sets.replaceAll {
            if (it.id == id) it.copy(enabled = enabled) else it
        }
        save()
    }

    fun reorder(orderedIds: List<String>) {
        orderedIds.forEachIndexed { index, id ->
            sets.replaceAll {
                if (it.id == id) it.copy(order = index) else it
            }
        }
        save()
    }

    /** Directory where a set's files are stored */
    fun setDir(id: String): File = File(File(filesDir, MUSIC_DIR), id)

    /**
     * Write an M3U playlist from all enabled sets' files, sorted alphabetically.
     * Returns the playlist path relative to filesDir, or null if no files.
     */
    fun writeM3U(): String? {
        val enabled = getEnabledSets()
        val allFiles = mutableListOf<Pair<String, String>>() // (sortKey, relativePath)
        for (set in enabled) {
            val dir = setDir(set.id)
            for (f in set.files.sorted()) {
                val path = File(dir, f)
                if (path.exists()) {
                    allFiles.add(f.lowercase() to path.absolutePath)
                }
            }
        }
        if (allFiles.isEmpty()) {
            File(filesDir, PLAYLIST_FILE).delete()
            return null
        }
        allFiles.sortBy { it.first }
        val m3u = StringBuilder("# Custom audio playlist\n")
        for ((_, path) in allFiles) {
            m3u.appendLine(path)
        }
        File(filesDir, PLAYLIST_FILE).writeText(m3u.toString())
        Log.i(TAG, "Wrote $PLAYLIST_FILE with ${allFiles.size} tracks")
        return File(filesDir, PLAYLIST_FILE).absolutePath
    }

    /** Get the merged track list for preview (filename, set label) */
    fun getMergedTrackList(): List<Pair<String, String>> {
        val enabled = getEnabledSets()
        val tracks = mutableListOf<Pair<String, String>>()
        for (set in enabled) {
            for (f in set.files.sorted()) {
                tracks.add(f to set.label)
            }
        }
        tracks.sortBy { it.first.lowercase() }
        return tracks
    }

    // ── Persistence ───────────────────────────────────────────────

    private fun load() {
        val file = File(filesDir, SETS_FILE)
        if (!file.exists()) {
            sets = mutableListOf()
            return
        }
        try {
            val json = JSONObject(file.readText())
            val arr = json.getJSONArray("sets")
            sets =
                (0 until arr.length())
                    .map { i ->
                        val obj = arr.getJSONObject(i)
                        val filesArr = obj.getJSONArray("files")
                        AudioSet(
                            id = obj.getString("id"),
                            label = obj.optString("label", "Unnamed"),
                            files = (0 until filesArr.length()).map { filesArr.getString(it) },
                            enabled = obj.optBoolean("enabled", true),
                            order = obj.optInt("order", 0),
                        )
                    }.toMutableList()
            Log.i(TAG, "Loaded ${sets.size} custom audio sets")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load $SETS_FILE", e)
            sets = mutableListOf()
        }
    }

    private fun save() {
        val json = JSONObject()
        val arr = JSONArray()
        for (set in sets) {
            val obj = JSONObject()
            obj.put("id", set.id)
            obj.put("label", set.label)
            val filesArr = JSONArray()
            set.files.forEach { filesArr.put(it) }
            obj.put("files", filesArr)
            obj.put("enabled", set.enabled)
            obj.put("order", set.order)
            arr.put(obj)
        }
        json.put("sets", arr)
        File(filesDir, SETS_FILE).writeText(json.toString(2))
    }
}
