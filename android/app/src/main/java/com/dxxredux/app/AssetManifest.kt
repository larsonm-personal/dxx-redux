package com.dxxredux.app

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileInputStream
import java.security.MessageDigest

/**
 * Manages `assets.json` — a manifest of imported game files with SHA-256 hashes
 * and version identification.
 *
 * The manifest lives in [filesDir] alongside the game files themselves.
 * It is a simple JSON array, human-readable and easy to inspect via adb.
 */
class AssetManifest(
    private val filesDir: File,
) {
    data class AssetEntry(
        val filename: String,
        val sha256: String,
        val sizeBytes: Long,
        val importedAt: Long,
        val versionName: String?,
        // non-null = externally picked (forgettable)
        val sourceUri: String? = null,
    ) {
        /** Last 8 hex chars of SHA-256 for UI display when version is unknown. */
        val shortHash: String get() = KnownVersions.shortHash(sha256)

        /** Display string: version name if known, otherwise short hash. */
        val versionDisplay: String get() = versionName ?: "#$shortHash"

        /** True when this file was picked from outside the data dir. */
        val isExternal: Boolean get() = sourceUri != null
    }

    private val manifestFile get() = File(filesDir, "assets.json")

    private fun findDiskFile(filename: String): File? =
        filesDir.listFiles()?.firstOrNull { it.name.equals(filename, ignoreCase = true) }

    private fun findCanonicalDiskFile(filename: String): File? =
        filesDir
            .listFiles()
            ?.filter { it.name.equals(filename, ignoreCase = true) }
            ?.takeIf { it.isNotEmpty() }
            ?.let { canonicalDiskFile(it) }

    private fun canonicalDiskFile(files: List<File>): File =
        files.firstOrNull { it.name == it.name.lowercase() }
            ?: files.sortedBy { it.name }.first()

    private fun readEntriesFromDisk(): List<AssetEntry> {
        val file = manifestFile
        if (!file.exists()) return emptyList()
        val json = JSONArray(file.readText())
        return (0 until json.length()).map { i ->
            val obj = json.getJSONObject(i)
            AssetEntry(
                filename = obj.getString("filename").lowercase(),
                sha256 = obj.getString("sha256").lowercase(),
                sizeBytes = obj.getLong("sizeBytes"),
                importedAt = obj.getLong("importedAt"),
                versionName = obj.optString("versionName").takeIf { it.isNotEmpty() },
                sourceUri = obj.optString("sourceUri").takeIf { it.isNotEmpty() },
            )
        }
    }

    private fun canonicalize(entries: List<AssetEntry>): List<AssetEntry> {
        val grouped =
            entries
                .map {
                    it.copy(
                        filename = it.filename.lowercase(),
                        sha256 = it.sha256.lowercase(),
                    )
                }.groupBy { it.filename }

        return grouped
            .map { (filename, duplicates) ->
                val diskFile = findCanonicalDiskFile(filename)
                val candidates =
                    if (diskFile != null) {
                        duplicates.filter { it.sizeBytes == diskFile.length() }.ifEmpty { duplicates }
                    } else {
                        duplicates
                    }
                val picked = candidates.maxBy { it.importedAt }
                val versionName =
                    picked.versionName ?: duplicates
                        .firstOrNull {
                            it.sha256.equals(picked.sha256, ignoreCase = true) && it.versionName != null
                        }?.versionName
                picked.copy(versionName = versionName)
            }.sortedBy { it.filename }
    }

    /**
     * Load the manifest from disk. Returns empty list if file doesn't exist or is corrupt.
     */
    fun load(): List<AssetEntry> =
        try {
            canonicalize(readEntriesFromDisk())
        } catch (e: Exception) {
            Log.e("AssetManifest", "Failed to parse assets.json", e)
            emptyList()
        }

    /**
     * Save the manifest to disk.
     */
    fun save(entries: List<AssetEntry>) {
        val json = JSONArray()
        for (entry in canonicalize(entries)) {
            val obj = JSONObject()
            obj.put("filename", entry.filename)
            obj.put("sha256", entry.sha256)
            obj.put("sizeBytes", entry.sizeBytes)
            obj.put("importedAt", entry.importedAt)
            if (entry.versionName != null) {
                obj.put("versionName", entry.versionName)
            }
            if (entry.sourceUri != null) {
                obj.put("sourceUri", entry.sourceUri)
            }
            json.put(obj)
        }
        manifestFile.writeText(json.toString(2))
    }

    /**
     * Insert or update an entry for [filename]. Automatically looks up the version
     * from [KnownVersions]. Returns the new/updated entry.
     */
    fun upsert(
        filename: String,
        sha256: String,
        sizeBytes: Long,
        sourceUri: String? = null,
    ): AssetEntry {
        val entries = load().toMutableList()
        val lowerName = filename.lowercase()
        val versionName = KnownVersions.lookup(lowerName, sha256)
        val now = System.currentTimeMillis()

        val entry = AssetEntry(lowerName, sha256.lowercase(), sizeBytes, now, versionName, sourceUri)
        entries.removeAll { it.filename == lowerName }
        entries.add(entry)

        save(entries)
        return entry
    }

    /**
     * Look up a manifest entry by filename (case-insensitive).
     */
    fun getEntry(filename: String): AssetEntry? = load().firstOrNull { it.filename == filename.lowercase() }

    /**
     * Remove a manifest entry by filename (case-insensitive).
     */
    fun remove(filename: String) {
        val entries = load().toMutableList()
        entries.removeAll { it.filename == filename.lowercase() }
        save(entries)
    }

    /**
     * Remove manifest entries whose files no longer exist on disk.
     * Returns list of pruned filenames for user notification.
     */
    fun pruneStaleEntries(): List<String> {
        val entries = load()
        val stale = entries.filter { findDiskFile(it.filename) == null }
        if (stale.isEmpty()) return emptyList()
        LauncherDebugLog.log(
            "asset-manifest prune start manifest=${manifestFile.absolutePath} stale_count=${stale.size}",
        )
        for (entry in stale.sortedBy { it.filename }) {
            val file = findDiskFile(entry.filename) ?: File(filesDir, entry.filename)
            LauncherDebugLog.log(
                "asset-manifest stale filename=${entry.filename} path=${file.absolutePath} exists=${file.exists()} manifest_size=${entry.sizeBytes} source_uri=${entry.sourceUri ?: "-"} version=${entry.versionName ?: "-"}",
            )
        }
        val pruned = stale.map { it.filename }
        val kept = entries.filterNot { findDiskFile(it.filename) == null }
        save(kept)
        LauncherDebugLog.log(
            "asset-manifest prune complete manifest=${manifestFile.absolutePath} kept_count=${kept.size}",
        )
        for (name in pruned) {
            Log.i("AssetManifest", "Pruned stale entry: $name")
        }
        return pruned
    }

    /**
     * Find files on disk that have no manifest entry or whose size has changed,
     * meaning they need (re-)hashing.
     */
    fun findStaleFiles(gameFilenames: Set<String>): List<File> {
        val entries = load().associateBy { it.filename }
        val diskFiles = filesDir.listFiles() ?: return emptyList()
        return diskFiles
            .filter { it.name.lowercase() in gameFilenames }
            .groupBy { it.name.lowercase() }
            .toSortedMap()
            .mapNotNull { (lower, variants) ->
                val file = canonicalDiskFile(variants)
                val entry = entries[lower]
                if (entry == null || entry.sizeBytes != file.length()) file else null
            }
    }

    companion object {
        /**
         * Compute SHA-256 of a file with streaming progress callback.
         * Runs on [Dispatchers.IO].
         *
         * @param onProgress called with (bytesRead, totalBytes) for progress UI
         */
        suspend fun computeSha256(
            file: File,
            onProgress: ((bytesRead: Long, totalBytes: Long) -> Unit)? = null,
        ): String? =
            withContext(Dispatchers.IO) {
                try {
                    val digest = MessageDigest.getInstance("SHA-256")
                    val totalBytes = file.length()
                    var bytesRead = 0L
                    val buffer = ByteArray(8192)

                    FileInputStream(file).use { input ->
                        while (true) {
                            val n = input.read(buffer)
                            if (n <= 0) break
                            digest.update(buffer, 0, n)
                            bytesRead += n
                            onProgress?.invoke(bytesRead, totalBytes)
                        }
                    }

                    digest.digest().joinToString("") { "%02x".format(it) }
                } catch (e: Exception) {
                    Log.w("AssetManifest", "Failed to hash ${file.absolutePath}", e)
                    null
                }
            }
    }
}
