package com.dxxredux.app

import android.content.Context
import android.util.Log
import org.json.JSONObject

/**
 * SHA-256 → version-name table for identifying known game asset versions.
 *
 * Loads from known_versions.json5 in APK assets. Call [init] once from
 * an Activity before first use (e.g. in SetupActivity.onCreate).
 */
object KnownVersions {
    data class VersionEntry(
        val sha256: String,
        val versionName: String,
    )

    /** filename (lowercase) → list of known versions */
    private var table: Map<String, List<VersionEntry>> = emptyMap()
    private var packages: List<String> = emptyList()

    fun init(context: Context) {
        if (table.isNotEmpty()) return
        try {
            val raw =
                context.assets
                    .open("known_versions.json5")
                    .bufferedReader()
                    .readText()
            val json = JSONObject(Json5.strip(raw))
            val arr = json.getJSONArray("versions")
            val t = mutableMapOf<String, MutableList<VersionEntry>>()
            val pkgs = mutableSetOf<String>()
            for (i in 0 until arr.length()) {
                val obj = arr.getJSONObject(i)
                val file = obj.getString("file").lowercase()
                val sha = obj.getString("sha256").lowercase()
                val ver = obj.getString("version")
                t.getOrPut(file) { mutableListOf() }.add(VersionEntry(sha, ver))
                pkgs.add(ver)
            }
            table = t
            packages = pkgs.toList()
        } catch (e: Exception) {
            Log.e("KnownVersions", "Failed to load known_versions.json5: ${e.message}")
        }
    }

    /** All known distribution package names. */
    val KNOWN_PACKAGES: List<String> get() = packages

    /**
     * Look up a version name for a file by its SHA-256 hash.
     * Returns null if the hash is not in the known table.
     */
    fun lookup(
        filename: String,
        sha256: String,
    ): String? {
        val entries = table[filename.lowercase()] ?: return null
        return entries.firstOrNull { it.sha256 == sha256.lowercase() }?.versionName
    }

    /**
     * Given a set of filename → sha256 pairs, identify the distribution package.
     * Returns the most common version name among matched files, or null if none match.
     */
    fun identifyPackage(files: Map<String, String>): String? {
        val votes = mutableMapOf<String, Int>()
        for ((filename, sha256) in files) {
            val version = lookup(filename, sha256)
            if (version != null) {
                votes[version] = (votes[version] ?: 0) + 1
            }
        }
        return votes.maxByOrNull { it.value }?.key
    }

    /**
     * Return short display hash (last 8 hex chars) for an unknown version.
     */
    fun shortHash(sha256: String): String = sha256.takeLast(8)
}
