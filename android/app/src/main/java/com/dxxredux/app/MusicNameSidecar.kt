package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import java.util.Locale

internal object MusicNameSidecar {
    const val VERSION = 1
    const val MAX_RECORDS = 4096
    const val MAX_PATHS_PER_RECORD = 4
    const val MAX_ALIASES_PER_RECORD = 4
    const val MAX_PATH_BYTES = 1024
    const val MAX_NAME_BYTES = 512
    const val MAX_FILE_BYTES = 512 * 1024

    data class Record(
        val paths: List<String>,
        val aliases: List<String>,
        val name: String,
    )

    fun encode(
        records: List<Record>,
        sourceIdentity: String? = null,
    ): String {
        require(records.size <= MAX_RECORDS) { "Music name sidecar exceeds $MAX_RECORDS records" }
        val exactPaths = mutableSetOf<String>()
        val encodedRecords = JSONArray()
        for (record in records) {
            val paths = normalizedDistinct(record.paths)
            val aliases = normalizedDistinct(record.aliases)
            require(paths.isNotEmpty() && paths.size <= MAX_PATHS_PER_RECORD) { "Invalid music path count" }
            require(aliases.size <= MAX_ALIASES_PER_RECORD) { "Invalid music alias count" }
            require(record.name.isNotBlank() && record.name.toByteArray().size <= MAX_NAME_BYTES) {
                "Invalid music display name"
            }
            for (path in paths + aliases) {
                require(path.toByteArray().size <= MAX_PATH_BYTES) { "Music path exceeds $MAX_PATH_BYTES bytes" }
            }
            for (path in paths) {
                require(exactPaths.add(path.lowercase(Locale.US))) { "Duplicate exact music path: $path" }
            }
            encodedRecords.put(
                JSONObject()
                    .put("paths", JSONArray(paths))
                    .put("aliases", JSONArray(aliases))
                    .put("name", record.name),
            )
        }
        val root =
            JSONObject()
                .put("version", VERSION)
                .put("records", encodedRecords)
        sourceIdentity?.let { root.put("sourceIdentity", it) }
        val text = root.toString(2)
        require(text.toByteArray().size <= MAX_FILE_BYTES) { "Music name sidecar exceeds $MAX_FILE_BYTES bytes" }
        return text
    }

    private fun normalizedDistinct(values: List<String>): List<String> =
        values
            .map { it.replace('\\', '/').trimEnd('/') }
            .filter { it.isNotBlank() }
            .distinctBy { it.lowercase(Locale.US) }
}
