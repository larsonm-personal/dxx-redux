package com.dxxredux.app

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.IOException

// Keep in sync with CD_CUE_MAX_BYTES in native cd_read_contract.h.
internal const val CD_CUE_MAX_BYTES = 1024L * 1024L

// Keep in sync with MAX_SOURCES and MAX_TRACKS in native rbaudio_bin.c.
internal const val AUDIO_PLAYLIST_MAX_SOURCES = 8
internal const val AUDIO_PLAYLIST_MAX_TRACKS = 100

internal fun requireCueSizeWithinLimit(
    size: Long,
    label: String,
) {
    if (size <= 0L || size > CD_CUE_MAX_BYTES) {
        throw IOException("CUE $label must be between 1 and $CD_CUE_MAX_BYTES bytes")
    }
}

internal fun requireAudioPlaylistCapacity(sources: List<AudioSourceManager.AudioSource>) {
    if (sources.size > AUDIO_PLAYLIST_MAX_SOURCES) {
        throw IOException("Audio playlist has ${sources.size} sources; maximum is $AUDIO_PLAYLIST_MAX_SOURCES")
    }
    var totalTracks = 0
    sources.forEach { source ->
        if (source.trackCount <= 0 || source.audioTrackCount <= 0 || source.audioTrackCount > source.trackCount) {
            throw IOException("Audio source ${source.discLabel} has invalid track counts")
        }
        if (source.trackCount > AUDIO_PLAYLIST_MAX_TRACKS - totalTracks) {
            throw IOException("Audio playlist exceeds the $AUDIO_PLAYLIST_MAX_TRACKS track maximum")
        }
        totalTracks += source.trackCount
    }
}

internal fun getManagedInternalArtifactPaths(
    filesDir: File,
    sources: List<AudioSourceManager.AudioSource>,
): Set<String> =
    sources
        .flatMap { source -> getManagedInternalArtifactFilesForSource(filesDir, source) }
        .map { it.absolutePath }
        .toSet()

internal fun getSafLinkedHelperArtifactPaths(
    filesDir: File,
    sources: List<AudioSourceManager.AudioSource>,
): Set<String> = getManagedInternalArtifactPaths(filesDir, sources.filter(::hasSafLinkedCdContent))

internal fun AudioSourceManager.AudioSource.binContentUriList(): List<String> =
    if (binContentUris.isNotEmpty()) binContentUris else listOfNotNull(binContentUri)

internal fun resolvePlaylistCuePath(
    filesDir: File,
    source: AudioSourceManager.AudioSource,
    fallback: () -> String,
): String {
    val localCue = resolveCdAudioSourceFile(filesDir, source.cuePath)
    val binContentUris = source.binContentUriList()
    return if (binContentUris.isNotEmpty() && binContentUris.all(::isLocalCdContentPath) && localCue.exists()) {
        localCue.absolutePath
    } else {
        fallback()
    }
}

private fun getManagedInternalArtifactFilesForSource(
    filesDir: File,
    source: AudioSourceManager.AudioSource,
): List<File> {
    val files = linkedSetOf<File>()
    val cueFile = resolveCdAudioSourceFile(filesDir, source.cuePath)
    if (isManagedCdArtifactFile(cueFile, filesDir)) {
        files.add(cueFile)
    }

    val localBinContentPaths = source.binContentUriList().filter(::isLocalCdContentPath)
    if (localBinContentPaths.isNotEmpty()) {
        localBinContentPaths
            .map(::File)
            .filter { file -> isManagedCdArtifactFile(file, filesDir) }
            .forEach(files::add)
    } else if (source.binContentUriList().isEmpty()) {
        source.binPaths
            .map { resolveCdAudioSourceFile(filesDir, it) }
            .filter { file -> isManagedCdArtifactFile(file, filesDir) }
            .forEach(files::add)
    }

    return files.toList()
}

private fun isManagedCdArtifactFile(
    file: File,
    filesDir: File,
): Boolean {
    if (!file.name.endsWith(".cue", ignoreCase = true) && !file.name.endsWith(".bin", ignoreCase = true)) {
        return false
    }
    val generatedRoot = File(ImportLocationManager(filesDir).getActiveRoot(), GENERATED_CD_AUDIO_ARTIFACT_DIR)
    if (isUnderDirectory(file, generatedRoot)) return true
    if (!isUnderDirectory(file, filesDir)) return false
    if (file.name.endsWith(".cue", ignoreCase = true)) return true
    return isGeneratedMergedStorageArtifact(file) || isLegacyGeneratedMergedStorageArtifact(file)
}

private fun isUnderDirectory(
    file: File,
    root: File,
): Boolean =
    try {
        val filePath = file.canonicalPath
        val rootPath = root.canonicalPath
        filePath == rootPath || filePath.startsWith(rootPath + File.separator)
    } catch (_: Exception) {
        false
    }

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
    private val setDir: File? = null,
) {
    companion object {
        private const val TAG = "DXX-AudioSrc"
        private const val SOURCES_FILE = "audio_sources.json"
        private const val PLAYLIST_FILE = "audio_playlist.json"

        fun forActiveSet(filesDir: File): AudioSourceManager {
            val fileSets = FileSetManager(filesDir)
            return AudioSourceManager(filesDir, fileSets.getSetDir(fileSets.getActive()))
        }

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
        // path to CUE file (relative to filesDir, or an absolute local path)
        val cuePath: String,
        // paths to BIN file(s), relative to filesDir or absolute local paths
        val binPaths: List<String>,
        // human-readable disc label
        val discLabel: String,
        // known_discs.json disc id (or "unknown")
        val discId: String,
        // total tracks (data + audio)
        val trackCount: Int,
        // audio tracks only
        val audioTrackCount: Int,
        // physical 1-based CUE track numbers in audio playback order
        val audioTrackNumbers: List<Int> = emptyList(),
        // e.g., 0x7d0ff809 for backward compat
        val legacyDiscId: Long,
        val enabled: Boolean = true,
        // user-defined sort order
        val order: Int = 0,
        // fingerprint-matched track names: 1-based track number -> name
        val trackNames: Map<Int, String> = emptyMap(),
        // SAF content URI for BIN file (when referenced in-place, not copied)
        val binContentUri: String? = null,
        // SAF content URI(s) for BIN file(s) when referenced in-place, not copied
        val binContentUris: List<String> = emptyList(),
        // SAF content URI for CUE file (when referenced in-place, not copied)
        val cueContentUri: String? = null,
    )

    private var sources: MutableList<AudioSource> = mutableListOf()
    private var loadedRegistryBytes: ByteArray? = null
    private val sourcesFile
        get() = File(setDir?.let { File(it, ".content/audio") } ?: filesDir, SOURCES_FILE)

    init {
        load()
    }

    /** All registered audio sources, sorted by user order */
    fun getSources(): List<AudioSource> = sources.sortedBy { it.order }

    internal fun registryFile(): File = sourcesFile

    internal fun trackedSafUris(): List<String> = sources.flatMap { it.trackedSafUris() }

    /** Get only enabled sources, in order */
    fun getEnabledSources(): List<AudioSource> = sources.filter { it.enabled }.sortedBy { it.order }

    /** Add a new audio source */
    fun addSource(source: AudioSource) {
        AtomicFilePublication.transaction {
            reloadIfChanged()
            // Remove any existing source with the same id
            sources.removeAll { it.id == source.id }
            sources.add(source)
            save()
        }
        Log.i(TAG, "Added source: ${source.discLabel} (${source.audioTrackCount} audio tracks)")
    }

    /** Remove an audio source by id */
    fun removeSource(
        id: String,
        context: Context,
    ) {
        removeSource(id) { removedTrackedUris, retainedTrackedUris ->
            revokeUnusedPersistedReadPermissions(context, removedTrackedUris, retainedTrackedUris)
        }
    }

    internal fun removeSource(
        id: String,
        revokePermissions: (removedTrackedUris: Collection<String>, retainedTrackedUris: Collection<String>) -> Unit,
    ) {
        val permissions =
            AtomicFilePublication.transaction {
                reloadIfChanged()
                val source = sources.firstOrNull { it.id == id } ?: return@transaction null
                val removedTrackedUris = source.trackedSafUris()
                val retainedTrackedUris = sources.filterNot { it.id == id }.flatMap { it.trackedSafUris() }
                releaseSourceResources(source)
                sources.removeAll { it.id == id }
                pruneOrphanedGeneratedMergedFiles()
                save()
                removedTrackedUris to retainedTrackedUris
            } ?: return
        revokePermissions(permissions.first, permissions.second)
    }

    fun getManagedInternalArtifactPaths(): Set<String> = getManagedInternalArtifactPaths(filesDir, sources)

    fun clearAll(
        context: Context? = null,
        retainedTrackedUris: Collection<String> = emptyList(),
    ) {
        val removedTrackedUris =
            AtomicFilePublication.transaction {
                reloadIfChanged()
                val removed = sources.flatMap { it.trackedSafUris() }
                closeActivePfds()
                sources.forEach(::releaseSourceResources)
                sources.clear()
                pruneOrphanedGeneratedMergedFiles()
                sourcesFile.delete()
                File(filesDir, PLAYLIST_FILE).delete()
                removed
            }
        context?.let {
            revokeUnusedPersistedReadPermissions(it, removedTrackedUris, retainedTrackedUris)
        }
    }

    private fun releaseSourceResources(source: AudioSource) {
        val managedFiles = managedInternalFilesForSource(source)
        managedFiles.forEach { file ->
            if (file.exists() && isManagedCdArtifactFile(file, filesDir)) {
                file.delete()
            }
        }
        cleanupEmptyManagedDirs(managedFiles)
    }

    private fun AudioSource.trackedSafUris(): List<String> =
        (binContentUriList() + listOfNotNull(cueContentUri)).filterNot(::isLocalCdContentPath)

    private fun managedInternalFilesForSource(source: AudioSource): List<File> =
        getManagedInternalArtifactFilesForSource(filesDir, source)

    private fun cleanupEmptyManagedDirs(files: List<File>) {
        val cleanupRoots =
            listOf(filesDir, ImportLocationManager(filesDir).getActiveRoot())
                .map { it.absoluteFile }
        files
            .mapNotNull(File::getParentFile)
            .distinct()
            .forEach { parent ->
                var current = parent
                while (
                    current.isDirectory &&
                    (current.list()?.isEmpty() == true) &&
                    cleanupRoots.none { sameFile(current, it) } &&
                    cleanupRoots.any { isUnderDirectory(current, it) }
                ) {
                    current.delete()
                    current = current.parentFile ?: break
                }
            }
    }

    private fun sameFile(
        a: File,
        b: File,
    ): Boolean =
        try {
            a.canonicalFile == b.canonicalFile
        } catch (_: Exception) {
            a.absoluteFile == b.absoluteFile
        }

    private fun pruneOrphanedGeneratedMergedFiles() {
        val referencedPaths = getManagedInternalArtifactPaths()
        val generatedRoot = generatedCdAudioArtifactsDir(filesDir)
        val scanDirs =
            listOf(filesDir, generatedRoot)
                .distinctBy { it.absolutePath }
        scanDirs.forEach { dir ->
            val cueFiles =
                dir.listFiles()?.filter { it.isFile && it.name.endsWith(".cue", ignoreCase = true) }
                    ?: return@forEach
            cueFiles.forEach { cueFile ->
                val isOwnedGeneration =
                    isUnderDirectory(cueFile, generatedRoot) ||
                        isGeneratedMergedCueFile(cueFile) ||
                        (isUnderDirectory(cueFile, filesDir) && isLegacyGeneratedMergedStorageArtifact(cueFile))
                if (!isOwnedGeneration) return@forEach
                val binFile = File(cueFile.parentFile, "${cueFile.nameWithoutExtension}.bin")
                if (!binFile.isFile) return@forEach
                if (cueFile.absolutePath in referencedPaths || binFile.absolutePath in referencedPaths) return@forEach
                cueFile.delete()
                binFile.delete()
            }
        }
    }

    /**
     * Remove sources whose BIN/CUE files no longer exist on disk.
     * Checks paths relative to filesDir with case-insensitive matching
     * (GOG extraction may produce different case than what was stored).
     * Returns list of pruned source labels for user notification.
     */
    fun pruneMissingSources(setDir: File? = null): List<String> =
        AtomicFilePublication.transaction {
            reloadIfChanged()
            val activeSetDir = setDir ?: activeSetDirOrNull()
            val pruned = mutableListOf<String>()
            val toRemove =
                sources.filter { src ->
                    val binContentUris = src.binContentUriList()
                    if (binContentUris.any { !isLocalCdContentPath(it) }) return@filter false
                    val allFiles =
                        if (binContentUris.isNotEmpty()) {
                            binContentUris + src.cuePath
                        } else {
                            src.binPaths + src.cuePath
                        }
                    allFiles.any { name ->
                        resolveExistingFile(name, activeSetDir, includeDisabledManaged = true) == null
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
            pruned
        }

    /** Toggle enabled state */
    fun setEnabled(
        id: String,
        enabled: Boolean,
    ) {
        AtomicFilePublication.transaction {
            reloadIfChanged()
            sources.replaceAll {
                if (it.id == id) it.copy(enabled = enabled) else it
            }
            save()
        }
    }

    /** Update ordering */
    fun reorder(orderedIds: List<String>) {
        AtomicFilePublication.transaction {
            reloadIfChanged()
            orderedIds.forEachIndexed { index, id ->
                sources.replaceAll {
                    if (it.id == id) it.copy(order = index) else it
                }
            }
            save()
        }
    }

    /**
     * Write audio_playlist.json for the C engine.
     *
     * Called before game launch so RBAInit() can read it.
     * For SAF content URIs, opens file descriptors and writes /proc/self/fd paths.
     * Local filesystem paths are written directly. Call [closeActivePfds] after
     * the game exits to release opened SAF descriptors.
     * Returns true if a playlist was written, false if legacy mode.
     */
    fun writePlaylist(resolver: ContentResolver? = null): Boolean {
        closeActivePfds()
        val playlistFile = File(filesDir, PLAYLIST_FILE)
        val activeSetDir = activeSetDirOrNull()
        val enabled = getEnabledSources().filter { sourceFilesAvailable(it, activeSetDir) }
        if (enabled.isEmpty()) {
            return false
        }
        requireAudioPlaylistCapacity(enabled)

        val json = JSONObject()
        val arr = JSONArray()
        for (src in enabled) {
            val entry = JSONObject()
            val cuePath = resolvePlaylistCuePath(filesDir, src) { stagedCuePath(src, activeSetDir) }
            resolveCdAudioSourceFile(filesDir, cuePath).takeIf { it.isFile }?.let {
                requireCueSizeWithinLimit(it.length(), it.name)
            }
            entry.put("cue", cuePath)
            val bins = JSONArray()
            resolvePlaylistBinPaths(src, resolver, activeSetDir).forEach(bins::put)
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

        AtomicFilePublication.writeUtf8(playlistFile, json.toString(2))
        runCatching { Log.i(TAG, "Wrote $PLAYLIST_FILE with ${enabled.size} sources (${activePfds.size} SAF fds)") }
        return true
    }

    private fun activeSetDirOrNull(): File? =
        setDir ?: runCatching { FileSetManager(filesDir).let { it.getSetDir(it.getActive()) } }.getOrNull()

    private fun resolvePlaylistBinPaths(
        src: AudioSource,
        resolver: ContentResolver?,
        activeSetDir: File?,
    ): List<String> {
        val binContentUris = src.binContentUriList()
        if (binContentUris.isEmpty()) {
            return src.binPaths.map { resolveBinPath(it, activeSetDir) }
        }

        val openedPfds = mutableListOf<ParcelFileDescriptor>()
        return try {
            binContentUris
                .map { uriStr ->
                    if (isLocalCdContentPath(uriStr)) {
                        return@map resolveBinPath(uriStr, activeSetDir)
                    }
                    val pfd =
                        openBinContentUri(uriStr, resolver)
                            ?: throw IllegalStateException("Could not open BIN content URI: $uriStr")
                    openedPfds.add(pfd)
                    "/proc/self/fd/${pfd.fd}"
                }.also { activePfds.addAll(openedPfds) }
        } catch (e: Exception) {
            openedPfds.forEach { pfd ->
                try {
                    pfd.close()
                } catch (_: Exception) {
                    // already closed
                }
            }
            Log.w(TAG, "BIN fd open failed for ${src.discLabel}", e)
            src.binPaths.map { resolveBinPath(it, activeSetDir) }
        }
    }

    private fun openBinContentUri(
        uriStr: String,
        resolver: ContentResolver?,
    ): ParcelFileDescriptor? =
        if (isLocalCdContentPath(uriStr)) {
            ParcelFileDescriptor.open(
                File(uriStr),
                ParcelFileDescriptor.MODE_READ_ONLY,
            )
        } else if (resolver != null) {
            resolver.openFileDescriptor(Uri.parse(uriStr), "r")
        } else {
            null
        }

    private fun resolveExistingFile(
        path: String,
        activeSetDir: File?,
        includeDisabledManaged: Boolean = false,
    ): File? {
        val direct = File(path)
        if (direct.isAbsolute && direct.exists()) return direct

        val local = File(filesDir, path)
        if (local.exists()) return local

        val name = direct.name.takeIf { it.isNotEmpty() } ?: return null
        return activeSetDir?.let { setDir ->
            findCaseInsensitive(setDir, name)
                ?: FileSetContentManager(setDir).resolveFile(path, !includeDisabledManaged)
        }
    }

    private fun sourceFilesAvailable(
        source: AudioSource,
        activeSetDir: File?,
    ): Boolean {
        val externalCue = source.cueContentUri?.takeUnless(::isLocalCdContentPath)
        if (externalCue == null && resolveExistingFile(source.cuePath, activeSetDir) == null) return false
        val contentBins = source.binContentUriList()
        val localBins =
            if (contentBins.isEmpty()) source.binPaths else contentBins.filter(::isLocalCdContentPath)
        return localBins.all { resolveExistingFile(it, activeSetDir) != null }
    }

    private fun findCaseInsensitive(
        dir: File,
        name: String,
    ): File? {
        val target = name.lowercase()
        return dir.listFiles()?.firstOrNull { it.name.lowercase() == target }
    }

    private fun stagedCuePath(
        src: AudioSource,
        activeSetDir: File?,
    ): String {
        val localCue = resolveCdAudioSourceFile(filesDir, src.cuePath)
        if (localCue.exists()) return src.cuePath

        val cueFile = resolveExistingFile(src.cuePath, activeSetDir) ?: return src.cuePath
        requireCueSizeWithinLimit(cueFile.length(), cueFile.name)
        val stageDir = File(filesDir, "audio_cue_stage").also { it.mkdirs() }
        val safeId = src.id.replace(Regex("[^A-Za-z0-9_.-]"), "_")
        val ext = cueFile.extension.ifEmpty { "cue" }
        val staged = File(stageDir, "$safeId.$ext")
        if (!staged.exists() || staged.length() != cueFile.length() || staged.lastModified() < cueFile.lastModified()) {
            LauncherFileCopy.copyFileToFile(cueFile, staged, maxBytes = CD_CUE_MAX_BYTES)
            staged.setLastModified(cueFile.lastModified())
        }
        return staged.relativeTo(filesDir).path
    }

    private fun resolveBinPath(
        path: String,
        activeSetDir: File?,
    ): String {
        val resolved = resolveExistingFile(path, activeSetDir) ?: return path
        return if (isUnderDirectory(resolved, filesDir)) {
            resolved.relativeTo(filesDir).path
        } else {
            resolved.absolutePath
        }
    }

    // ── Persistence ───────────────────────────────────────────────

    private fun load() {
        val file = sourcesFile
        loadedRegistryBytes = file.takeIf(File::isFile)?.readBytes()
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
                        val legacyBinContentUri = obj.optString("bin_content_uri", "").ifEmpty { null }
                        val binContentUris =
                            obj.optJSONArray("bin_content_uris")?.let { uriArr ->
                                (0 until uriArr.length()).mapNotNull { index ->
                                    uriArr.optString(index).ifEmpty { null }
                                }
                            } ?: legacyBinContentUri?.let(::listOf) ?: emptyList()
                        AudioSource(
                            id = obj.getString("id"),
                            cuePath = obj.getString("cue"),
                            binPaths = (0 until binArr.length()).map { binArr.getString(it) },
                            discLabel = obj.optString("label", "Unknown Disc"),
                            discId = obj.optString("disc_id", "unknown"),
                            trackCount = obj.optInt("track_count", 0),
                            audioTrackCount = obj.optInt("audio_track_count", 0),
                            audioTrackNumbers =
                                obj.optJSONArray("audio_track_numbers")?.let { numbers ->
                                    (0 until numbers.length()).map(numbers::getInt)
                                } ?: emptyList(),
                            legacyDiscId = obj.optLong("legacy_disc_id", 0),
                            enabled = obj.optBoolean("enabled", true),
                            order = obj.optInt("order", 0),
                            trackNames =
                                obj.optJSONObject("track_names")?.let { tn ->
                                    tn.keys().asSequence().associate { k -> k.toInt() to tn.getString(k) }
                                } ?: emptyMap(),
                            binContentUri = legacyBinContentUri ?: binContentUris.firstOrNull(),
                            binContentUris = binContentUris,
                            cueContentUri = obj.optString("cue_content_uri", "").ifEmpty { null },
                        )
                    }.toMutableList()
            runCatching { Log.i(TAG, "Loaded ${sources.size} audio sources") }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load $SOURCES_FILE", e)
            sources = mutableListOf()
        }
    }

    private fun save() {
        sourcesFile.parentFile?.mkdirs()
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
            if (src.audioTrackNumbers.isNotEmpty()) {
                val audioTrackNumbers = JSONArray()
                src.audioTrackNumbers.forEach(audioTrackNumbers::put)
                obj.put("audio_track_numbers", audioTrackNumbers)
            }
            obj.put("legacy_disc_id", src.legacyDiscId)
            obj.put("enabled", src.enabled)
            obj.put("order", src.order)
            if (src.trackNames.isNotEmpty()) {
                val tn = JSONObject()
                src.trackNames.forEach { (k, v) -> tn.put(k.toString(), v) }
                obj.put("track_names", tn)
            }
            val binContentUris = src.binContentUriList()
            if (binContentUris.isNotEmpty()) {
                val binUriArray = JSONArray()
                binContentUris.forEach(binUriArray::put)
                obj.put("bin_content_uris", binUriArray)
                obj.put("bin_content_uri", binContentUris.first())
            }
            src.cueContentUri?.let { obj.put("cue_content_uri", it) }
            arr.put(obj)
        }
        json.put("sources", arr)
        val text = json.toString(2)
        AtomicFilePublication.writeUtf8(sourcesFile, text)
        loadedRegistryBytes = text.toByteArray(Charsets.UTF_8)
    }

    private fun reloadIfChanged() {
        val current = sourcesFile.takeIf(File::isFile)?.readBytes()
        if (!java.util.Arrays.equals(current, loadedRegistryBytes)) load()
    }
}
