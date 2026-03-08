package com.dxxredux.app

import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Manages multiple file sets — named collections of game data files.
 *
 * All sets (including "default") live in `filesDir/sets/<name>/`.
 * The root [filesDir] holds only configs, saves, music, and metadata.
 *
 * Each set can have its own `.saf_manifest.json` for leave-in-place files,
 * plus any locally copied files in its directory.
 *
 * Persistence is via `file_sets.json` in [filesDir].
 *
 * Call [migrateDefaultSetIfNeeded] once at startup to move legacy game data
 * from filesDir root into `sets/default/`.
 */
class FileSetManager(private val filesDir: File) {

    data class FileSetInfo(
        val name: String,
        val createdAt: Long,
        val source: String? = null
    )

    private val configFile get() = File(filesDir, "file_sets.json")
    private val setsDir get() = File(filesDir, "sets")

    /**
     * List all known sets. Always includes "default" even if file_sets.json
     * doesn't exist yet.
     */
    fun listSets(): List<FileSetInfo> {
        val config = loadConfig()
        val sets = config.optJSONArray("sets") ?: JSONArray()
        val result = mutableListOf<FileSetInfo>()
        for (i in 0 until sets.length()) {
            val obj = sets.getJSONObject(i)
            result.add(FileSetInfo(
                name = obj.getString("name"),
                createdAt = obj.optLong("createdAt", 0),
                source = obj.optString("source").takeIf { it.isNotEmpty() }
            ))
        }
        if (result.none { it.name == DEFAULT_SET }) {
            result.add(0, FileSetInfo(DEFAULT_SET, 0))
        }
        return result
    }

    /**
     * Get the name of the currently active set.
     */
    fun getActive(): String {
        val config = loadConfig()
        return config.optString("active", DEFAULT_SET).ifEmpty { DEFAULT_SET }
    }

    /**
     * Set the active file set. The name must refer to an existing set.
     */
    fun setActive(name: String) {
        val config = loadConfig()
        val sets = config.optJSONArray("sets") ?: JSONArray()
        val exists = (0 until sets.length()).any {
            sets.getJSONObject(it).getString("name") == name
        } || name == DEFAULT_SET
        if (!exists) {
            Log.w(TAG, "Cannot activate unknown set: $name")
            return
        }
        config.put("active", name)
        saveConfig(config)
    }

    /**
     * Create a new named set. Returns the set's directory.
     * Throws if the name is invalid or already exists.
     */
    fun createSet(name: String, source: String? = null): File {
        validateSetName(name)
        val config = loadConfig()
        val sets = config.optJSONArray("sets") ?: JSONArray()
        for (i in 0 until sets.length()) {
            if (sets.getJSONObject(i).getString("name").equals(name, ignoreCase = true)) {
                throw IllegalArgumentException("Set '$name' already exists")
            }
        }
        val dir = getSetDir(name)
        dir.mkdirs()
        val obj = JSONObject()
        obj.put("name", name)
        obj.put("createdAt", System.currentTimeMillis())
        if (source != null) obj.put("source", source)
        sets.put(obj)
        config.put("sets", sets)
        saveConfig(config)
        return dir
    }

    /**
     * Delete a named set. Cannot delete "default".
     * If the deleted set was active, switches to "default".
     */
    fun deleteSet(name: String) {
        if (name == DEFAULT_SET) {
            Log.w(TAG, "Cannot delete default set")
            return
        }
        val config = loadConfig()
        val sets = config.optJSONArray("sets") ?: return
        val newSets = JSONArray()
        for (i in 0 until sets.length()) {
            val obj = sets.getJSONObject(i)
            if (obj.getString("name") != name) newSets.put(obj)
        }
        config.put("sets", newSets)
        if (config.optString("active") == name) {
            config.put("active", DEFAULT_SET)
        }
        saveConfig(config)
        val dir = File(setsDir, name)
        if (dir.exists()) dir.deleteRecursively()
    }

    /**
     * Get the directory for a set. All sets live under `filesDir/sets/<name>/`,
     * including "default".
     */
    fun getSetDir(name: String): File {
        return File(setsDir, name).also { it.mkdirs() }
    }

    /**
     * Get the SAF manifest for a set.
     */
    fun safManifestForSet(name: String): SafManifest {
        return SafManifest.forDir(getSetDir(name))
    }

    /**
     * Calculate disk usage for a set's directory (excludes SAF leave-in-place files).
     */
    fun diskUsage(name: String): Long {
        val dir = getSetDir(name)
        if (!dir.exists()) return 0
        return dir.walkTopDown()
            .filter { it.isFile }
            .sumOf { it.length() }
    }

    /**
     * Write `.active_set_path` so the C engine can find the active set at init.
     * Call this before launching the game engine.
     */
    fun writeActiveSetPath() {
        val aspFile = File(filesDir, ".active_set_path")
        aspFile.writeText(getSetDir(getActive()).absolutePath)
    }

    private fun validateSetName(name: String) {
        require(name.isNotEmpty()) { "Set name cannot be empty" }
        require(name != "." && name != "..") { "Invalid set name" }
        require(!name.contains('/') && !name.contains('\\')) { "Set name cannot contain path separators" }
        require(name.length <= 50) { "Set name too long (max 50 chars)" }
        require(name != DEFAULT_SET) { "Cannot create set named '$DEFAULT_SET'" }
    }

    private fun loadConfig(): JSONObject {
        return try {
            if (configFile.exists()) JSONObject(configFile.readText()) else JSONObject()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse file_sets.json", e)
            JSONObject()
        }
    }

    private fun saveConfig(config: JSONObject) {
        configFile.writeText(config.toString(2))
    }

    /**
     * Migrate game data files from filesDir root into sets/default/.
     * Called once at startup. Idempotent — skips if already migrated.
     *
     * Moves files by extension (case-insensitive) and known game data
     * subdirectories (e.g. missions/). Music (.gog/.inst) and
     * configs/saves/metadata stay in filesDir.
     */
    fun migrateDefaultSetIfNeeded() {
        val defaultDir = getSetDir(DEFAULT_SET) // creates dir via mkdirs()

        val config = loadConfig()
        if (config.optInt("migration_version", 0) >= 1) return

        var movedCount = 0
        val files = filesDir.listFiles() ?: emptyArray()
        for (file in files) {
            if (file.isDirectory) {
                if (file.name.lowercase() in GAME_DATA_DIRS) {
                    val dest = File(defaultDir, file.name)
                    if (!dest.exists() && file.renameTo(dest)) movedCount++
                }
                continue
            }
            val ext = file.extension.lowercase()
            if (ext in GAME_DATA_EXTENSIONS) {
                val dest = File(defaultDir, file.name)
                if (!dest.exists() && file.renameTo(dest)) movedCount++
            }
        }

        // Move per-set manifests to default set dir
        for (name in listOf(".asset_manifest.json", ".saf_manifest.json")) {
            val src = File(filesDir, name)
            if (src.exists()) {
                val dest = File(defaultDir, name)
                if (!dest.exists()) src.renameTo(dest)
            }
        }

        config.put("migration_version", 1)
        saveConfig(config)
        Log.i(TAG, "Default-set migration: moved $movedCount items to ${defaultDir.absolutePath}")
    }

    companion object {
        private const val TAG = "FileSetManager"
        const val DEFAULT_SET = "default"

        /** File extensions (lowercase) that are per-set game data. */
        private val GAME_DATA_EXTENSIONS = setOf(
            "pig", "hog", "ham", "mvl", "s11", "s22",
            "mn2", "msn", "dxa", "pog", "rl2", "dtx",
        )

        /** Subdirectory names (lowercase) that contain per-set game data. */
        private val GAME_DATA_DIRS = setOf("missions")
    }
}
