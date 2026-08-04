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

    fun read(): List<SafFileEntry> =
        AtomicFilePublication.transaction {
            try {
                readStrictLocked()
            } catch (e: Exception) {
                runCatching { Log.e(TAG, "Failed to parse ${manifestFile.name}", e) }
                emptyList()
            }
        }

    private fun readStrictLocked(): List<SafFileEntry> {
        if (!manifestFile.exists()) return emptyList()
        return run {
            val root = JSONObject(manifestFile.readText())
            require(root.keys().asSequence().toSet() == setOf(ROOT_FILES))
            val arr = root.getJSONArray(ROOT_FILES)
            val filenames = mutableSetOf<String>()
            (0 until arr.length()).map { i ->
                val obj = arr.getJSONObject(i)
                require(obj.keys().asSequence().toSet() == ENTRY_FIELDS)
                val filename = obj.getString(FIELD_FILENAME)
                val contentUri = obj.getString(FIELD_CONTENT_URI)
                val rawSize = obj.get(FIELD_SIZE_BYTES)
                require(rawSize is Byte || rawSize is Short || rawSize is Int || rawSize is Long)
                val entry =
                    SafFileEntry(
                        filename = filename,
                        contentUri = contentUri,
                        sizeBytes = (rawSize as Number).toLong(),
                    )
                validateEntry(entry)
                require(filenames.add(filename.lowercaseSafName())) { "Duplicate SAF filename: $filename" }
                entry
            }
        }
    }

    fun write(entries: List<SafFileEntry>) =
        AtomicFilePublication.transaction {
            writeLocked(entries)
        }

    private fun writeLocked(entries: List<SafFileEntry>) {
        val filenames = mutableSetOf<String>()
        entries.forEach { entry ->
            validateEntry(entry)
            require(filenames.add(entry.filename.lowercaseSafName())) {
                "Duplicate SAF filename: ${entry.filename}"
            }
        }
        val arr = JSONArray()
        for (entry in entries) {
            arr.put(
                JSONObject().apply {
                    put(FIELD_FILENAME, entry.filename)
                    put(FIELD_CONTENT_URI, entry.contentUri)
                    put(FIELD_SIZE_BYTES, entry.sizeBytes)
                },
            )
        }
        val root = JSONObject()
        root.put(ROOT_FILES, arr)
        AtomicFilePublication.writeUtf8(manifestFile, root.toString(2))
    }

    fun addOrReplace(entry: SafFileEntry) =
        AtomicFilePublication.transaction {
            val entries = readStrictLocked().toMutableList()
            val lower = entry.filename.lowercaseSafName()
            val replaced = SafFileEntry(lower, entry.contentUri, entry.sizeBytes)
            val idx = entries.indexOfFirst { it.filename.lowercaseSafName() == lower }
            if (idx >= 0) {
                entries[idx] = replaced
            } else {
                entries.add(replaced)
            }
            writeLocked(entries)
        }

    fun remove(filename: String) =
        AtomicFilePublication.transaction {
            val entries = readStrictLocked().toMutableList()
            val lower = filename.lowercaseSafName()
            entries.removeAll { it.filename.lowercaseSafName() == lower }
            writeLocked(entries)
        }

    fun isEmpty(): Boolean = read().isEmpty()

    private fun validateEntry(entry: SafFileEntry) {
        require(entry.filename.isNotEmpty()) { "SAF filename is empty" }
        require(entry.contentUri.isNotEmpty()) { "SAF content URI is empty" }
        require('\u0000' !in entry.filename) { "SAF filename contains a null character" }
        require('\u0000' !in entry.contentUri) { "SAF content URI contains a null character" }
        require(entry.sizeBytes >= 0) { "SAF size is negative" }
    }

    private fun String.lowercaseSafName(): String =
        map { character ->
            if (character in 'A'..'Z') character.lowercaseChar() else character
        }.joinToString("")

    /**
     * Remove entries whose content URIs are no longer readable.
     * Returns list of pruned filenames for user notification.
     */
    fun pruneStaleEntries(context: android.content.Context): List<String> =
        AtomicFilePublication.transaction {
            val entries = readStrictLocked()
            val stale = mutableListOf<SafFileEntry>()
            val staleReasons = linkedMapOf<String, String>()
            for (entry in entries) {
                try {
                    val uri = android.net.Uri.parse(entry.contentUri)
                    context.contentResolver.openInputStream(uri)?.close()
                } catch (e: Exception) {
                    stale += entry
                    staleReasons[entry.filename] =
                        "${e::class.java.simpleName}:${e.message ?: "<no-message>"}"
                }
            }
            if (stale.isEmpty()) return@transaction emptyList()
            LauncherDebugLog.log(
                "saf-manifest prune start manifest=${manifestFile.absolutePath} stale_count=${stale.size}",
            )
            for (entry in stale.sortedBy { it.filename }) {
                LauncherDebugLog.log(
                    "saf-manifest stale filename=${entry.filename} uri=${entry.contentUri} size=${entry.sizeBytes} reason=${staleReasons[entry.filename] ?: "unknown"}",
                )
            }
            val pruned = stale.map { it.filename }
            writeLocked(entries.filterNot { e -> stale.any { it.filename == e.filename } })
            LauncherDebugLog.log(
                "saf-manifest prune complete manifest=${manifestFile.absolutePath} kept_count=${entries.size - stale.size}",
            )
            for (name in pruned) {
                Log.i(TAG, "Pruned stale SAF entry: $name")
            }
            pruned
        }

    companion object {
        private const val TAG = "SafManifest"
        private const val ROOT_FILES = "files"
        private const val FIELD_FILENAME = "filename"
        private const val FIELD_CONTENT_URI = "content_uri"
        private const val FIELD_SIZE_BYTES = "size_bytes"
        private val ENTRY_FIELDS = setOf(FIELD_FILENAME, FIELD_CONTENT_URI, FIELD_SIZE_BYTES)
        const val FILENAME = ".saf_manifest.json"

        /** Convenience: create a SafManifest for the default location in filesDir. */
        fun forDir(dir: File) = SafManifest(File(dir, FILENAME))
    }
}
