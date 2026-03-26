package com.dxxredux.app

import android.content.ContentResolver
import android.net.Uri
import android.os.ParcelFileDescriptor
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

        /** Open PFDs kept alive during game session for SAF sources */
        private val activePfds = mutableListOf<ParcelFileDescriptor>()

        /** Close all PFDs from a previous game session */
        fun closeActivePfds() {
            for (pfd in activePfds) {
                try {
                    pfd.close()
                } catch (_: Exception) {
                    // already closed
                }
            }
            activePfds.clear()
        }
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
        // fingerprint-matched track names: 1-based track number -> name
        val trackNames: Map<Int, String> = emptyMap(),
        // SAF content URI for BIN file (when referenced in-place, not copied)
        val binContentUri: String? = null,
        // SAF content URI for CUE file (when referenced in-place, not copied)
        val cueContentUri: String? = null,
    )

    private var sources: MutableList<AudioSource> = mutableListOf()

    init {
        load()
    }

    /** All registered audio sources, sorted by user order */
    fun getSources(): List<AudioSource> = sources.sortedBy { it.order }

    /** Get only enabled sources, in order */
    fun getEnabledSources(): List<AudioSource> = sources.filter { it.enabled }.sortedBy { it.order }

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
     * Checks paths relative to filesDir with case-insensitive matching
     * (GOG extraction may produce different case than what was stored).
     * Returns list of pruned source labels for user notification.
     */
    fun pruneMissingSources(setDir: File? = null): List<String> {
        val pruned = mutableListOf<String>()
        val toRemove =
            sources.filter { src ->
                // SAF sources don't have local files to check
                if (src.binContentUri != null) return@filter false
                val allFiles = src.binPaths + src.cuePath
                allFiles.any { name ->
                    val f = File(filesDir, name)
                    if (f.exists()) return@any false
                    // Case-insensitive fallback: check parent dir listing
                    val parent = f.parentFile
                    val target = f.name.lowercase()
                    val found = parent?.list()?.any { it.lowercase() == target } == true
                    !found
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
     * For SAF sources, opens file descriptors and writes /proc/self/fd paths.
     * Call [closeActivePfds] after the game exits to release them.
     * Returns true if a playlist was written, false if legacy mode.
     */
    fun writePlaylist(resolver: ContentResolver? = null): Boolean {
        closeActivePfds()
        val enabled = getEnabledSources()
        if (enabled.isEmpty()) {
            File(filesDir, PLAYLIST_FILE).delete()
            return false
        }

        val json = JSONObject()
        val arr = JSONArray()
        for (src in enabled) {
            val entry = JSONObject()
            entry.put("cue", src.cuePath)
            val bins = JSONArray()
            if (src.binContentUri != null && resolver != null) {
                // SAF source: open fd and use /proc/self/fd path
                try {
                    val pfd = resolver.openFileDescriptor(Uri.parse(src.binContentUri), "r")
                    if (pfd != null) {
                        activePfds.add(pfd)
                        bins.put("/proc/self/fd/${pfd.fd}")
                    } else {
                        Log.w(TAG, "Could not open SAF URI for ${src.discLabel}")
                        src.binPaths.forEach { bins.put(it) }
                    }
                } catch (e: Exception) {
                    Log.w(TAG, "SAF fd open failed for ${src.discLabel}", e)
                    src.binPaths.forEach { bins.put(it) }
                }
            } else {
                src.binPaths.forEach { bins.put(it) }
            }
            entry.put("bins", bins)
            entry.put("label", src.discLabel)
            entry.put("legacy_disc_id", src.legacyDiscId)
            if (src.trackNames.isNotEmpty()) {
                val tn = JSONObject()
                src.trackNames.forEach { (k, v) -> tn.put(k.toString(), v) }
                entry.put("track_names", tn)
            }
            arr.put(entry)
        }
        json.put("sources", arr)

        File(filesDir, PLAYLIST_FILE).writeText(json.toString(2))
        Log.i(TAG, "Wrote $PLAYLIST_FILE with ${enabled.size} sources (${activePfds.size} SAF fds)")
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
                            trackNames =
                                obj.optJSONObject("track_names")?.let { tn ->
                                    tn.keys().asSequence().associate { k -> k.toInt() to tn.getString(k) }
                                } ?: emptyMap(),
                            binContentUri = obj.optString("bin_content_uri", "").ifEmpty { null },
                            cueContentUri = obj.optString("cue_content_uri", "").ifEmpty { null },
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
            if (src.trackNames.isNotEmpty()) {
                val tn = JSONObject()
                src.trackNames.forEach { (k, v) -> tn.put(k.toString(), v) }
                obj.put("track_names", tn)
            }
            src.binContentUri?.let { obj.put("bin_content_uri", it) }
            src.cueContentUri?.let { obj.put("cue_content_uri", it) }
            arr.put(obj)
        }
        json.put("sources", arr)
        File(filesDir, SOURCES_FILE).writeText(json.toString(2))
    }
}
