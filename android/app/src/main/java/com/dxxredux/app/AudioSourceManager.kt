package com.dxxredux.app

import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Manages multiple BIN/CUE audio sources for Redbook CD audio playback.
 *
 * Sources are persisted in `audio_sources.json` in the app's files directory.
 * Before game launch, [writePlaylist] writes `audio_playlist.json` which the
 * C engine reads in `RBAInit()` to build its combined track table.
 *
 * For the legacy single-source case (GOG descent_ii.gog/.inst), no
 * audio_sources.json is needed — the engine falls back to its hardcoded path.
 */
class AudioSourceManager(
    private val filesDir: File,
) {
    companion object {
        private const val TAG = "DXX-AudioSrc"
        private const val SOURCES_FILE = "audio_sources.json"
        private const val PLAYLIST_FILE = "audio_playlist.json"
    }

    /**
     * One registered audio source (a BIN/CUE pair from a disc image).
     */
    data class AudioSource(
        // unique id (e.g., "d2-gog-v1.2")
        val id: String,
        // path to CUE file (relative to filesDir)
        val cuePath: String,
        // paths to BIN file(s)
        val binPaths: List<String>,
        // human-readable disc label
        val discLabel: String,
        // known_discs.json disc id (or "unknown")
        val discId: String,
        // total tracks (data + audio)
        val trackCount: Int,
        // audio tracks only
        val audioTrackCount: Int,
        // e.g., 0x7d0ff809 for backward compat
        val legacyDiscId: Long,
        val enabled: Boolean = true,
        // user-defined sort order
        val order: Int = 0,
    )

    private var sources: MutableList<AudioSource> = mutableListOf()

    init {
        load()
    }

    /** All registered audio sources, sorted by user order */
    fun getSources(): List<AudioSource> = sources.sortedBy { it.order }

    /** Get only enabled sources, in order */
    fun getEnabledSources(): List<AudioSource> = sources.filter { it.enabled }.sortedBy { it.order }

    /** Check if the legacy GOG pair exists (no explicit source registration needed) */
    fun hasLegacyGog(setDir: File? = null): Boolean {
        // Extracted filenames may be UPPERCASE; Android FS is case-sensitive
        fun dirHasGogPair(dir: File): Boolean {
            val files = dir.list() ?: return false
            var hasGog = false
            var hasInst = false
            for (f in files) {
                val lower = f.lowercase()
                if (lower == "descent_ii.gog") hasGog = true
                if (lower == "descent_ii.inst") hasInst = true
                if (hasGog && hasInst) return true
            }
            return false
        }
        if (setDir != null && dirHasGogPair(setDir)) return true
        return dirHasGogPair(filesDir)
    }

    /** Add a new audio source */
    fun addSource(source: AudioSource) {
        // Remove any existing source with the same id
        sources.removeAll { it.id == source.id }
        sources.add(source)
        save()
        Log.i(TAG, "Added source: ${source.discLabel} (${source.audioTrackCount} audio tracks)")
    }

    /** Remove an audio source by id */
    fun removeSource(id: String) {
        sources.removeAll { it.id == id }
        save()
    }

    /**
     * Remove sources whose BIN/CUE files no longer exist on disk.
     * Checks both filesDir and optional setDir.
     * Returns list of pruned source labels for user notification.
     */
    fun pruneMissingSources(setDir: File? = null): List<String> {
        val pruned = mutableListOf<String>()
        val toRemove =
            sources.filter { src ->
                val allFiles = src.binPaths + src.cuePath
                allFiles.any { name ->
                    val inRoot = File(filesDir, name).exists()
                    val inSet = setDir != null && File(setDir, name).exists()
                    !inRoot && !inSet
                }
            }
        for (src in toRemove) {
            pruned.add(src.discLabel)
            Log.i(TAG, "Pruning stale source: ${src.discLabel} (${src.id})")
        }
        if (toRemove.isNotEmpty()) {
            sources.removeAll(toRemove.toSet())
            save()
        }
        return pruned
    }

    /** Toggle enabled state */
    fun setEnabled(
        id: String,
        enabled: Boolean,
    ) {
        sources.replaceAll {
            if (it.id == id) it.copy(enabled = enabled) else it
        }
        save()
    }

    /** Update ordering */
    fun reorder(orderedIds: List<String>) {
        orderedIds.forEachIndexed { index, id ->
            sources.replaceAll {
                if (it.id == id) it.copy(order = index) else it
            }
        }
        save()
    }

    /**
     * Write audio_playlist.json for the C engine.
     *
     * Called before game launch so RBAInit() can read it.
     * Returns true if a playlist was written, false if legacy mode.
     */
    fun writePlaylist(): Boolean {
        val enabled = getEnabledSources()
        if (enabled.isEmpty()) {
            // No explicit sources — engine uses legacy descent_ii.gog/.inst
            File(filesDir, PLAYLIST_FILE).delete()
            return false
        }

        val json = JSONObject()
        val arr = JSONArray()
        for (src in enabled) {
            val entry = JSONObject()
            entry.put("cue", src.cuePath)
            val bins = JSONArray()
            src.binPaths.forEach { bins.put(it) }
            entry.put("bins", bins)
            entry.put("label", src.discLabel)
            entry.put("legacy_disc_id", src.legacyDiscId)
            arr.put(entry)
        }
        json.put("sources", arr)

        File(filesDir, PLAYLIST_FILE).writeText(json.toString(2))
        Log.i(TAG, "Wrote $PLAYLIST_FILE with ${enabled.size} sources")
        return true
    }

    // ── Persistence ───────────────────────────────────────────────

    private fun load() {
        val file = File(filesDir, SOURCES_FILE)
        if (!file.exists()) {
            sources = mutableListOf()
            return
        }
        try {
            val json = JSONObject(file.readText())
            val arr = json.getJSONArray("sources")
            sources =
                (0 until arr.length())
                    .map { i ->
                        val obj = arr.getJSONObject(i)
                        val binArr = obj.getJSONArray("bins")
                        AudioSource(
                            id = obj.getString("id"),
                            cuePath = obj.getString("cue"),
                            binPaths = (0 until binArr.length()).map { binArr.getString(it) },
                            discLabel = obj.optString("label", "Unknown Disc"),
                            discId = obj.optString("disc_id", "unknown"),
                            trackCount = obj.optInt("track_count", 0),
                            audioTrackCount = obj.optInt("audio_track_count", 0),
                            legacyDiscId = obj.optLong("legacy_disc_id", 0),
                            enabled = obj.optBoolean("enabled", true),
                            order = obj.optInt("order", 0),
                        )
                    }.toMutableList()
            Log.i(TAG, "Loaded ${sources.size} audio sources")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load $SOURCES_FILE", e)
            sources = mutableListOf()
        }
    }

    private fun save() {
        val json = JSONObject()
        val arr = JSONArray()
        for (src in sources) {
            val obj = JSONObject()
            obj.put("id", src.id)
            obj.put("cue", src.cuePath)
            val bins = JSONArray()
            src.binPaths.forEach { bins.put(it) }
            obj.put("bins", bins)
            obj.put("label", src.discLabel)
            obj.put("disc_id", src.discId)
            obj.put("track_count", src.trackCount)
            obj.put("audio_track_count", src.audioTrackCount)
            obj.put("legacy_disc_id", src.legacyDiscId)
            obj.put("enabled", src.enabled)
            obj.put("order", src.order)
            arr.put(obj)
        }
        json.put("sources", arr)
        File(filesDir, SOURCES_FILE).writeText(json.toString(2))
    }
}
