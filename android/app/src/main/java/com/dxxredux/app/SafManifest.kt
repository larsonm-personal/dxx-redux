package com.dxxredux.app

import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Manages `.saf_manifest.json` — the metadata file that maps game filenames
 * to SAF content URIs for the leave-in-place system.
 *
 * The C-side PhysFS archiver (`physfs_archiver_saf.c`) parses this same file
 * to know which URIs to open via JNI when the engine requests a game file.
 */
class SafManifest(
    private val manifestFile: File,
) {
    data class SafFileEntry(
        val filename: String,
        val contentUri: String,
        val sizeBytes: Long,
    )

    fun read(): List<SafFileEntry> {
        if (!manifestFile.exists()) return emptyList()
        return try {
            val root = JSONObject(manifestFile.readText())
            val arr = root.optJSONArray("files") ?: return emptyList()
            (0 until arr.length()).map { i ->
                val obj = arr.getJSONObject(i)
                SafFileEntry(
                    filename = obj.getString("filename"),
                    contentUri = obj.getString("content_uri"),
                    sizeBytes = obj.getLong("size_bytes"),
                )
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse ${manifestFile.name}", e)
            emptyList()
        }
    }

    fun write(entries: List<SafFileEntry>) {
        val arr = JSONArray()
        for (entry in entries) {
            arr.put(
                JSONObject().apply {
                    put("filename", entry.filename)
                    put("content_uri", entry.contentUri)
                    put("size_bytes", entry.sizeBytes)
                },
            )
        }
        val root = JSONObject()
        root.put("files", arr)
        manifestFile.writeText(root.toString(2))
    }

    fun addOrReplace(entry: SafFileEntry) {
        val entries = read().toMutableList()
        val lower = entry.filename.lowercase()
        val replaced = SafFileEntry(lower, entry.contentUri, entry.sizeBytes)
        val idx = entries.indexOfFirst { it.filename == lower }
        if (idx >= 0) {
            entries[idx] = replaced
        } else {
            entries.add(replaced)
        }
        write(entries)
    }

    fun remove(filename: String) {
        val entries = read().toMutableList()
        entries.removeAll { it.filename == filename.lowercase() }
        write(entries)
    }

    fun isEmpty(): Boolean = read().isEmpty()

    /**
     * Remove entries whose content URIs are no longer readable.
     * Returns list of pruned filenames for user notification.
     */
    fun pruneStaleEntries(context: android.content.Context): List<String> {
        val entries = read()
        val stale =
            entries.filter { entry ->
                try {
                    val uri = android.net.Uri.parse(entry.contentUri)
                    context.contentResolver.openInputStream(uri)?.close()
                    false // accessible
                } catch (_: Exception) {
                    true // stale
                }
            }
        if (stale.isEmpty()) return emptyList()
        val pruned = stale.map { it.filename }
        write(entries.filterNot { e -> stale.any { it.filename == e.filename } })
        for (name in pruned) {
            Log.i(TAG, "Pruned stale SAF entry: $name")
        }
        return pruned
    }

    companion object {
        private const val TAG = "SafManifest"
        const val FILENAME = ".saf_manifest.json"

        /** Convenience: create a SafManifest for the default location in filesDir. */
        fun forDir(dir: File) = SafManifest(File(dir, FILENAME))
    }
}
