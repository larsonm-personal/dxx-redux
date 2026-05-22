package com.dxxredux.app

import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/**
 * Manages multiple file sets — named collections of game data files.
 *
 * All sets (including "default") live in `<importRoot>/sets/<name>/`.
 * The root [filesDir] holds only configs, saves, music, and metadata.
 *
 * The import root is provided by [ImportLocationManager] and may live on
 * a different volume (SD card / USB OTG) when the user has opted in.
 *
 * Each set can have its own `.saf_manifest.json` for leave-in-place files,
 * plus any locally copied files in its directory.
 *
 * Persistence is via `file_sets.json` in [filesDir].
 *
 * Call [migrateDefaultSetIfNeeded] once at startup to move legacy game data
 * from filesDir root into `sets/default/`.
 */
class FileSetManager(
    private val filesDir: File,
    private val importRoot: File = ImportLocationManager(filesDir).getActiveRoot(),
) {
    data class FileSetInfo(
        val name: String,
        val createdAt: Long,
        val source: String? = null,
    )

    private val configFile get() = File(filesDir, "file_sets.json")
    private val setsDir get() = File(importRoot, "sets").also { it.mkdirs() }

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
            result.add(
                FileSetInfo(
                    name = obj.getString("name"),
                    createdAt = obj.optLong("createdAt", 0),
                    source = obj.optString("source").takeIf { it.isNotEmpty() },
                ),
            )
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
        val exists =
            (0 until sets.length()).any {
                sets.getJSONObject(it).getString("name") == name
            } ||
                name == DEFAULT_SET
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
    fun createSet(
        name: String,
        source: String? = null,
    ): File {
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
        NativeTextureLookupCache.clear()
    }

    /**
     * Get the directory for a set. All sets live under `filesDir/sets/<name>/`,
     * including "default".
     */
    fun getSetDir(name: String): File = File(setsDir, name).also { it.mkdirs() }

    /**
     * Get the SAF manifest for a set.
     */
    fun safManifestForSet(name: String): SafManifest = SafManifest.forDir(getSetDir(name))

    /**
     * Calculate disk usage for a set's directory (excludes SAF leave-in-place files).
     */
    fun diskUsage(name: String): Long {
        val dir = getSetDir(name)
        if (!dir.exists()) return 0
        return dir
            .walkTopDown()
            .filter { it.isFile }
            .sumOf { it.length() }
    }

    /**
     * Write `.active_set_path` so the C engine can find the active set at init.
     * The C code reads from `<pref_dir>/.active_set_path` where pref_dir is
     * `filesDir/d2x-redux/` or `filesDir/d1x-redux/`.  Write to both so
     * whichever game is launched finds the file.
     */
    fun writeActiveSetPath() {
        val path = getSetDir(getActive()).absolutePath
        for (game in arrayOf("d2x-redux", "d1x-redux")) {
            val dir = File(filesDir, game)
            dir.mkdirs()
            File(dir, ".active_set_path").writeText(path)
        }
        NativeTextureLookupCache.clear()
    }

    private fun validateSetName(name: String) {
        require(name.isNotEmpty()) { "Set name cannot be empty" }
        require(name != "." && name != "..") { "Invalid set name" }
        require(!name.contains('/') && !name.contains('\\')) { "Set name cannot contain path separators" }
        require(name.length <= 50) { "Set name too long (max 50 chars)" }
        require(name != DEFAULT_SET) { "Cannot create set named '$DEFAULT_SET'" }
    }

    private fun loadConfig(): JSONObject =
        try {
            if (configFile.exists()) JSONObject(configFile.readText()) else JSONObject()
        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse file_sets.json", e)
            JSONObject()
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
        val config = loadConfig()
        val currentVersion = config.optInt("migration_version", 0)

        // v0 -> v1: move legacy game-data files from filesDir root into
        // <importRoot>/sets/default/.  Pre-v1 the sets/ dir was at
        // filesDir/sets/, but importRoot defaults to filesDir/imported so
        // the destination already lives in the right place.
        if (currentVersion < 1) {
            val defaultDir = getSetDir(DEFAULT_SET) // creates dir via mkdirs()
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

        // v1 -> v2: when sets/ used to live at filesDir/sets/ and now lives
        // at importRoot/sets/, relocate it.  This is a no-op when importRoot
        // is filesDir (overlap) or when the legacy dir was already moved.
        if (config.optInt("migration_version", 0) < 2) {
            val legacy = File(filesDir, "sets")
            val target = File(importRoot, "sets")
            if (legacy.exists() && legacy.absolutePath != target.absolutePath) {
                target.parentFile?.mkdirs()
                if (!target.exists() && legacy.renameTo(target)) {
                    Log.i(TAG, "Relocated sets/ from ${legacy.absolutePath} to ${target.absolutePath}")
                } else if (target.exists()) {
                    // Both exist -- merge legacy entries that aren't already present.
                    var merged = 0
                    for (child in legacy.listFiles() ?: emptyArray()) {
                        val dest = File(target, child.name)
                        if (!dest.exists() && child.renameTo(dest)) merged++
                    }
                    Log.i(TAG, "Merged $merged legacy set dir(s) into ${target.absolutePath}")
                    if ((legacy.listFiles()?.size ?: 0) == 0) legacy.delete()
                }
            }
            config.put("migration_version", 2)
            saveConfig(config)
        }
    }

    /**
     * Remove any game-data files still sitting in filesDir root.
     * After the one-time migration, these are either duplicates of files
     * already in a set dir, or orphans left by adb push / incomplete
     * migration.  Either way they must not remain in filesDir because it
     * is always on the PhysFS search path and would leak data into every
     * set.  Runs every startup (cheap — just a dir listing).
     */
    fun sweepRootGameFiles() {
        val config = loadConfig()
        if (config.optInt("migration_version", 0) < 1) return // migration hasn't run yet
        var swept = 0
        val files = filesDir.listFiles() ?: return
        for (file in files) {
            if (file.isDirectory) {
                if (file.name.lowercase() in GAME_DATA_DIRS) {
                    file.deleteRecursively()
                    swept++
                }
                continue
            }
            if (file.extension.lowercase() in GAME_DATA_EXTENSIONS) {
                file.delete()
                swept++
            }
        }
        if (swept > 0) {
            Log.i(TAG, "Swept $swept orphaned game-data items from filesDir root")
        }
    }

    /**
     * Migrate pilot files from game pref-dir roots into Players/ subdirs.
     * With SysUsePlayersDir=1 the engine looks in Players/ exclusively.
     * Runs every startup (cheap dir listing, idempotent).
     */
    fun migratePilotFiles() {
        val sets = listSets()
        for (set in sets) {
            val setDir = getSetDir(set.name)
            for (game in arrayOf("d1x-redux", "d2x-redux")) {
                val gameDir = File(setDir, game)
                if (!gameDir.isDirectory) continue
                val playersDir = File(gameDir, "Players")
                val moved = movePilotFilesInto(gameDir, playersDir)
                if (moved > 0) {
                    Log.i(TAG, "Migrated $moved pilot file(s) from ${gameDir.name}/ to Players/ in set ${set.name}")
                }
            }
        }
        // Also sweep any stray pilot files at filesDir root into d2x-redux/Players/
        val defaultSet = getSetDir(DEFAULT_SET)
        val fallbackDir = File(File(defaultSet, "d2x-redux"), "Players")
        val swept = movePilotFilesInto(filesDir, fallbackDir)
        if (swept > 0) {
            Log.i(TAG, "Swept $swept stray pilot file(s) from filesDir root to d2x-redux/Players/")
        }
    }

    private fun movePilotFilesInto(
        srcDir: File,
        destDir: File,
    ): Int {
        val files = srcDir.listFiles() ?: return 0
        var count = 0
        for (file in files) {
            if (!file.isFile) continue
            if (file.extension.lowercase() !in PILOT_FILE_EXTENSIONS) continue
            destDir.mkdirs()
            val dest = File(destDir, file.name)
            if (!dest.exists() && file.renameTo(dest)) count++
        }
        return count
    }

    /**
     * Delete all player/pilot files from both game dirs across all sets,
     * plus legacy locations in filesDir root from before d1/d2 separation.
     */
    fun deleteAllPilotFiles(): Int {
        var total = 0
        val sets = listSets()
        for (set in sets) {
            val setDir = getSetDir(set.name)
            for (game in arrayOf("d1x-redux", "d2x-redux")) {
                val gameDir = File(setDir, game)
                if (!gameDir.isDirectory) continue
                total += deletePilotFilesIn(gameDir)
                val playersDir = File(gameDir, "Players")
                if (playersDir.isDirectory) total += deletePilotFilesIn(playersDir)
            }
        }
        // Legacy locations from before d1/d2 separation
        total += deletePilotFilesIn(filesDir)
        for (sub in arrayOf(
            "Players",
            "d1x-redux",
            "d2x-redux",
            "d1x-redux/Players",
            "d2x-redux/Players",
        )) {
            val dir = File(filesDir, sub)
            if (dir.isDirectory) total += deletePilotFilesIn(dir)
        }
        return total
    }

    private fun deletePilotFilesIn(dir: File): Int {
        val files = dir.listFiles() ?: return 0
        var count = 0
        for (file in files) {
            if (!file.isFile) continue
            if (file.extension.lowercase() in PILOT_FILE_EXTENSIONS) {
                if (file.delete()) count++
            }
        }
        return count
    }

    /**
     * Clear all files in a set without removing the set entry.
     * Works for default and non-default sets. Recreates the empty directory.
     */
    fun clearSet(name: String) {
        val dir = File(setsDir, name)
        if (dir.exists()) dir.deleteRecursively()
        dir.mkdirs()
        NativeTextureLookupCache.clear()
        Log.i(TAG, "Cleared set '$name'")
    }

    /**
     * Clear imported game data from every set while preserving pilot files,
     * saved games, and control/config files that live outside set storage.
     * Non-default sets that no longer contain preserved player data are
     * removed from the set list.
     */
    fun clearAllGameDataPreservingPlayers(): Int {
        val currentSets = listSets()
        val retainedSets = mutableListOf<FileSetInfo>()
        var activeSetName = getActive()

        for (set in currentSets) {
            val setDir = File(setsDir, set.name)
            val hasPlayerData = clearSetDirectoryPreservingPlayers(setDir)
            val keepSet = set.name == DEFAULT_SET || hasPlayerData

            if (keepSet) {
                if (set.name != DEFAULT_SET) retainedSets += set
            } else {
                if (activeSetName == set.name) activeSetName = DEFAULT_SET
                if (setDir.exists()) setDir.deleteRecursively()
            }
        }

        clearRootGameDataArtifacts()

        val config = loadConfig()
        val newSets = JSONArray()
        for (set in retainedSets) {
            newSets.put(
                JSONObject().apply {
                    put("name", set.name)
                    put("createdAt", set.createdAt)
                    if (set.source != null) put("source", set.source)
                },
            )
        }
        config.put("sets", newSets)
        config.put("active", activeSetName.ifEmpty { DEFAULT_SET })
        saveConfig(config)
        writeActiveSetPath()
        NativeTextureLookupCache.clear()
        Log.i(TAG, "Cleared game data from ${currentSets.size} set(s)")
        return currentSets.size
    }

    private fun clearSetDirectoryPreservingPlayers(dir: File): Boolean {
        if (!dir.exists()) return false

        var hasPlayerData = false
        val files = dir.listFiles() ?: return false
        for (file in files) {
            if (file.isDirectory) {
                if (clearSetDirectoryPreservingPlayers(file)) {
                    hasPlayerData = true
                } else if (file.exists()) {
                    file.deleteRecursively()
                }
                continue
            }

            if (file.extension.lowercase() in PILOT_FILE_EXTENSIONS) {
                hasPlayerData = true
            } else {
                file.delete()
            }
        }
        return hasPlayerData
    }

    private fun clearRootGameDataArtifacts() {
        val files = filesDir.listFiles() ?: return
        for (file in files) {
            if (file.isDirectory) {
                if (file.name.lowercase() in GAME_DATA_DIRS) {
                    file.deleteRecursively()
                }
                continue
            }

            val name = file.name.lowercase()
            if (file.extension.lowercase() in GAME_DATA_EXTENSIONS ||
                name == ".asset_manifest.json" ||
                name == ".saf_manifest.json"
            ) {
                file.delete()
            }
        }
    }

    companion object {
        private const val TAG = "FileSetManager"
        const val DEFAULT_SET = "default"

        /** File extensions (lowercase) that are per-set game data. */
        private val GAME_DATA_EXTENSIONS =
            setOf(
                "pig",
                "hog",
                "ham",
                "mvl",
                "s11",
                "s22",
                "mn2",
                "msn",
                "dxa",
                "pog",
                "rl2",
                "dtx",
            )

        /** Subdirectory names (lowercase) that contain per-set game data. */
        private val GAME_DATA_DIRS = setOf("missions")

        /** File extensions (lowercase) for pilot/player files. */
        private val PILOT_FILE_EXTENSIONS =
            buildSet {
                addAll(listOf("plr", "plx", "eff", "ngp"))
                // Saved games: .sg0-.sg9, .mg0-.mg9
                for (i in 0..9) {
                    add("sg$i")
                    add("mg$i")
                }
            }
    }
}
