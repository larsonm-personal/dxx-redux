package com.dxxredux.app

import android.content.ContentResolver
import android.net.Uri
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.security.MessageDigest
import java.util.Locale
import java.util.zip.ZipFile

/**
 * Manages .dxa mod files: import, enable/disable, reorder, delete.
 *
 * Mods are stored in filesDir/mods/ with metadata in mod_manifest.json.
 * Before game launch, [writeEnabledModPaths] writes .active_mod_paths
 * which the C engine reads in physfsx.c to mount enabled mods via PhysFS.
 */
class ModManager(
    private val filesDir: File,
) {
    companion object {
        private const val TAG = "DXX-Mods"
        private const val MANIFEST_FILE = "mod_manifest.json"
    }

    data class ModInfo(
        val filename: String,
        val displayName: String,
        val enabled: Boolean,
        val addedAt: Long,
        val sizeBytes: Long,
        val game: String, // "d1", "d2", or "both"
        val order: Int,
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

    data class ModCompatibilityReport(
        val failures: List<ModCompatibilityFailure>,
    ) {
        val ok: Boolean get() = failures.isEmpty()

        fun toUserMessage(): String {
            if (ok) return ""
            return buildString {
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
        }

        fun toLogMessage(): String = toUserMessage().replace('\n', ' ')
    }

    private data class ActualBaseFile(
        val sha256: String,
        val versionName: String?,
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
        mods =
            mods
                .map {
                    if (it.filename == filename) it.copy(enabled = enabled) else it
                }.toMutableList()
        save()
    }

    fun deleteMod(filename: String) {
        File(modsDir, filename).delete()
        mods.removeAll { it.filename == filename }
        save()
        Log.i(TAG, "Deleted mod: $filename")
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
                        onProgress,
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
                enabled = true,
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
                enabled = true,
                addedAt = System.currentTimeMillis(),
                sizeBytes = sizeBytes,
                game = game,
                order = maxOrder + 1,
            ),
        )
        save()
        Log.i(TAG, "Registered downloaded mod: $displayName")
    }

    /** Reorder: swap item at [index] with the one above it */
    fun moveUp(index: Int) {
        val sorted = mods.sortedBy { it.order }.toMutableList()
        if (index <= 0 || index >= sorted.size) return
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

    /** Reorder: swap item at [index] with the one below it */
    fun moveDown(index: Int) {
        val sorted = mods.sortedBy { it.order }.toMutableList()
        if (index < 0 || index >= sorted.size - 1) return
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

    /**
     * Write .active_mod_paths for the given game (d1 or d2).
     * Called before game launch alongside writeActiveSetPath().
     */
    fun writeEnabledModPaths(game: String) {
        val enabled =
            mods
                .filter { it.enabled && (it.game == game || it.game == "both") }
                .sortedBy { it.order }
        val gameDir = if (game == "d1") "d1x-redux" else "d2x-redux"
        val pathFile = File(File(filesDir, gameDir), ".active_mod_paths")
        pathFile.parentFile?.mkdirs()
        if (enabled.isEmpty()) {
            pathFile.delete()
        } else {
            val validPaths = mutableListOf<String>()
            for (mod in enabled) {
                val modFile = File(modsDir, mod.filename)
                if (modFile.exists() && modFile.length() > 0) {
                    validPaths.add(modFile.absolutePath)
                } else {
                    Log.e(TAG, "Mod file missing or empty: ${modFile.absolutePath} (${mod.displayName})")
                }
            }
            if (validPaths.isEmpty()) {
                pathFile.delete()
                Log.w(TAG, "All enabled mods missing on disk, removed .active_mod_paths")
            } else {
                pathFile.writeText(validPaths.joinToString("\n"))
            }
        }
        Log.i(TAG, "Wrote ${enabled.size} mod paths for $game to ${pathFile.absolutePath}")
    }

    fun checkEnabledModCompatibility(
        game: String,
        setDir: File,
    ): ModCompatibilityReport {
        val enabled =
            mods
                .filter { it.enabled && (it.game == game || it.game == "both") }
                .sortedBy { it.order }
        val assetEntries = AssetManifest(setDir).load().associateBy { it.filename.lowercase(Locale.US) }
        val failures = mutableListOf<ModCompatibilityFailure>()
        for (mod in enabled) {
            val modFile = File(modsDir, mod.filename)
            if (!modFile.isFile) continue
            failures += checkModCompatibility(mod, modFile, game, setDir, assetEntries)
        }
        return ModCompatibilityReport(failures)
    }

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

    private fun findActualBaseFile(
        setDir: File,
        assetEntries: Map<String, AssetManifest.AssetEntry>,
        filename: String,
    ): ActualBaseFile? {
        val lower = filename.lowercase(Locale.US)
        val diskFile = setDir.listFiles()?.firstOrNull { it.name.equals(lower, ignoreCase = true) }
        val manifestEntry = assetEntries[lower]
        if (manifestEntry != null && (diskFile == null || diskFile.length() == manifestEntry.sizeBytes)) {
            return ActualBaseFile(manifestEntry.sha256.lowercase(Locale.US), manifestEntry.versionName)
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

    private fun detectGame(filename: String): String {
        val lower = filename.lowercase()
        return when {
            lower.contains("d1") && !lower.contains("d2") -> "d1"
            lower.contains("d2") && !lower.contains("d1") -> "d2"
            else -> "both"
        }
    }

    private fun generateDisplayName(filename: String): String =
        filename
            .removeSuffix(".dxa")
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
                },
            )
        }
        manifestFile.writeText(JSONObject().put("mods", arr).toString(2))
    }
}
