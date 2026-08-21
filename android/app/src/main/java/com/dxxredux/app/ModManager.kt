package com.dxxredux.app

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.InputStream
import java.security.MessageDigest
import java.util.Locale
import java.util.zip.ZipFile
import java.util.zip.ZipInputStream

internal const val ACTIVE_MOD_PATH_LIMIT = 64

internal class ActiveModPathCapacityException(
    count: Int,
) : IllegalStateException("Enabled content needs $count mount paths; the game supports at most $ACTIVE_MOD_PATH_LIMIT")

internal fun requireActiveModPathCapacity(paths: List<String>) {
    if (paths.size > ACTIVE_MOD_PATH_LIMIT) throw ActiveModPathCapacityException(paths.size)
}

/**
 * Manages .dxa mod files: import, enable/disable, reorder, delete.
 *
 * Mods are stored in filesDir/mods/ with metadata in mod_manifest.json.
 * Before game launch, [writeEnabledModPaths] writes .active_mod_paths
 * which the C engine reads in physfsx.c to mount enabled mods via PhysFS.
 */
class ModManager(
    private val filesDir: File,
    private val context: Context? = null,
) {
    companion object {
        private const val TAG = "DXX-Mods"
        private const val MANIFEST_FILE = "mod_manifest.json"
        private const val GENERATED_PATCH_DIR = ".generated_mod_patches"
        private const val GENERATED_MISSION_ZIP_DIR = ".generated_mission_zips"
        const val MOD_KIND_DXA = "dxa"
        const val MOD_KIND_MISSION_ZIP = MissionZip.KIND
        private val MISSION_SOUNDTRACK_EXTENSIONS = setOf("flac", "hmp", "mid", "mp3", "ogg")
        private val MISSION_SONG_LIST_FILES = setOf("descent.sng", "dxx-r.sng")
    }

    data class ModInfo(
        val filename: String,
        val displayName: String,
        val enabled: Boolean,
        val addedAt: Long,
        val sizeBytes: Long,
        val game: String, // "d1", "d2", or "both"
        val order: Int,
        val kind: String = MOD_KIND_DXA,
        val category: String? = null,
        val missionTitle: String? = null,
        val importMode: String? = null,
    )

    data class ModCompatibilityFailure(
        val modDisplayName: String,
        val requiredBaseDescription: String,
        val filename: String,
        val expectedSha256: String,
        val expectedVersion: String,
        val actualSha256: String?,
        val actualVersion: String?,
        val reason: String,
    )

    data class ModPatchConflict(
        val patchPath: String,
        val modDisplayNames: List<String>,
        val details: List<String> = emptyList(),
    )

    data class ModCompatibilityReport(
        val failures: List<ModCompatibilityFailure>,
        val patchConflicts: List<ModPatchConflict> = emptyList(),
    ) {
        val ok: Boolean get() = failures.isEmpty() && patchConflicts.isEmpty()

        fun toUserMessage(): String {
            if (ok) return ""
            return buildString {
                if (failures.isNotEmpty()) {
                    append("One or more enabled mods require different base game files")
                    val grouped = failures.groupBy { it.modDisplayName }
                    for ((modName, modFailures) in grouped) {
                        append("\n\n")
                        append(modName)
                        val description = modFailures.first().requiredBaseDescription
                        if (description.isNotBlank()) {
                            append("\nRequired base files: ")
                            append(description)
                        }
                        for (failure in modFailures) {
                            append("\n")
                            append(failure.filename)
                            append(": expected ")
                            append(failure.expectedVersion)
                            append(" sha256=")
                            append(failure.expectedSha256)
                            append("; found ")
                            if (failure.actualSha256 == null) {
                                append("missing")
                            } else {
                                append(failure.actualVersion ?: "unknown")
                                append(" sha256=")
                                append(failure.actualSha256)
                            }
                            if (failure.reason.isNotBlank()) {
                                append("\n")
                                append(failure.reason)
                            }
                        }
                    }
                }
                if (patchConflicts.isNotEmpty()) {
                    if (isNotEmpty()) append("\n\n")
                    append("Two or more enabled mods patch the same engine metadata file")
                    append("\nMove or disable one of the conflicting mods, or use a combined patch DXA")
                    for (conflict in patchConflicts) {
                        append("\n")
                        append(conflict.patchPath)
                        append(": ")
                        append(conflict.modDisplayNames.joinToString(", "))
                        if (conflict.details.isNotEmpty()) {
                            append("\n")
                            append(conflict.details.joinToString("\n"))
                        }
                    }
                }
            }
        }

        fun toLogMessage(): String = toUserMessage().replace('\n', ' ')
    }

    data class VisualReplacementSummary(
        val omittedModCount: Int = 0,
        val omittedTextureCount: Int = 0,
        val omittedModNames: List<String> = emptyList(),
    ) {
        val hasOmittedVisuals: Boolean get() = omittedModCount > 0
    }

    data class ModFileCategorySummary(
        val label: String,
        val count: Int,
        val sizeBytes: Long,
        val examples: List<String>,
        val examplesTruncated: Boolean,
    )

    data class ModPatchDetail(
        val path: String,
        val sizeBytes: Long,
        val operationCount: Int?,
        val affectedFiles: List<String>,
        val expectedBaseVersions: List<String>,
    )

    data class ModBaseRequirement(
        val game: String,
        val role: String,
        val filename: String,
        val expectedSha256: String,
        val expectedVersion: String,
        val actualSha256: String?,
        val actualVersion: String?,
        val required: Boolean,
        val reason: String,
        val patchPaths: List<String>,
    ) {
        val ok: Boolean get() = !required || actualSha256 == expectedSha256
    }

    data class ModDetails(
        val archivePath: String,
        val fileCount: Int,
        val archiveSizeBytes: Long,
        val manifestSchema: String?,
        val notes: List<String>,
        val categories: List<ModFileCategorySummary>,
        val patches: List<ModPatchDetail>,
        val baseRequirements: List<ModBaseRequirement>,
        val problems: List<String>,
        val missionZip: MissionZip.ScanResult? = null,
        val missionZipMusic: MissionZipMusicCatalog? = null,
        val missionZipExtraction: MissionZipExtractionInfo? = null,
    )

    data class MissionZipExtractionInfo(
        val rootPath: String,
        val sourceArchiveName: String,
        val archiveFormat: String,
        val ownerSizeBytes: Long,
        val extractedSizeBytes: Long,
        val fileCount: Int,
        val importMode: String,
    )

    private data class ActualBaseFile(
        val sha256: String,
        val versionName: String?,
    )

    private data class RawBaseRequirement(
        val game: String,
        val role: String,
        val filename: String,
        val expectedSha256: String,
        val expectedVersion: String,
        val required: Boolean,
        val reason: String,
        val patchPaths: List<String>,
    )

    private data class PatchOwner(
        val patchPath: String,
        val modDisplayName: String,
        val modFilename: String,
    )

    private data class PatchScope(
        val rowPath: String,
        val field: String?,
    ) {
        val displayPath: String
            get() = if (this.field == null) rowPath else "$rowPath/${this.field}"

        fun overlaps(other: PatchScope): Boolean =
            rowPath == other.rowPath && (field == null || other.field == null || field == other.field)
    }

    private data class PatchWrite(
        val owner: PatchOwner,
        val scope: PatchScope,
        val valueText: String,
    )

    private data class ModPatchDocument(
        val owner: PatchOwner,
        val operations: JSONArray?,
        val writes: List<PatchWrite>,
        val problem: String? = null,
    )

    private data class PatchConflictBuilder(
        val names: MutableSet<String> = linkedSetOf(),
        val details: MutableSet<String> = linkedSetOf(),
    )

    private data class CategoryBucket(
        val label: String,
        var count: Int = 0,
        var sizeBytes: Long = 0,
        val examples: MutableList<String> = mutableListOf(),
    )

    private val modsDir get() = File(filesDir, "mods").also { it.mkdirs() }
    private val manifestFile get() = File(modsDir, MANIFEST_FILE)

    private var mods: MutableList<ModInfo> = mutableListOf()

    init {
        load()
    }

    fun listMods(): List<ModInfo> = mods.sortedBy { it.order }

    /** Re-read the manifest from disk, e.g. after another ModManager instance wrote it */
    fun reload() {
        load()
    }

    fun setEnabled(
        filename: String,
        enabled: Boolean,
    ) {
        val previous = mods.firstOrNull { it.filename == filename }
        if (enabled) {
            val scan = DxaTextureScanner.scan(File(modsDir, filename))
            if (scan?.canEnable != true) {
                Log.w(TAG, "Refusing to enable unsafe or unreadable mod: $filename")
                return
            }
        }
        updateManifest {
            mods =
                mods
                    .map {
                        if (it.filename == filename) it.copy(enabled = enabled) else it
                    }.toMutableList()
            save()
        }
        if (enabled && previous?.enabled == false && previous.kind == MOD_KIND_MISSION_ZIP &&
            modHasMissionSoundtrack(previous)
        ) {
            context?.let { selectBundledMusicForNewMission(filesDir, it, previous.game) }
        }
    }

    fun deleteMod(filename: String) {
        updateManifest {
            File(modsDir, filename).delete()
            MissionZipMusicNames.deleteCacheFile(filesDir, filename)
            MissionZipExtractionStore(filesDir).removeOwner(filename)
            mods.removeAll { it.filename == filename }
            save()
        }
        logInfo("Deleted mod: $filename")
    }

    fun clearAllMods(): Int {
        val removedMods = (modsDir.listFiles() ?: emptyArray()).count { it.isFile && it.name != MANIFEST_FILE }

        if (modsDir.exists()) modsDir.deleteRecursively()
        for (gameDir in arrayOf("d1x-redux", "d2x-redux")) {
            val dir = File(filesDir, gameDir)
            File(dir, ".active_mod_paths").delete()
            File(dir, GENERATED_PATCH_DIR).deleteRecursively()
            File(dir, GENERATED_MISSION_ZIP_DIR).deleteRecursively()
        }

        mods = mutableListOf()
        NativeTextureLookupCache.clear()
        Log.i(TAG, "Cleared all mods")
        return removedMods
    }

    /** Import a .dxa file from a SAF URI. Streams to avoid loading into memory. */
    fun importMod(
        uri: Uri,
        displayName: String,
        contentResolver: ContentResolver,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): ModInfo? {
        val safeName = displayName.replace(Regex("[^a-zA-Z0-9._-]"), "_")
        val dest = File(modsDir, safeName)
        try {
            ImportStorageGuard.requireFreeSpace(
                modsDir,
                ImportStorageGuard.queryUriSizeBytes(contentResolver, uri) ?: 0L,
                "import mod $displayName",
            )
            contentResolver.openInputStream(uri)?.use { input ->
                FileOutputStream(dest).use { output ->
                    LauncherFileCopy.copyStream(
                        input,
                        output,
                        ImportStorageGuard.queryUriSizeBytes(contentResolver, uri) ?: 0L,
                        displayName,
                        onProgress = onProgress,
                    )
                }
            } ?: run {
                Log.e(TAG, "Failed to open input stream for $displayName (URI: $uri)")
                return null
            }
        } catch (e: InsufficientStorageException) {
            Log.e(TAG, "Not enough space to import $displayName", e)
            ImportStorageGuard.recordFailure(filesDir, "DXA import failed for $displayName", e)
            dest.delete()
            throw e
        } catch (e: Exception) {
            Log.e(TAG, "Failed to copy $displayName to ${dest.absolutePath}: ${e.message}", e)
            ImportStorageGuard.recordFailure(filesDir, "DXA import failed for $displayName", e)
            dest.delete()
            return null
        }

        val fileSize = dest.length()
        if (fileSize == 0L) {
            Log.e(TAG, "Import produced 0-byte file for $displayName (dest: ${dest.absolutePath})")
            dest.delete()
            return null
        }

        // Remove existing entry with same filename
        mods.removeAll { it.filename == safeName }

        val maxOrder = mods.maxOfOrNull { it.order } ?: -1
        val mod =
            ModInfo(
                filename = safeName,
                displayName = generateDisplayName(safeName),
                enabled = DxaTextureScanner.scan(dest)?.canEnable == true,
                addedAt = System.currentTimeMillis(),
                sizeBytes = dest.length(),
                game = detectGame(safeName),
                order = maxOrder + 1,
            )
        mods.add(mod)
        save()
        Log.i(TAG, "Imported mod: ${mod.displayName} (${mod.sizeBytes / 1024 / 1024} MB)")
        return mod
    }

    /** Register a mod file that was already downloaded directly into modsDir */
    fun importCompleted(
        filename: String,
        displayName: String,
        sizeBytes: Long,
        game: String,
    ) {
        mods.removeAll { it.filename == filename }
        val maxOrder = mods.maxOfOrNull { it.order } ?: -1
        mods.add(
            ModInfo(
                filename = filename,
                displayName = displayName,
                enabled = DxaTextureScanner.scan(File(modsDir, filename))?.canEnable == true,
                addedAt = System.currentTimeMillis(),
                sizeBytes = sizeBytes,
                game = game,
                order = maxOrder + 1,
            ),
        )
        save()
        Log.i(TAG, "Registered downloaded mod: $displayName")
    }

    /** Import a mission ZIP from a SAF URI. */
    fun importMissionZip(
        uri: Uri,
        displayName: String,
        contentResolver: ContentResolver,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): ModInfo? {
        val suffix =
            GameFileFormats
                .extensionOf(displayName)
                .takeIf { it in setOf("zip", "7z", "rar") }
                ?.let { ".$it" }
                ?: ".zip"
        val temp = File.createTempFile("mission_zip_import_", suffix, modsDir)
        try {
            val total = ImportStorageGuard.queryUriSizeBytes(contentResolver, uri) ?: 0L
            ImportStorageGuard.requireFreeSpace(modsDir, total, "import mission zip $displayName")
            contentResolver.openInputStream(uri)?.use { input ->
                FileOutputStream(temp).use { output ->
                    LauncherFileCopy.copyStream(
                        input,
                        output,
                        total,
                        "Copying level pack: $displayName",
                        onProgress = onProgress,
                    )
                }
            } ?: return null
            onProgress(LauncherCopyProgress("Inspecting level pack: $displayName", 0L, 0L))
            nestedRebirthChildSize(temp)?.let { childSize ->
                ImportStorageGuard.requireFreeSpace(modsDir, childSize, "extract mission zip $displayName")
            }
            return importMissionZipFile(temp, displayName, moveDirectSource = true, onProgress = onProgress)
        } catch (e: InsufficientStorageException) {
            Log.e(TAG, "Not enough space to import mission zip $displayName", e)
            ImportStorageGuard.recordFailure(filesDir, "Mission ZIP import failed for $displayName", e)
            throw e
        } catch (e: Exception) {
            Log.e(TAG, "Failed to import mission zip $displayName", e)
            ImportStorageGuard.recordFailure(filesDir, "Mission ZIP import failed for $displayName", e)
            return null
        } finally {
            temp.delete()
        }
    }

    internal fun importMissionZipFile(
        source: File,
        displayName: String = source.name,
    ): ModInfo? = importMissionZipFile(source, displayName, moveDirectSource = false)

    internal fun importMissionZipFileMovingSource(
        source: File,
        displayName: String = source.name,
    ): ModInfo? = importMissionZipFile(source, displayName, moveDirectSource = true)

    private fun importMissionZipFile(
        source: File,
        displayName: String,
        moveDirectSource: Boolean,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): ModInfo? {
        onProgress(LauncherCopyProgress("Inspecting level pack: $displayName", 0L, 0L))
        val scan = MissionZip.inspect(source)
        if (scan == null) return importNestedRebirthMissionZip(source, onProgress)
        val safeName = displayName.replace(Regex("[^a-zA-Z0-9._-]"), "_")
        val dest = File(modsDir, safeName)
        val extractionStore = MissionZipExtractionStore(filesDir)
        extractionStore.removeOwner(safeName)
        if (source.absoluteFile != dest.absoluteFile) {
            val moved = moveDirectSource && source.renameTo(dest)
            if (!moved) {
                ImportStorageGuard.requireFreeSpace(modsDir, source.length(), "import mission zip $displayName")
                source.copyTo(dest, overwrite = true)
            }
        }
        try {
            if (scan.importMode == "extracted_bundle") {
                val extractLabel = "Extracting level pack cache for faster launches: $displayName"
                onProgress(LauncherCopyProgress(extractLabel, 0L, 0L))
                extractionStore.ensureExtracted(safeName, dest, scan) { done, total, path ->
                    onProgress(LauncherCopyProgress(extractionProgressLabel(extractLabel, path), done, total))
                }
                onProgress(LauncherCopyProgress("Cached level pack for faster launches: $displayName", 1L, 1L))
            }
            if (scan.importMode != "extracted_bundle") {
                onProgress(LauncherCopyProgress("Finalizing level pack: $displayName", 0L, 0L))
            }
        } catch (e: Exception) {
            dest.delete()
            extractionStore.removeOwner(safeName)
            throw e
        }
        return registerMissionZip(safeName, dest.length(), scan)
    }

    private fun importNestedRebirthMissionZip(
        source: File,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ): ModInfo? =
        try {
            ZipFile(source).use { zip ->
                val child = selectNestedRebirthZip(zip) ?: return null
                val safeName = launcherLeafNameOf(child.name).replace(Regex("[^a-zA-Z0-9._-]"), "_")
                val dest = File(modsDir, safeName)
                ImportStorageGuard.requireFreeSpace(
                    modsDir,
                    child.size.coerceAtLeast(0),
                    "extract mission zip $safeName",
                )
                zip.getInputStream(child).use { input ->
                    FileOutputStream(dest).use { output ->
                        LauncherFileCopy.copyStream(
                            input,
                            output,
                            child.size.coerceAtLeast(0),
                            "Extracting Rebirth level pack from archive: $safeName",
                            onProgress = onProgress,
                        )
                    }
                }
                onProgress(LauncherCopyProgress("Inspecting level pack: $safeName", 0L, 0L))
                val scan =
                    MissionZip.inspect(dest)
                        ?: run {
                            dest.delete()
                            return null
                        }
                val extractionStore = MissionZipExtractionStore(filesDir)
                extractionStore.removeOwner(safeName)
                if (scan.importMode == "extracted_bundle") {
                    val extractLabel = "Extracting level pack cache for faster launches: $safeName"
                    onProgress(LauncherCopyProgress(extractLabel, 0L, 0L))
                    extractionStore.ensureExtracted(safeName, dest, scan) { done, total, path ->
                        onProgress(
                            LauncherCopyProgress(extractionProgressLabel(extractLabel, path), done, total),
                        )
                    }
                    onProgress(
                        LauncherCopyProgress("Cached level pack for faster launches: $safeName", 1L, 1L),
                    )
                }
                registerMissionZip(safeName, dest.length(), scan)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to import nested Rebirth mission ZIP from ${source.absolutePath}", e)
            null
        }

    private fun extractionProgressLabel(
        base: String,
        path: String,
    ): String {
        val leaf = path.replace('\\', '/').substringAfterLast('/').takeIf { it.isNotBlank() }
        return if (leaf == null) base else "$base ($leaf)"
    }

    private fun nestedRebirthChildSize(source: File): Long? =
        try {
            ZipFile(source).use { zip ->
                selectNestedRebirthZip(zip)?.size?.coerceAtLeast(0)
            }
        } catch (_: Exception) {
            null
        }

    private fun selectNestedRebirthZip(zip: ZipFile): java.util.zip.ZipEntry? {
        val rebirthChildren = mutableListOf<java.util.zip.ZipEntry>()
        val entries = zip.entries()
        while (entries.hasMoreElements()) {
            val entry = entries.nextElement()
            if (entry.isDirectory) continue
            val leaf = launcherLeafNameOf(entry.name)
            if (GameFileFormats.extensionOf(leaf) == "zip" && leaf.lowercase(Locale.US).contains("rebirth")) {
                rebirthChildren += entry
            }
        }
        return rebirthChildren.singleOrNull()
    }

    /** Reorder: swap item at [index] with the one above it */
    fun moveUp(index: Int) {
        updateManifest {
            val sorted = mods.sortedBy { it.order }.toMutableList()
            if (index <= 0 || index >= sorted.size) return@updateManifest
            val orderA = sorted[index - 1].order
            val orderB = sorted[index].order
            mods =
                mods
                    .map { m ->
                        when (m.filename) {
                            sorted[index].filename -> m.copy(order = orderA)
                            sorted[index - 1].filename -> m.copy(order = orderB)
                            else -> m
                        }
                    }.toMutableList()
            save()
        }
    }

    /** Reorder: swap item at [index] with the one below it */
    fun moveDown(index: Int) {
        updateManifest {
            val sorted = mods.sortedBy { it.order }.toMutableList()
            if (index < 0 || index >= sorted.size - 1) return@updateManifest
            val orderA = sorted[index].order
            val orderB = sorted[index + 1].order
            mods =
                mods
                    .map { m ->
                        when (m.filename) {
                            sorted[index].filename -> m.copy(order = orderB)
                            sorted[index + 1].filename -> m.copy(order = orderA)
                            else -> m
                        }
                    }.toMutableList()
            save()
        }
    }

    /**
     * Write .active_mod_paths for the given game (d1 or d2).
     * Called before game launch alongside writeActiveSetPath().
     */
    fun writeEnabledModPaths(
        game: String,
        includeD1MissionZipsForD2: Boolean = true,
    ) {
        val enabled =
            mods
                .filter { it.enabledForLaunch(game, includeD1MissionZipsForD2) }
                .sortedBy { it.order }
        val gameDir = if (game == "d1") "d1x-redux" else "d2x-redux"
        val pathFile = File(File(filesDir, gameDir), ".active_mod_paths")
        val generatedPatchDir = generatedPatchDir(game)
        val generatedMissionZipDir = generatedMissionZipDir(game)
        generatedPatchDir.deleteRecursively()
        generatedMissionZipDir.deleteRecursively()
        pathFile.parentFile?.mkdirs()
        if (enabled.isEmpty()) {
            pathFile.delete()
        } else {
            val validPaths = mutableListOf<String>()
            writeGeneratedPatchOverrides(game, enabled)?.let { validPaths += it.absolutePath }
            for (mod in enabled) {
                val modFile = File(modsDir, mod.filename)
                if (modFile.exists() && modFile.length() > 0) {
                    validPaths.addAll(activeModPathLines(game, mod, modFile))
                } else {
                    Log.e(TAG, "Mod file missing or empty: ${modFile.absolutePath} (${mod.displayName})")
                }
            }
            if (validPaths.isEmpty()) {
                pathFile.delete()
                Log.w(TAG, "All enabled mods missing on disk, removed .active_mod_paths")
            } else {
                try {
                    requireActiveModPathCapacity(validPaths)
                } catch (failure: ActiveModPathCapacityException) {
                    pathFile.delete()
                    throw failure
                }
                AtomicFilePublication.writeUtf8(pathFile, validPaths.joinToString("\n"))
            }
        }
        logInfo("Wrote ${enabled.size} mod paths for $game to ${pathFile.absolutePath}")
        NativeTextureLookupCache.clear()
    }

    private fun activeModPathLines(
        game: String,
        mod: ModInfo,
        modFile: File,
    ): List<String> =
        if (mod.kind == MOD_KIND_MISSION_ZIP) {
            generatedMissionZipPathLines(game, mod, modFile)
        } else {
            listOf(modFile.absolutePath)
        }

    private fun generatedMissionZipPathLines(
        game: String,
        mod: ModInfo,
        modFile: File,
    ): List<String> {
        val extractionStore = MissionZipExtractionStore(filesDir)
        if (mod.importMode == "extracted_bundle") {
            extractionStore.freshRecord(mod.filename, modFile)?.let { record ->
                writeMissionZipMusicNames(
                    mod.filename,
                    modFile,
                    record,
                    mod.displayName,
                )
                return extractionStore.activePathLines(mod.filename, modFile) ?: emptyList()
            }
        }
        val scan = MissionZip.inspect(modFile) ?: return emptyList()
        if (mod.importMode == "extracted_bundle" || scan.importMode == "extracted_bundle") {
            val record = extractionStore.ensureExtracted(mod.filename, modFile, scan)
            writeMissionZipMusicNames(mod.filename, modFile, record, mod.displayName)
            return extractionStore.activePathLines(mod.filename, modFile) ?: emptyList()
        }
        val stageDir = File(generatedMissionZipDir(game), safeMissionZipDirName(mod.filename))
        try {
            extractZipToRoot(modFile, stageDir, scan)
            writeMissionZipMusicNames(mod.filename, modFile, null, mod.displayName)
            copyMissionZipMusicNames(mod.filename, stageDir)
        } catch (e: InsufficientStorageException) {
            throw e
        } catch (e: Exception) {
            Log.e(TAG, "Could not stage mission zip ${modFile.absolutePath}: ${e.message ?: e.javaClass.simpleName}")
            return emptyList()
        }
        return buildList {
            add(stageDir.absolutePath)
            for (constituent in scan.constituents.filter { it.role == GameFileFormats.MISSION_ZIP_MOD_ARCHIVE }) {
                val archive =
                    File(
                        stageDir,
                        stagedRelativePath(scan, constituent.path).replace('/', File.separatorChar),
                    )
                if (archive.isFile) add(archive.absolutePath)
            }
        }
    }

    private fun writeMissionZipMusicNames(
        ownerFilename: String,
        modFile: File,
        extractionRecord: MissionZipExtractionRecord?,
        displayName: String,
        onProgress: (LauncherCopyProgress) -> Unit = {},
    ) {
        val appContext = context ?: return
        val catalog =
            try {
                extractionRecord?.let { MissionZipMusic.inspectExtracted(it) }
                    ?: MissionZipMusic.inspect(modFile)
                    ?: return
            } catch (e: Exception) {
                Log.w(TAG, "Could not inspect optional mission music for $displayName", e)
                return
            }
        val sidecar =
            extractionRecord?.let { File(it.rootDir, MISSION_ZIP_MUSIC_NAMES_FILE) }
                ?: MissionZipMusicNames.cacheFile(filesDir, ownerFilename)
        if (MissionZipMusicNames.isCurrent(sidecar, catalog)) return
        try {
            val count =
                MissionZipMusicNames.identifyLocalAndWrite(
                    appContext,
                    filesDir,
                    catalog,
                    sidecar,
                ) { done, total, track ->
                    if (total > 0) {
                        onProgress(
                            LauncherCopyProgress(
                                "Identifying music tracks: $displayName ($track)",
                                done.toLong(),
                                total.toLong(),
                            ),
                        )
                    }
                }
            if (count > 0) logInfo("Wrote $count mission music names for $displayName")
        } catch (e: Exception) {
            Log.w(TAG, "Could not identify mission zip music for $displayName", e)
        }
    }

    private fun copyMissionZipMusicNames(
        ownerFilename: String,
        stageDir: File,
    ) {
        val sidecar = MissionZipMusicNames.cacheFile(filesDir, ownerFilename)
        if (!sidecar.isFile) return
        sidecar.copyTo(File(stageDir, MISSION_ZIP_MUSIC_NAMES_FILE), overwrite = true)
    }

    fun checkEnabledModCompatibility(
        game: String,
        setDir: File,
        includeD1MissionZipsForD2: Boolean = true,
    ): ModCompatibilityReport {
        val enabled =
            mods
                .filter { it.enabledForLaunch(game, includeD1MissionZipsForD2) }
                .sortedBy { it.order }
        val assetEntries = AssetManifest(setDir).load().associateBy { it.filename.lowercase(Locale.US) }
        val failures = mutableListOf<ModCompatibilityFailure>()
        val patchDocuments = mutableListOf<ModPatchDocument>()
        for (mod in enabled) {
            val modFile = File(modsDir, mod.filename)
            if (!modFile.isFile) continue
            failures += checkModCompatibility(mod, modFile, game, setDir, assetEntries)
            patchDocuments += collectModPatchDocuments(mod, modFile, game)
        }
        val patchConflicts = collectPatchConflicts(patchDocuments)
        return ModCompatibilityReport(failures, patchConflicts)
    }

    fun enabledVisualReplacementSummary(
        game: String,
        includeD1MissionZipsForD2: Boolean = true,
    ): VisualReplacementSummary {
        val names = mutableListOf<String>()
        var omittedModCount = 0
        var textureCount = 0
        val enabled =
            mods
                .filter { it.enabledForLaunch(game, includeD1MissionZipsForD2) }
                .sortedBy { it.order }

        for (mod in enabled) {
            val modFile = File(modsDir, mod.filename)
            val count = countVisualReplacementAssets(modFile)
            if (count <= 0) continue
            omittedModCount++
            textureCount += count
            if (names.size < 3) names += mod.displayName
        }

        return VisualReplacementSummary(
            omittedModCount = omittedModCount,
            omittedTextureCount = textureCount,
            omittedModNames = names,
        )
    }

    fun hasEnabledMissionZipSoundtrack(game: String): Boolean =
        hasEnabledMissionZipSoundtrack(game, includeD1MissionZipsForD2 = true)

    fun hasEnabledMissionZipSoundtrack(
        game: String,
        includeD1MissionZipsForD2: Boolean,
    ): Boolean =
        mods
            .filter { it.enabledForLaunch(game, includeD1MissionZipsForD2) && it.kind == MOD_KIND_MISSION_ZIP }
            .any(::modHasMissionSoundtrack)

    private fun modHasMissionSoundtrack(mod: ModInfo): Boolean {
        val modFile = File(modsDir, mod.filename)
        val record = MissionZipExtractionStore(filesDir).freshRecord(mod.filename, modFile)
        return if (record != null) missionZipHasSoundtrack(record) else missionZipHasSoundtrack(modFile)
    }

    fun hasEnabledD1MissionZipForD2(): Boolean =
        mods.any { it.enabled && it.kind == MOD_KIND_MISSION_ZIP && it.game == "d1" }

    private fun ModInfo.enabledForLaunch(
        game: String,
        includeD1MissionZipsForD2: Boolean = true,
    ): Boolean =
        enabled &&
            (
                this.game == game ||
                    this.game == "both" ||
                    (
                        includeD1MissionZipsForD2 &&
                            game == "d2" &&
                            kind == MOD_KIND_MISSION_ZIP &&
                            this.game == "d1"
                    )
            )

    private fun registerMissionZip(
        filename: String,
        sizeBytes: Long,
        scan: MissionZip.ScanResult,
    ): ModInfo {
        mods.removeAll { it.filename == filename }
        val maxOrder = mods.maxOfOrNull { it.order } ?: -1
        val mod =
            ModInfo(
                filename = filename,
                displayName = scan.mission.displayName,
                enabled = true,
                addedAt = System.currentTimeMillis(),
                sizeBytes = sizeBytes,
                game = scan.game,
                order = maxOrder + 1,
                kind = MOD_KIND_MISSION_ZIP,
                category = scan.category,
                missionTitle = scan.mission.displayName,
                importMode = scan.importMode,
            )
        mods.add(mod)
        save()
        if (modHasMissionSoundtrack(mod)) {
            context?.let { selectBundledMusicForNewMission(filesDir, it, mod.game) }
        }
        logInfo("Imported mission zip: ${mod.displayName} (${mod.sizeBytes / 1024 / 1024} MB)")
        return mod
    }

    fun getModDetails(
        mod: ModInfo,
        setDir: File,
    ): ModDetails {
        val modFile = File(modsDir, mod.filename)
        if (!modFile.isFile) {
            return ModDetails(
                archivePath = modFile.absolutePath,
                fileCount = 0,
                archiveSizeBytes = 0,
                manifestSchema = null,
                notes = emptyList(),
                categories = emptyList(),
                patches = emptyList(),
                baseRequirements = emptyList(),
                problems = listOf("Mod archive is missing from storage"),
            )
        }
        if (mod.kind == MOD_KIND_MISSION_ZIP) return getMissionZipDetails(modFile)
        val assetEntries = AssetManifest(setDir).load().associateBy { it.filename.lowercase(Locale.US) }
        return try {
            ZipFile(modFile).use { zip ->
                val entries =
                    zip
                        .entries()
                        .asSequence()
                        .filterNot { it.isDirectory }
                        .toList()
                val manifest = readModManifest(zip)
                val rawRequirements = readBaseRequirements(manifest, game = null)
                val baseRequirements = attachBaseRequirementStatus(rawRequirements, setDir, assetEntries)
                val conflicts = collectPatchConflictsForMod(mod)
                val problems = mutableListOf<String>()
                if (manifest == null) {
                    problems += "No manifest metadata; compatibility cannot be preflighted"
                }
                for (requirement in baseRequirements.filter { !it.ok }) {
                    problems += describeBaseRequirementProblem(requirement)
                }
                for (conflict in conflicts) {
                    problems += describePatchConflictProblem(mod, conflict)
                }
                ModDetails(
                    archivePath = modFile.absolutePath,
                    fileCount = entries.size,
                    archiveSizeBytes = modFile.length(),
                    manifestSchema = manifest?.optString("schema")?.takeIf { it.isNotBlank() },
                    notes = readStringArray(manifest?.optJSONArray("notes")),
                    categories = categorizeModEntries(entries),
                    patches = readPatchDetails(zip, entries, rawRequirements),
                    baseRequirements = baseRequirements,
                    problems = problems,
                )
            }
        } catch (e: Exception) {
            ModDetails(
                archivePath = modFile.absolutePath,
                fileCount = 0,
                archiveSizeBytes = modFile.length(),
                manifestSchema = null,
                notes = emptyList(),
                categories = emptyList(),
                patches = emptyList(),
                baseRequirements = emptyList(),
                problems = listOf("Could not read DXA archive: ${e.message ?: e.javaClass.simpleName}"),
            )
        }
    }

    private fun getMissionZipDetails(modFile: File): ModDetails =
        try {
            val extractionStore = MissionZipExtractionStore(filesDir)
            var extractionRecord = extractionStore.freshRecord(modFile.name, modFile)
            var scan = extractionRecord?.let { MissionZip.inspectExtracted(it) } ?: MissionZip.inspect(modFile)
            val sourceScan = scan
            if (extractionRecord == null && sourceScan?.importMode == "extracted_bundle") {
                extractionRecord = extractionStore.ensureExtracted(modFile.name, modFile, sourceScan)
                scan = MissionZip.inspectExtracted(extractionRecord) ?: scan
            }
            if (scan == null) {
                ModDetails(
                    archivePath = modFile.absolutePath,
                    fileCount = 0,
                    archiveSizeBytes = modFile.length(),
                    manifestSchema = null,
                    notes = emptyList(),
                    categories = emptyList(),
                    patches = emptyList(),
                    baseRequirements = emptyList(),
                    problems = listOf("Could not find a mission descriptor inside this ZIP"),
                )
            } else {
                ModDetails(
                    archivePath = modFile.absolutePath,
                    fileCount = scan.constituents.size,
                    archiveSizeBytes = modFile.length(),
                    manifestSchema = null,
                    notes = missionZipFeatureNotes(modFile, scan, extractionRecord),
                    categories = missionZipCategories(scan.constituents),
                    patches = emptyList(),
                    baseRequirements = emptyList(),
                    problems = emptyList(),
                    missionZip = scan,
                    missionZipMusic =
                        extractionRecord?.let { MissionZipMusic.inspectExtracted(it) }
                            ?: MissionZipMusic.inspect(modFile),
                    missionZipExtraction = extractionRecord?.toMissionZipExtractionInfo(),
                )
            }
        } catch (e: Exception) {
            ModDetails(
                archivePath = modFile.absolutePath,
                fileCount = 0,
                archiveSizeBytes = modFile.length(),
                manifestSchema = null,
                notes = emptyList(),
                categories = emptyList(),
                patches = emptyList(),
                baseRequirements = emptyList(),
                problems = listOf("Could not read mission ZIP: ${e.message ?: e.javaClass.simpleName}"),
            )
        }

    private fun missionZipFeatureNotes(
        modFile: File,
        scan: MissionZip.ScanResult,
        extractionRecord: MissionZipExtractionRecord? = null,
    ): List<String> =
        buildList {
            if (scan.constituents.any { launcherExtensionOf(it.name) == "sng" }) {
                add("Includes a mission song list")
            }
            scan.constituents
                .filter { GameFileFormats.extensionOf(it.name) == "hog" }
                .forEach { constituent ->
                    val summary =
                        missionZipConstituentSummary(modFile, constituent, extractionRecord)
                            ?: return@forEach
                    if (summary.categories.isNotEmpty()) {
                        add(
                            "${constituent.name} contents: " +
                                summary.categories
                                    .take(8)
                                    .joinToString(", ") { "${it.count} ${it.label.lowercase(Locale.US)}" },
                        )
                    }
                    summary.notes.forEach { add("${constituent.name}: $it") }
                    summary.problems.forEach { add("${constituent.name}: $it") }
                }
            scan.constituents
                .filter { GameFileFormats.extensionOf(it.name) == "dxa" }
                .forEach { constituent ->
                    val summary =
                        missionZipConstituentSummary(modFile, constituent, extractionRecord)
                            ?: return@forEach
                    if (summary.categories.isNotEmpty()) {
                        add(
                            "${constituent.name} contents: " +
                                summary.categories
                                    .take(8)
                                    .joinToString(", ") { "${it.count} ${it.label.lowercase(Locale.US)}" },
                        )
                    }
                    summary.notes.forEach { add("${constituent.name}: $it") }
                    summary.problems.forEach { add("${constituent.name}: $it") }
                }
        }

    private fun missionZipConstituentSummary(
        modFile: File,
        constituent: MissionZip.Constituent,
        extractionRecord: MissionZipExtractionRecord?,
    ): GameFileMetadata.Summary? =
        if (extractionRecord != null) {
            MissionZipExtractionStore(filesDir).extractedSummaryForRecordEntry(extractionRecord, constituent.path)
        } else {
            GameFileMetadata.summarizeZipConstituent(modFile, constituent.path, constituent.name)
        }

    private fun MissionZipExtractionRecord.toMissionZipExtractionInfo(): MissionZipExtractionInfo =
        MissionZipExtractionInfo(
            rootPath = rootDir.absolutePath,
            sourceArchiveName = sourceArchiveName,
            archiveFormat = archiveFormat,
            ownerSizeBytes = ownerSizeBytes,
            extractedSizeBytes = extractedSizeBytes,
            fileCount = fileCount,
            importMode = importMode,
        )

    private fun collectModPatchDocuments(
        mod: ModInfo,
        modFile: File,
        game: String?,
    ): List<ModPatchDocument> {
        if (mod.kind == MOD_KIND_MISSION_ZIP && GameFileFormats.extensionOf(mod.filename) != "zip") return emptyList()
        if (mod.kind == MOD_KIND_MISSION_ZIP &&
            mod.importMode == "extracted_bundle" &&
            MissionZipExtractionStore(filesDir).freshRecord(mod.filename, modFile) != null
        ) {
            return emptyList()
        }
        try {
            ZipFile(modFile).use { zip ->
                val patchPaths = mutableSetOf<String>()
                val entriesByPath = mutableMapOf<String, java.util.zip.ZipEntry>()
                val manifest = readModManifest(zip)
                if (manifest != null) {
                    val manifestGame = manifest.optString("game", "both")
                    if (game == null || manifestGame == "both" || manifestGame == game) {
                        addManifestPatchPaths(manifest, game, patchPaths)
                    }
                }
                val entries = zip.entries()
                while (entries.hasMoreElements()) {
                    val zipEntry = entries.nextElement()
                    if (!zipEntry.isDirectory && isPatchEntry(zipEntry.name, game)) {
                        val patchPath = normalizeDxaPath(zipEntry.name)
                        patchPaths += patchPath
                        entriesByPath[patchPath] = zipEntry
                    }
                }
                return patchPaths.map { patchPath ->
                    val owner = PatchOwner(patchPath, mod.displayName, mod.filename)
                    val entry = entriesByPath[patchPath]
                    if (entry == null) {
                        ModPatchDocument(
                            owner = owner,
                            operations = null,
                            writes = emptyList(),
                            problem = "declares $patchPath but the archive entry is missing",
                        )
                    } else {
                        readPatchDocument(zip, entry, owner)
                    }
                }
            }
        } catch (e: Exception) {
            logWarning("Failed to read DXA patch metadata from ${modFile.name}: ${e.message}")
            return emptyList()
        }
    }

    private fun readPatchDocument(
        zip: ZipFile,
        entry: java.util.zip.ZipEntry,
        owner: PatchOwner,
    ): ModPatchDocument =
        try {
            val text = zip.getInputStream(entry).bufferedReader().use { it.readText() }
            val operations = JSONArray(text)
            ModPatchDocument(owner, operations, collectPatchWrites(owner, operations))
        } catch (e: Exception) {
            ModPatchDocument(
                owner = owner,
                operations = null,
                writes = emptyList(),
                problem = "could not read ${owner.patchPath}: ${e.message ?: e.javaClass.simpleName}",
            )
        }

    private fun collectPatchWrites(
        owner: PatchOwner,
        operations: JSONArray,
    ): List<PatchWrite> {
        val testsByPath = mutableMapOf<String, Any?>()
        val writes = mutableListOf<PatchWrite>()
        for (index in 0 until operations.length()) {
            val operation =
                operations.optJSONObject(index) ?: throw IllegalArgumentException("operation $index is not an object")
            val operationName = operation.optString("op")
            val path = operation.optString("path")
            if (path.isBlank()) throw IllegalArgumentException("operation $index is missing path")
            if (owner.patchPath == D2HamPatchSchema.PATH) D2HamPatchSchema.validate(operation)
            if (operationName == "test") {
                testsByPath[path] = operation.opt("value")
                continue
            }
            if (operationName != "add" && operationName != "replace" && operationName != "remove") {
                throw IllegalArgumentException("unsupported JSON Patch operation '$operationName'")
            }
            val scope = parsePatchScope(path) ?: throw IllegalArgumentException("unsupported patch path $path")
            val value = operation.opt("value")
            val testedValue = testsByPath[path]
            if (operationName == "replace" && scope.field == null && value is JSONObject && testedValue is JSONObject) {
                addChangedObjectFieldWrites(owner, scope, testedValue, value, writes)
            } else {
                writes += PatchWrite(owner, scope, canonicalJson(value))
            }
        }
        return writes
    }

    private fun addChangedObjectFieldWrites(
        owner: PatchOwner,
        rowScope: PatchScope,
        baseValue: JSONObject,
        patchValue: JSONObject,
        writes: MutableList<PatchWrite>,
    ) {
        val keys = linkedSetOf<String>()
        val baseKeys = baseValue.keys()
        while (baseKeys.hasNext()) keys += baseKeys.next()
        val patchKeys = patchValue.keys()
        while (patchKeys.hasNext()) keys += patchKeys.next()
        for (key in keys.sorted()) {
            val baseField = baseValue.opt(key)
            val patchField = patchValue.opt(key)
            if (canonicalJson(baseField) != canonicalJson(patchField)) {
                writes += PatchWrite(owner, rowScope.copy(field = key), canonicalJson(patchField))
            }
        }
    }

    private fun collectPatchConflicts(documents: List<ModPatchDocument>): List<ModPatchConflict> {
        val builders = linkedMapOf<String, PatchConflictBuilder>()
        for (document in documents) {
            val problem = document.problem ?: continue
            val builder = builders.getOrPut(document.owner.patchPath) { PatchConflictBuilder() }
            builder.names += document.owner.modDisplayName
            builder.details += "${document.owner.modDisplayName}: $problem"
        }
        val documentsByPath = documents.filter { it.problem == null }.groupBy { it.owner.patchPath }
        for ((patchPath, patchDocuments) in documentsByPath) {
            val writes = patchDocuments.flatMap { it.writes }
            for (leftIndex in writes.indices) {
                val left = writes[leftIndex]
                for (rightIndex in leftIndex + 1 until writes.size) {
                    val right = writes[rightIndex]
                    if (left.owner.modFilename == right.owner.modFilename) continue
                    if (!left.scope.overlaps(right.scope) || left.valueText == right.valueText) continue
                    val builder = builders.getOrPut(patchPath) { PatchConflictBuilder() }
                    builder.names += left.owner.modDisplayName
                    builder.names += right.owner.modDisplayName
                    builder.details +=
                        "${left.scope.displayPath}: ${left.owner.modDisplayName} and ${right.owner.modDisplayName} write different values"
                }
            }
        }
        return builders.map { (patchPath, builder) ->
            ModPatchConflict(
                patchPath = patchPath,
                modDisplayNames = builder.names.toList(),
                details = builder.details.toList(),
            )
        }
    }

    private fun generatedPatchDir(game: String): File {
        val gameDir = if (game == "d1") "d1x-redux" else "d2x-redux"
        return File(File(filesDir, gameDir), GENERATED_PATCH_DIR)
    }

    private fun generatedMissionZipDir(game: String): File {
        val gameDir = if (game == "d1") "d1x-redux" else "d2x-redux"
        return File(File(filesDir, gameDir), GENERATED_MISSION_ZIP_DIR)
    }

    private fun missionZipHasSoundtrack(modFile: File): Boolean =
        runCatching {
            ArchiveFiles.open(modFile).use { outer ->
                for (entry in outer.entries) {
                    if (entry.isDirectory) continue
                    when (launcherExtensionOf(entry.path)) {
                        "sng" -> {
                            outer.openInputStream(entry).use { input ->
                                if (songListReferencesMissionSoundtrack(input.readBytes().toString(Charsets.UTF_8))) {
                                    return@runCatching true
                                }
                            }
                        }

                        "dxa" -> {
                            outer.openInputStream(entry).use { input ->
                                if (dxaHasSoundtrack(ZipInputStream(input))) return@runCatching true
                            }
                        }

                        "hog" -> {
                            outer.openInputStream(entry).use { input ->
                                if (hogHasSoundtrack(input)) return@runCatching true
                            }
                        }

                        in MISSION_SOUNDTRACK_EXTENSIONS -> {
                            return@runCatching true
                        }
                    }
                }
            }
            false
        }.getOrDefault(false)

    private fun missionZipHasSoundtrack(record: MissionZipExtractionRecord): Boolean =
        runCatching {
            for (entry in record.files) {
                val file = File(record.rootDir, entry.relativePath.replace('/', File.separatorChar))
                if (!file.isFile) continue
                when (launcherExtensionOf(entry.relativePath)) {
                    "sng" -> {
                        if (songListReferencesMissionSoundtrack(file.readText(Charsets.UTF_8))) return@runCatching true
                    }

                    "dxa" -> {
                        file.inputStream().use { input ->
                            if (dxaHasSoundtrack(ZipInputStream(input))) return@runCatching true
                        }
                    }

                    "hog" -> {
                        file.inputStream().use { input ->
                            if (hogHasSoundtrack(input)) return@runCatching true
                        }
                    }

                    in MISSION_SOUNDTRACK_EXTENSIONS -> {
                        return@runCatching true
                    }
                }
            }
            false
        }.getOrDefault(false)

    private fun dxaHasSoundtrack(zip: ZipInputStream): Boolean =
        zip.use {
            var entry = it.nextEntry
            while (entry != null) {
                if (!entry.isDirectory) {
                    val leaf = launcherLeafNameOf(entry.name).lowercase(Locale.US)
                    if (leaf in MISSION_SONG_LIST_FILES) {
                        if (songListReferencesMissionSoundtrack(it.readBytes().toString(Charsets.UTF_8))) return true
                    } else if (launcherExtensionOf(leaf) in MISSION_SOUNDTRACK_EXTENSIONS) {
                        return true
                    }
                }
                it.closeEntry()
                entry = it.nextEntry
            }
            false
        }

    private fun songListReferencesMissionSoundtrack(text: String): Boolean =
        text
            .lineSequence()
            .map { it.trim().substringBefore(' ').substringBefore('\t') }
            .any { launcherExtensionOf(it) in MISSION_SOUNDTRACK_EXTENSIONS }

    private fun hogHasSoundtrack(input: InputStream): Boolean {
        val magic = input.readNBytesCompat(3)
        if (magic.toString(Charsets.US_ASCII) != "DHF") return false
        while (true) {
            val nameBytes = input.readNBytesCompat(13)
            if (nameBytes.isEmpty() || nameBytes.size != 13) return false
            val lenBytes = input.readNBytesCompat(4)
            if (lenBytes.size != 4) return false
            val name = hogEntryName(nameBytes)
            val size = leInt(lenBytes).toLong() and 0xffff_ffffL
            if (launcherExtensionOf(name) in MISSION_SOUNDTRACK_EXTENSIONS) return true
            input.skipFullyCompat(size)
        }
    }

    private fun InputStream.readNBytesCompat(count: Int): ByteArray {
        val out = ByteArray(count)
        var total = 0
        while (total < count) {
            val read = read(out, total, count - total)
            if (read < 0) break
            total += read
        }
        return if (total == count) out else out.copyOf(total)
    }

    private fun InputStream.skipFullyCompat(count: Long) {
        var remaining = count
        while (remaining > 0) {
            val skipped = skip(remaining)
            if (skipped > 0) {
                remaining -= skipped
            } else if (read() >= 0) {
                remaining--
            } else {
                return
            }
        }
    }

    private fun hogEntryName(nameBytes: ByteArray): String =
        nameBytes
            .takeWhile { it.toInt() != 0 }
            .toByteArray()
            .toString(Charsets.US_ASCII)
            .trim()

    private fun leInt(bytes: ByteArray): Int =
        (bytes[0].toInt() and 0xff) or
            ((bytes[1].toInt() and 0xff) shl 8) or
            ((bytes[2].toInt() and 0xff) shl 16) or
            ((bytes[3].toInt() and 0xff) shl 24)

    private fun launcherLeafNameOf(path: String): String = path.replace('\\', '/').substringAfterLast('/')

    private fun writeGeneratedPatchOverrides(
        game: String,
        enabled: List<ModInfo>,
    ): File? {
        val documents = mutableListOf<ModPatchDocument>()
        for (mod in enabled) {
            val modFile = File(modsDir, mod.filename)
            if (modFile.isFile) documents += collectModPatchDocuments(mod, modFile, game)
        }
        if (collectPatchConflicts(documents).isNotEmpty()) return null
        val root = generatedPatchDir(game)
        var wrotePatch = false
        for ((patchPath, patchDocuments) in documents.filter { it.operations != null }.groupBy { it.owner.patchPath }) {
            if (patchDocuments.map { it.owner.modFilename }.distinct().size <= 1) continue
            val merged = JSONArray()
            for (document in patchDocuments) {
                val operations = document.operations ?: continue
                for (index in 0 until operations.length()) merged.put(operations.get(index))
            }
            val output = File(root, patchPath.replace('/', File.separatorChar))
            output.parentFile?.mkdirs()
            output.writeText(merged.toString(2))
            wrotePatch = true
        }
        return if (wrotePatch) root else null
    }

    private fun parsePatchScope(path: String): PatchScope? {
        if (!path.startsWith("/")) return null
        val segments = path.split('/').drop(1).map { decodeJsonPointerSegment(it) }
        if (segments.size !in 3..4 || segments[0] != "sections") return null
        val index = segments[2].toIntOrNull() ?: return null
        val rowPath = "/sections/${segments[1]}/$index"
        return PatchScope(rowPath, segments.getOrNull(3))
    }

    private fun decodeJsonPointerSegment(text: String): String = text.replace("~1", "/").replace("~0", "~")

    private fun canonicalJson(value: Any?): String =
        when (value) {
            null, JSONObject.NULL -> {
                "null"
            }

            is JSONObject -> {
                val keys = mutableListOf<String>()
                val iterator = value.keys()
                while (iterator.hasNext()) keys += iterator.next()
                keys.sorted().joinToString(prefix = "{", postfix = "}") { key ->
                    JSONObject.quote(key) + ":" + canonicalJson(value.opt(key))
                }
            }

            is JSONArray -> {
                (0 until value.length()).joinToString(prefix = "[", postfix = "]") { index ->
                    canonicalJson(value.opt(index))
                }
            }

            is String -> {
                JSONObject.quote(value)
            }

            else -> {
                value.toString()
            }
        }

    private fun addManifestPatchPaths(
        manifest: JSONObject,
        game: String?,
        patchPaths: MutableSet<String>,
    ) {
        val compatibility = manifest.optJSONObject("compatibility") ?: return
        val requiredFiles = compatibility.optJSONArray("requiredBaseFiles") ?: return
        for (index in 0 until requiredFiles.length()) {
            val required = requiredFiles.optJSONObject(index) ?: continue
            val requiredGame = required.optString("game", game ?: "both")
            if (game != null && requiredGame != "both" && requiredGame != game) continue
            val paths = required.optJSONArray("patchPaths") ?: continue
            for (pathIndex in 0 until paths.length()) {
                val patchPath = normalizeDxaPath(paths.optString(pathIndex))
                if (patchPath.isNotBlank()) patchPaths += patchPath
            }
        }
    }

    private fun isPatchEntry(
        name: String,
        game: String?,
    ): Boolean {
        val normalized = normalizeDxaPath(name)
        if (!normalized.startsWith("patches/") || !normalized.endsWith(".rfc6902.json")) return false
        return game == null || normalized.startsWith("patches/$game/")
    }

    private fun normalizeDxaPath(path: String): String = path.replace('\\', '/').trim('/').lowercase(Locale.US)

    private fun checkModCompatibility(
        mod: ModInfo,
        modFile: File,
        game: String,
        setDir: File,
        assetEntries: Map<String, AssetManifest.AssetEntry>,
    ): List<ModCompatibilityFailure> {
        try {
            ZipFile(modFile).use { zip ->
                val entry = zip.getEntry("metadata/manifest.json") ?: return emptyList()
                val manifestText = zip.getInputStream(entry).bufferedReader().use { it.readText() }
                val manifest = JSONObject(manifestText)
                val manifestGame = manifest.optString("game", "both")
                if (manifestGame != "both" && manifestGame != game) return emptyList()
                val compatibility = manifest.optJSONObject("compatibility") ?: return emptyList()
                val description = compatibility.optString("requiredBaseDescription")
                val requiredFiles = compatibility.optJSONArray("requiredBaseFiles") ?: return emptyList()
                val failures = mutableListOf<ModCompatibilityFailure>()
                for (index in 0 until requiredFiles.length()) {
                    val required = requiredFiles.optJSONObject(index) ?: continue
                    if (!required.optBoolean("required", true)) continue
                    val requiredGame = required.optString("game", game)
                    if (requiredGame != "both" && requiredGame != game) continue
                    val filename = required.optString("filename").lowercase(Locale.US)
                    val expectedSha256 = required.optString("sha256").lowercase(Locale.US)
                    if (filename.isBlank() || expectedSha256.isBlank()) continue
                    val actual = findActualBaseFile(setDir, assetEntries, filename)
                    if (actual?.sha256 != expectedSha256) {
                        failures +=
                            ModCompatibilityFailure(
                                modDisplayName = mod.displayName,
                                requiredBaseDescription = description,
                                filename = filename,
                                expectedSha256 = expectedSha256,
                                expectedVersion = required.optString("version", "required version"),
                                actualSha256 = actual?.sha256,
                                actualVersion = actual?.versionName,
                                reason = required.optString("reason"),
                            )
                    }
                }
                return failures
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to read DXA compatibility metadata from ${modFile.name}: ${e.message}")
            return emptyList()
        }
    }

    private fun readModManifest(zip: ZipFile): JSONObject? {
        val entry = zip.getEntry("metadata/manifest.json") ?: return null
        val manifestText = zip.getInputStream(entry).bufferedReader().use { it.readText() }
        return JSONObject(manifestText)
    }

    private fun readBaseRequirements(
        manifest: JSONObject?,
        game: String?,
    ): List<RawBaseRequirement> {
        val compatibility = manifest?.optJSONObject("compatibility") ?: return emptyList()
        val requiredFiles = compatibility.optJSONArray("requiredBaseFiles") ?: return emptyList()
        val result = mutableListOf<RawBaseRequirement>()
        for (index in 0 until requiredFiles.length()) {
            val required = requiredFiles.optJSONObject(index) ?: continue
            val requiredGame = required.optString("game", game ?: "both")
            if (game != null && requiredGame != "both" && requiredGame != game) continue
            val filename = required.optString("filename").lowercase(Locale.US)
            val expectedSha256 = required.optString("sha256").lowercase(Locale.US)
            if (filename.isBlank() || expectedSha256.isBlank()) continue
            result +=
                RawBaseRequirement(
                    game = requiredGame,
                    role = required.optString("role"),
                    filename = filename,
                    expectedSha256 = expectedSha256,
                    expectedVersion = required.optString("version", "required version"),
                    required = required.optBoolean("required", true),
                    reason = required.optString("reason"),
                    patchPaths = readStringArray(required.optJSONArray("patchPaths")).map { normalizeDxaPath(it) },
                )
        }
        return result
    }

    private fun attachBaseRequirementStatus(
        requirements: List<RawBaseRequirement>,
        setDir: File,
        assetEntries: Map<String, AssetManifest.AssetEntry>,
    ): List<ModBaseRequirement> =
        requirements.map { requirement ->
            val actual = findActualBaseFile(setDir, assetEntries, requirement.filename)
            ModBaseRequirement(
                game = requirement.game,
                role = requirement.role,
                filename = requirement.filename,
                expectedSha256 = requirement.expectedSha256,
                expectedVersion = requirement.expectedVersion,
                actualSha256 = actual?.sha256,
                actualVersion = actual?.versionName,
                required = requirement.required,
                reason = requirement.reason,
                patchPaths = requirement.patchPaths,
            )
        }

    private fun readStringArray(array: JSONArray?): List<String> {
        if (array == null) return emptyList()
        val result = mutableListOf<String>()
        for (index in 0 until array.length()) {
            val text = array.optString(index)
            if (text.isNotBlank()) result += text
        }
        return result
    }

    private fun categorizeModEntries(entries: List<java.util.zip.ZipEntry>): List<ModFileCategorySummary> {
        val buckets = linkedMapOf<String, CategoryBucket>()
        for (entry in entries) {
            val (label, example) = classifyModEntry(entry.name)
            val bucket = buckets.getOrPut(label) { CategoryBucket(label) }
            bucket.count++
            if (entry.size > 0) bucket.sizeBytes += entry.size
            if (bucket.examples.size < 3 && example !in bucket.examples) bucket.examples += example
        }
        return buckets.values.map { bucket ->
            ModFileCategorySummary(
                label = bucket.label,
                count = bucket.count,
                sizeBytes = bucket.sizeBytes,
                examples = bucket.examples,
                examplesTruncated = bucket.count > bucket.examples.size,
            )
        }
    }

    private fun missionZipCategories(entries: List<MissionZip.Constituent>): List<ModFileCategorySummary> {
        val buckets = linkedMapOf<String, CategoryBucket>()
        for (entry in entries) {
            val label =
                when (entry.role) {
                    GameFileFormats.MISSION_ZIP_OTHER -> "Other files"
                    else -> GameFileFormats.missionZipRoleLabel(entry.role)
                }
            val bucket = buckets.getOrPut(label) { CategoryBucket(label) }
            bucket.count++
            bucket.sizeBytes += entry.sizeBytes
            val example = "${entry.name} - ${launcherFileTypeLabel(entry.name)}"
            if (bucket.examples.size < 3 && example !in bucket.examples) bucket.examples += example
        }
        return buckets.values.map { bucket ->
            ModFileCategorySummary(
                label = bucket.label,
                count = bucket.count,
                sizeBytes = bucket.sizeBytes,
                examples = bucket.examples,
                examplesTruncated = bucket.count > bucket.examples.size,
            )
        }
    }

    private fun classifyModEntry(name: String): Pair<String, String> {
        val normalized = normalizeDxaPath(name)
        val leaf = normalized.substringAfterLast('/')
        val exampleWithPurpose = "$leaf - ${launcherFileTypeLabel(leaf)}"
        val registryCategory = GameFileFormats.modCategoryLabel(leaf)
        return when {
            isPatchEntry(name, game = null) -> "Metadata patches" to normalized
            normalized.startsWith("metadata/") || normalized.startsWith("patches/") -> "Mod metadata" to normalized
            GameFileFormats.isTextureReplacement(leaf) -> "Texture replacements" to leaf
            GameFileFormats.isDocumentation(leaf) -> "Documentation" to leaf
            registryCategory != null -> registryCategory to exampleWithPurpose
            else -> "Other files" to leaf
        }
    }

    private fun countVisualReplacementAssets(file: File): Int {
        if (!file.isFile) return 0
        if (isVisualReplacementAsset(file.name)) return 1
        return try {
            var count = 0
            ZipFile(file).use { zip ->
                val entries = zip.entries()
                while (entries.hasMoreElements()) {
                    val entry = entries.nextElement()
                    if (!entry.isDirectory && isVisualReplacementAsset(entry.name)) count++
                }
            }
            count
        } catch (e: Exception) {
            Log.w(TAG, "Could not scan visual replacements in ${file.name}", e)
            0
        }
    }

    private fun isVisualReplacementAsset(path: String): Boolean {
        val leaf = launcherLeafNameOf(path).lowercase(Locale.US)
        return GameFileFormats.isTextureReplacement(leaf) ||
            leaf.endsWith(".ase") ||
            (leaf.startsWith("model") && leaf.endsWith(".bin"))
    }

    private fun readPatchDetails(
        zip: ZipFile,
        entries: List<java.util.zip.ZipEntry>,
        requirements: List<RawBaseRequirement>,
    ): List<ModPatchDetail> {
        val requirementByPatch =
            requirements
                .flatMap { requirement -> requirement.patchPaths.map { it to requirement } }
                .groupBy({ it.first }, { it.second })
        return entries
            .filter { isPatchEntry(it.name, game = null) }
            .sortedBy { normalizeDxaPath(it.name) }
            .map { entry ->
                val patchPath = normalizeDxaPath(entry.name)
                val patchRequirements = requirementByPatch[patchPath].orEmpty()
                ModPatchDetail(
                    path = patchPath,
                    sizeBytes = entry.size.coerceAtLeast(0),
                    operationCount = readPatchOperationCount(zip, entry),
                    affectedFiles = patchRequirements.map { it.filename }.distinct(),
                    expectedBaseVersions =
                        patchRequirements
                            .map {
                                it.expectedVersion.ifBlank {
                                    "sha256=${KnownVersions.shortHash(
                                        it.expectedSha256,
                                    )}"
                                }
                            }.distinct(),
                )
            }
    }

    private fun readPatchOperationCount(
        zip: ZipFile,
        entry: java.util.zip.ZipEntry,
    ): Int? =
        try {
            val text = zip.getInputStream(entry).bufferedReader().use { it.readText() }
            JSONArray(text).length()
        } catch (_: Exception) {
            null
        }

    private fun collectPatchConflictsForMod(mod: ModInfo): List<ModPatchConflict> {
        val documents = mutableListOf<ModPatchDocument>()
        for (candidate in mods.filter { it.enabled || it.filename == mod.filename }) {
            val candidateFile = File(modsDir, candidate.filename)
            if (!candidateFile.isFile) continue
            documents += collectModPatchDocuments(candidate, candidateFile, game = null)
        }
        return collectPatchConflicts(documents).filter { conflict -> mod.displayName in conflict.modDisplayNames }
    }

    private fun describeBaseRequirementProblem(requirement: ModBaseRequirement): String {
        if (requirement.actualSha256 == null) return "Missing base file: ${requirement.filename}"
        val actual = requirement.actualVersion ?: "unknown #${KnownVersions.shortHash(requirement.actualSha256)}"
        return "Base mismatch: ${requirement.filename} expected ${requirement.expectedVersion}, found $actual" +
            "\nExpected sha256=${requirement.expectedSha256}" +
            "\nActual sha256=${requirement.actualSha256}"
    }

    private fun describePatchConflictProblem(
        mod: ModInfo,
        conflict: ModPatchConflict,
    ): String {
        val otherMods = conflict.modDisplayNames.filter { it != mod.displayName }
        val header =
            if (otherMods.isEmpty()) {
                "Patch problem: ${conflict.patchPath}"
            } else {
                "Patch conflict: ${conflict.patchPath} also used by ${otherMods.joinToString(", ")}"
            }
        if (conflict.details.isEmpty()) return header
        return header + "\n" + conflict.details.joinToString("\n")
    }

    private fun findActualBaseFile(
        setDir: File,
        assetEntries: Map<String, AssetManifest.AssetEntry>,
        filename: String,
    ): ActualBaseFile? {
        val lower = filename.lowercase(Locale.US)
        val diskFile = setDir.listFiles()?.firstOrNull { it.name.equals(lower, ignoreCase = true) }
        val manifestEntry = assetEntries[lower]
        if (manifestEntry != null && (diskFile == null || diskFile.length() == manifestEntry.sizeBytes)) {
            return ActualBaseFile(
                manifestEntry.sha256.lowercase(Locale.US),
                manifestEntry.versionName,
            )
        }
        if (diskFile?.isFile != true) return null
        val sha256 = computeSha256(diskFile) ?: return null
        return ActualBaseFile(sha256, KnownVersions.lookup(lower, sha256))
    }

    private fun computeSha256(file: File): String? {
        try {
            val digest = MessageDigest.getInstance("SHA-256")
            val buffer = ByteArray(8192)
            FileInputStream(file).use { input ->
                while (true) {
                    val read = input.read(buffer)
                    if (read <= 0) break
                    digest.update(buffer, 0, read)
                }
            }
            return digest.digest().joinToString("") { "%02x".format(it) }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to hash ${file.absolutePath}", e)
            return null
        }
    }

    private fun detectGame(filename: String): String = GameFileFormats.gameHint(filename)

    private fun generateDisplayName(filename: String): String =
        stripLauncherDxaSuffix(filename)
            .replace(Regex("[_-]"), " ")
            .split(" ")
            .joinToString(" ") { word ->
                if (word.length <= 2) word.uppercase() else word.replaceFirstChar { it.uppercase() }
            }

    private fun load() {
        if (!manifestFile.exists()) {
            mods = mutableListOf()
            return
        }
        try {
            val json = JSONObject(manifestFile.readText())
            val arr = json.getJSONArray("mods")
            mods =
                (0 until arr.length())
                    .map { i ->
                        val obj = arr.getJSONObject(i)
                        val filename = obj.getString("filename")
                        var size = obj.optLong("sizeBytes", 0)
                        if (size == 0L) {
                            val f = File(modsDir, filename)
                            if (f.exists()) size = f.length()
                        }
                        ModInfo(
                            filename = filename,
                            displayName = obj.getString("displayName"),
                            enabled = obj.optBoolean("enabled", true),
                            addedAt = obj.optLong("addedAt", 0),
                            sizeBytes = size,
                            game = obj.optString("game", "both"),
                            order = obj.optInt("order", i),
                            kind = obj.optString("kind", MOD_KIND_DXA),
                            category = obj.optString("category").takeIf { it.isNotBlank() },
                            missionTitle = obj.optString("missionTitle").takeIf { it.isNotBlank() },
                            importMode = obj.optString("importMode").takeIf { it.isNotBlank() },
                        )
                    }.toMutableList()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to load mod manifest: ${e.message}")
            mods = mutableListOf()
        }
    }

    private fun save() {
        val arr = JSONArray()
        for (m in mods) {
            arr.put(
                JSONObject().apply {
                    put("filename", m.filename)
                    put("displayName", m.displayName)
                    put("enabled", m.enabled)
                    put("addedAt", m.addedAt)
                    put("sizeBytes", m.sizeBytes)
                    put("game", m.game)
                    put("order", m.order)
                    put("kind", m.kind)
                    m.category?.let { put("category", it) }
                    m.missionTitle?.let { put("missionTitle", it) }
                    m.importMode?.let { put("importMode", it) }
                },
            )
        }
        AtomicFilePublication.writeUtf8(manifestFile, JSONObject().put("mods", arr).toString(2))
    }

    private fun <T> updateManifest(block: () -> T): T =
        AtomicFilePublication.transaction {
            load()
            block()
        }

    private fun logInfo(message: String) {
        try {
            Log.i(TAG, message)
        } catch (_: RuntimeException) {
            // Android Log is not available in local JVM tests
        }
    }

    private fun logWarning(message: String) {
        try {
            Log.w(TAG, message)
        } catch (_: RuntimeException) {
            // Android Log is not available in local JVM tests
        }
    }
}
