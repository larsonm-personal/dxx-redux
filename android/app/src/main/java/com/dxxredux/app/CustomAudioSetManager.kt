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
        const val NAMES_FILE = "custom_music_names.json"
    }

    private fun logInfo(message: String) {
        runCatching { Log.i(TAG, message) }
    }

    private fun logWarn(message: String) {
        runCatching { Log.w(TAG, message) }
    }

    private fun logError(
        message: String,
        error: Throwable,
    ) {
        runCatching { Log.e(TAG, message, error) }
    }

    data class AudioSet(
        val id: String,
        val label: String,
        // filenames relative to filesDir/custom_music/<id>/
        val files: List<String>,
        val enabled: Boolean = true,
        val order: Int = 0,
        // fingerprint-matched track names: filename -> track name
        val trackNames: Map<String, String> = emptyMap(),
        // embedded Title (Composer) fallback names: filename -> display name
        val embeddedTrackNames: Map<String, String> = emptyMap(),
        // fingerprint match confidence: filename -> confidence (0.0-1.0)
        val trackConfidences: Map<String, Float> = emptyMap(),
        // fingerprint match CD track number: filename -> 1-based track number
        val trackNumbers: Map<String, Int> = emptyMap(),
        // referenced (not copied) files: filename -> SAF content URI string
        val referencedUris: Map<String, String> = emptyMap(),
    )

    private var sets: MutableList<AudioSet> = mutableListOf()

    init {
        load()
    }

    fun getSets(): List<AudioSet> = sets.sortedBy { it.order }

    fun getEnabledSets(): List<AudioSet> = sets.filter { it.enabled }.sortedBy { it.order }

    fun hasUsableTrack(canAccessUri: (String) -> Boolean = { false }): Boolean =
        getEnabledSets().any { set ->
            set.files.any { name ->
                val uri = set.referencedUris[name]
                if (uri != null) canAccessUri(uri) else File(setDir(set.id), name).isFile
            }
        }

    fun addSet(set: AudioSet) {
        sets.removeAll { it.id == set.id }
        sets.add(set)
        save()
        logInfo("Added set: ${set.label} (${set.files.size} files)")
    }

    /** Append files to an existing set */
    fun addFilesToSet(
        id: String,
        newFiles: List<String>,
        newRefs: Map<String, String> = emptyMap(),
        newTrackNames: Map<String, String> = emptyMap(),
        newEmbeddedTrackNames: Map<String, String> = emptyMap(),
        newConfidences: Map<String, Float> = emptyMap(),
        newTrackNumbers: Map<String, Int> = emptyMap(),
    ) {
        val existing = sets.firstOrNull { it.id == id } ?: return
        val merged =
            existing.copy(
                files = existing.files + newFiles,
                trackNames = existing.trackNames + newTrackNames,
                embeddedTrackNames = existing.embeddedTrackNames + newEmbeddedTrackNames,
                trackConfidences = existing.trackConfidences + newConfidences,
                trackNumbers = existing.trackNumbers + newTrackNumbers,
                referencedUris = existing.referencedUris + newRefs,
            )
        sets.replaceAll { if (it.id == id) merged else it }
        save()
        logInfo("Added ${newFiles.size} files to set '${existing.label}'")
    }

    fun removeSet(
        id: String,
        deleteFiles: Boolean = false,
    ) {
        val set = sets.firstOrNull { it.id == id }
        if (deleteFiles && set != null) {
            // Only delete the set directory (copied files). Referenced files stay
            val dir = File(File(filesDir, MUSIC_DIR), id)
            if (dir.exists()) dir.deleteRecursively()
        }
        sets.removeAll { it.id == id }
        save()
    }

    fun removeReferencedFile(
        id: String,
        filename: String,
    ) {
        val set = sets.firstOrNull { it.id == id } ?: return
        val updated = removeReferencedFileFromSet(set, filename)
        if (updated == null) {
            sets.removeAll { it.id == id }
            save()
            logInfo("Removed final referenced file '$filename' and deleted set '${set.label}'")
            return
        }
        sets.replaceAll { if (it.id == id) updated else it }
        save()
        logInfo("Removed referenced file '$filename' from set '${set.label}'")
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
     * Referenced (non-copied) files are staged to a temp directory so the C
     * engine can read them by filesystem path.
     * Returns the playlist path relative to filesDir, or null if no files.
     */
    fun writeM3U(
        context: android.content.Context? = null,
        onProgress: (String, Long, Long) -> Unit = { _, _, _ -> },
    ): String? =
        writeM3UWith(
            referenceStager =
                context?.let {
                    { uri, staged, filename ->
                        LauncherFileCopy.copyUriToFile(
                            context,
                            android.net.Uri.parse(uri),
                            staged,
                            filename,
                        ) { progress ->
                            onProgress(progress.label, progress.bytesDone, progress.bytesTotal)
                        }
                    }
                },
            embeddedNameReader = { file, extension ->
                AudioTagMetadataBridge.parsePath(file, extension)?.display_name?.takeIf { it.isNotBlank() }
            },
        )

    internal fun writeM3UWith(
        referenceStager: ((String, File, String) -> Unit)?,
        embeddedNameReader: (File, String) -> String?,
    ): String? {
        val enabled = getEnabledSets()
        val allFiles = mutableListOf<Pair<String, String>>() // (sortKey, absolutePath)
        val embeddedNameUpdates = mutableMapOf<Pair<String, String>, String>()
        val stageDir = File(filesDir, "custom_music_stage")
        for (set in enabled) {
            val dir = setDir(set.id)
            for (f in set.files.sorted()) {
                val refUri = set.referencedUris[f]
                if (refUri != null && referenceStager != null) {
                    // Stage referenced file to temp dir
                    stageDir.mkdirs()
                    val staged = File(stageDir, "${set.id}_$f")
                    try {
                        referenceStager(refUri, staged, f)
                        allFiles.add(f.lowercase() to staged.absolutePath)
                        embeddedNameReader(staged, f.substringAfterLast('.', ""))
                            ?.takeIf { it.isNotBlank() }
                            ?.let { embeddedNameUpdates[set.id to f] = it }
                    } catch (e: InsufficientStorageException) {
                        throw e
                    } catch (e: Exception) {
                        logWarn("Failed to stage referenced file $f: ${e.message}")
                    }
                } else {
                    val path = File(dir, f)
                    if (path.exists()) {
                        allFiles.add(f.lowercase() to path.absolutePath)
                    }
                }
            }
        }
        if (allFiles.isEmpty()) {
            return null
        }
        // No global sort -- sets are sequential in their configured order,
        // tracks within each set are already sorted alphabetically above
        val m3u = StringBuilder("# Custom audio playlist\n")
        for ((_, path) in allFiles) {
            m3u.appendLine(path)
        }
        val nameRecords = mutableListOf<MusicNameSidecar.Record>()
        for (set in enabled) {
            val dir = setDir(set.id)
            for (f in set.files) {
                val name =
                    set.trackNames[f] ?: embeddedNameUpdates[set.id to f] ?: set.embeddedTrackNames[f] ?: continue
                val refUri = set.referencedUris[f]
                val absPath =
                    if (refUri != null) {
                        File(stageDir, "${set.id}_$f").absolutePath
                    } else {
                        File(dir, f).absolutePath
                    }
                nameRecords += MusicNameSidecar.Record(listOf(absPath), emptyList(), name)
            }
        }
        val namesText = nameRecords.takeIf { it.isNotEmpty() }?.let(MusicNameSidecar::encode)
        AtomicFilePublication.writeUtf8(File(filesDir, PLAYLIST_FILE), m3u.toString())
        if (namesText != null) {
            AtomicFilePublication.writeUtf8(File(filesDir, NAMES_FILE), namesText)
        } else {
            File(filesDir, NAMES_FILE).delete()
        }
        if (embeddedNameUpdates.isNotEmpty()) {
            sets.replaceAll { set ->
                val additions = embeddedNameUpdates.filterKeys { it.first == set.id }.mapKeys { it.key.second }
                if (additions.isEmpty()) set else set.copy(embeddedTrackNames = set.embeddedTrackNames + additions)
            }
            save()
        }
        logInfo("Wrote $PLAYLIST_FILE with ${allFiles.size} tracks, ${nameRecords.size} names")
        return File(filesDir, PLAYLIST_FILE).absolutePath
    }

    data class TrackDetail(
        val filename: String,
        val setId: String,
        val setLabel: String,
        val matchedName: String?,
        val embeddedName: String? = null,
        val confidence: Float? = null,
        val trackNum: Int? = null,
    )

    /** Get detailed track list including fingerprint-matched names */
    fun getDetailedTrackList(): List<TrackDetail> {
        val enabled = getEnabledSets()
        val tracks = mutableListOf<TrackDetail>()
        for (set in enabled) {
            for (f in set.files.sorted()) {
                tracks.add(
                    TrackDetail(
                        f,
                        set.id,
                        set.label,
                        set.trackNames[f],
                        set.embeddedTrackNames[f],
                        set.trackConfidences[f],
                        set.trackNumbers[f],
                    ),
                )
            }
        }
        // No global sort -- sets are sequential in their configured order,
        // tracks within each set are already sorted alphabetically above
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
                        val namesObj = obj.optJSONObject("trackNames")
                        val names =
                            if (namesObj != null) {
                                namesObj.keys().asSequence().associateWith { namesObj.getString(it) }
                            } else {
                                emptyMap()
                            }
                        val embeddedNamesObj = obj.optJSONObject("embeddedTrackNames")
                        val embeddedNames =
                            if (embeddedNamesObj != null) {
                                embeddedNamesObj.keys().asSequence().associateWith { embeddedNamesObj.getString(it) }
                            } else {
                                emptyMap()
                            }
                        val confObj = obj.optJSONObject("trackConfidences")
                        val confidences =
                            if (confObj != null) {
                                confObj.keys().asSequence().associateWith { confObj.getDouble(it).toFloat() }
                            } else {
                                emptyMap()
                            }
                        val numsObj = obj.optJSONObject("trackNumbers")
                        val trackNums =
                            if (numsObj != null) {
                                numsObj.keys().asSequence().associateWith { numsObj.getInt(it) }
                            } else {
                                emptyMap()
                            }
                        val refsObj = obj.optJSONObject("referencedUris")
                        val refs =
                            if (refsObj != null) {
                                refsObj.keys().asSequence().associateWith { refsObj.getString(it) }
                            } else {
                                emptyMap()
                            }
                        AudioSet(
                            id = obj.getString("id"),
                            label = obj.optString("label", "Unnamed"),
                            files = (0 until filesArr.length()).map { filesArr.getString(it) },
                            enabled = obj.optBoolean("enabled", true),
                            order = obj.optInt("order", 0),
                            trackNames = names,
                            embeddedTrackNames = embeddedNames,
                            trackConfidences = confidences,
                            trackNumbers = trackNums,
                            referencedUris = refs,
                        )
                    }.toMutableList()
            logInfo("Loaded ${sets.size} custom audio sets")
        } catch (e: Exception) {
            logError("Failed to load $SETS_FILE", e)
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
            if (set.trackNames.isNotEmpty()) {
                val namesObj = JSONObject()
                set.trackNames.forEach { (k, v) -> namesObj.put(k, v) }
                obj.put("trackNames", namesObj)
            }
            if (set.embeddedTrackNames.isNotEmpty()) {
                val namesObj = JSONObject()
                set.embeddedTrackNames.forEach { (k, v) -> namesObj.put(k, v) }
                obj.put("embeddedTrackNames", namesObj)
            }
            if (set.trackConfidences.isNotEmpty()) {
                val confObj = JSONObject()
                set.trackConfidences.forEach { (k, v) -> confObj.put(k, v.toDouble()) }
                obj.put("trackConfidences", confObj)
            }
            if (set.trackNumbers.isNotEmpty()) {
                val numsObj = JSONObject()
                set.trackNumbers.forEach { (k, v) -> numsObj.put(k, v) }
                obj.put("trackNumbers", numsObj)
            }
            if (set.referencedUris.isNotEmpty()) {
                val refsObj = JSONObject()
                set.referencedUris.forEach { (k, v) -> refsObj.put(k, v) }
                obj.put("referencedUris", refsObj)
            }
            arr.put(obj)
        }
        json.put("sets", arr)
        File(filesDir, SETS_FILE).writeText(json.toString(2))
    }
}

internal fun removeReferencedFileFromSet(
    set: CustomAudioSetManager.AudioSet,
    filename: String,
): CustomAudioSetManager.AudioSet? {
    val updated =
        set.copy(
            files = set.files.filter { it != filename },
            trackNames = set.trackNames - filename,
            embeddedTrackNames = set.embeddedTrackNames - filename,
            trackConfidences = set.trackConfidences - filename,
            trackNumbers = set.trackNumbers - filename,
            referencedUris = set.referencedUris - filename,
        )
    return updated.takeIf { it.files.isNotEmpty() }
}
