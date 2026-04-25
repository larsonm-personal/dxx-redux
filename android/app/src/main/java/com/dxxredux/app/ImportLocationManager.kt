package com.dxxredux.app

import android.content.Context
import android.os.Environment
import android.os.StatFs
import android.util.Log
import java.io.File

// Internal log helpers tolerate JVM unit tests where android.util.Log throws.
private const val TAG = "ImportLocationManager"

private fun logI(msg: String) {
    try {
        Log.i(TAG, msg)
    } catch (_: Throwable) {
        println("[INFO] $TAG: $msg")
    }
}

private fun logW(
    msg: String,
    e: Throwable? = null,
) {
    try {
        if (e != null) Log.w(TAG, msg, e) else Log.w(TAG, msg)
    } catch (_: Throwable) {
        System.err.println("[WARN] $TAG: $msg${e?.let { " -- $it" } ?: ""}")
    }
}

private fun logE(
    msg: String,
    e: Throwable? = null,
) {
    try {
        if (e != null) Log.e(TAG, msg, e) else Log.e(TAG, msg)
    } catch (_: Throwable) {
        System.err.println("[ERROR] $TAG: $msg${e?.let { " -- $it" } ?: ""}")
    }
}

/** Returns (availableBytes, totalBytes) for [dir].  Falls back to
 *  java.io.File.usableSpace when StatFs is unavailable (JVM unit tests). */
private fun statBytes(dir: File): Pair<Long, Long> =
    try {
        val s = StatFs(dir.absolutePath)
        s.availableBytes to s.totalBytes
    } catch (_: Throwable) {
        try {
            dir.usableSpace to dir.totalSpace
        } catch (_: Throwable) {
            0L to 0L
        }
    }

/**
 * Manages the location of the "imported files" storage root -- the directory
 * that contains `sets/<name>/...` for game data extracted from GOG installers,
 * CD images, mod archives, etc.
 *
 * By default this lives at `filesDir/imported/`.  The user may relocate it to
 * an app-private dir on a different volume (typically an SD card or USB OTG
 * drive) on devices with cramped internal storage like the NVIDIA Shield Tube.
 *
 * Saves, configs, pilots, controller mappings, and custom music stay in
 * [filesDir] regardless of override state.
 *
 * Persistence: a single JSON file `import_location.json` in [filesDir].
 *
 * Override storage uses `Context.getExternalFilesDirs(null)` -- app-private
 * external dirs.  These are real filesystem paths usable by PhysFS and the
 * native extractors with no SAF translation layer; they require no runtime
 * permissions and are deleted on app uninstall.
 */
class ImportLocationManager(
    private val filesDir: File,
) {
    data class VolumeOption(
        val label: String,
        val path: File,
        val freeBytes: Long,
        val totalBytes: Long,
        val isPrimary: Boolean,
        val isRemovable: Boolean,
        val isCurrent: Boolean,
    )

    private val prefFile get() = File(filesDir, PREF_FILENAME)
    private val defaultDir get() = File(filesDir, DEFAULT_DIR_NAME)

    /** Returns the active import root.  Falls back to default if pref missing
     *  or if the override path is on an unmounted volume. */
    fun getActiveRoot(): File {
        val override = readOverridePath() ?: return defaultDir.also { it.mkdirs() }
        val volume = override.parentFile?.parentFile
        // Sanity: the override path must be reachable.  If the underlying
        // volume disappeared (SD card removed) the path won't resolve.
        if (!override.exists() && (volume == null || !volume.exists())) {
            logW("Override import root not reachable: ${override.absolutePath}; using default")
            return defaultDir.also { it.mkdirs() }
        }
        override.mkdirs()
        return override
    }

    /** Returns the in-app default root (always `filesDir/imported`). */
    fun getDefaultRoot(): File = defaultDir.also { it.mkdirs() }

    fun isOverrideActive(): Boolean = readOverridePath() != null

    /** True when [readOverridePath] returned a path but the volume is gone. */
    fun isOverrideUnreachable(): Boolean {
        val override = readOverridePath() ?: return false
        return !override.exists() && override.parentFile?.parentFile?.exists() != true
    }

    /**
     * List candidate volumes the user can pick.  Always includes the in-app
     * default at index 0.  Excludes the primary internal app-private external
     * dir (which lives on the same partition as filesDir on most devices,
     * including Shield Tube).
     */
    fun listCandidateVolumes(ctx: Context): List<VolumeOption> {
        val active = getActiveRoot().absolutePath
        val result = mutableListOf<VolumeOption>()

        val defaultStat = safeStat(defaultDir)
        result.add(
            VolumeOption(
                label = "Internal app storage (default)",
                path = defaultDir,
                freeBytes = defaultStat.first,
                totalBytes = defaultStat.second,
                isPrimary = true,
                isRemovable = false,
                isCurrent = active == defaultDir.absolutePath,
            ),
        )

        val externals = ctx.getExternalFilesDirs(null) ?: emptyArray()
        // externals[0] is the primary external app-private dir (same partition
        // as internal on Shield).  Skip it; later entries are removable/USB.
        for ((idx, dir) in externals.withIndex()) {
            if (dir == null) continue
            if (idx == 0) continue
            val target = File(dir, DEFAULT_DIR_NAME)
            val stat = safeStat(dir)
            val removable =
                try {
                    Environment.isExternalStorageRemovable(dir)
                } catch (e: Throwable) {
                    true
                }
            val label =
                when {
                    removable -> "SD card / removable"
                    else -> "Secondary storage"
                }
            result.add(
                VolumeOption(
                    label = label,
                    path = target,
                    freeBytes = stat.first,
                    totalBytes = stat.second,
                    isPrimary = false,
                    isRemovable = removable,
                    isCurrent = active == target.absolutePath,
                ),
            )
        }
        return result
    }

    /** Persist a new override.  Caller must have already migrated data. */
    fun setOverride(newRoot: File) {
        newRoot.mkdirs()
        // Flat key=value format keeps the parser trivial and avoids depending
        // on org.json (which is an unmocked Android stub in JVM unit tests).
        prefFile.writeText(
            "schema=$SCHEMA_VERSION\n" +
                "import_root=${newRoot.absolutePath}\n",
        )
        logI("Import root override set to ${newRoot.absolutePath}")
    }

    /** Remove the override; subsequent [getActiveRoot] returns the default. */
    fun clearOverride() {
        if (prefFile.exists() && !prefFile.delete()) {
            logW("Failed to delete pref file ${prefFile.absolutePath}")
        }
        logI("Import root override cleared")
    }

    private fun readOverridePath(): File? {
        if (!prefFile.exists()) return null
        return try {
            var importRoot: String? = null
            for (line in prefFile.readLines()) {
                val eq = line.indexOf('=')
                if (eq <= 0) continue
                val key = line.substring(0, eq).trim()
                val value = line.substring(eq + 1).trim()
                if (key == "import_root" && value.isNotEmpty()) importRoot = value
            }
            importRoot?.let { File(it) }
        } catch (e: Exception) {
            logW("Bad import_location.json; ignoring", e)
            null
        }
    }

    private fun safeStat(dir: File): Pair<Long, Long> = statBytes(dir)

    /**
     * Result of a [migrate] call.
     */
    sealed class MigrateResult {
        object Success : MigrateResult()

        data class Failure(
            val reason: String,
        ) : MigrateResult()
    }

    /**
     * Copy everything under [src] into [dst], verify the copy by total byte
     * count, then delete the source.  Both directories must exist before
     * the call (they're created if absent).  [progress] is invoked with
     * `(bytesCopied, totalBytes)` periodically.
     *
     * On any failure the source is left untouched and the destination is
     * scrubbed of partial files; caller may safely retry.
     *
     * Free-space check requires `dstVolumeFree >= totalBytes + SAFETY_MARGIN`.
     */
    fun migrate(
        src: File,
        dst: File,
        progress: (Long, Long) -> Unit = { _, _ -> },
    ): MigrateResult {
        if (!src.exists()) {
            // Nothing to copy; just ensure dst exists.
            dst.mkdirs()
            return MigrateResult.Success
        }
        if (src.absolutePath == dst.absolutePath) return MigrateResult.Success

        val totalBytes = src.walkTopDown().filter { it.isFile }.sumOf { it.length() }
        val dstVolume = dst.parentFile ?: dst
        dstVolume.mkdirs()
        val (freeOnDst, _) = statBytes(dstVolume)
        if (freeOnDst > 0 && freeOnDst < totalBytes + SAFETY_MARGIN_BYTES) {
            return MigrateResult.Failure(
                "Not enough free space at destination " +
                    "(need ${totalBytes + SAFETY_MARGIN_BYTES} bytes, have $freeOnDst)",
            )
        }

        dst.mkdirs()
        // In-progress marker so a future launch can detect a crashed migrate.
        val marker = File(dst, IN_PROGRESS_MARKER)
        try {
            marker.writeText(System.currentTimeMillis().toString())
        } catch (e: Exception) {
            return MigrateResult.Failure("Cannot write progress marker: ${e.message}")
        }

        var copied = 0L
        try {
            for (file in src.walkTopDown()) {
                val rel = file.relativeTo(src).path
                if (rel.isEmpty()) {
                    dst.mkdirs()
                    continue
                }
                val target = File(dst, rel)
                if (file.isDirectory) {
                    target.mkdirs()
                    continue
                }
                target.parentFile?.mkdirs()
                file.inputStream().use { input ->
                    target.outputStream().use { output ->
                        val buf = ByteArray(64 * 1024)
                        while (true) {
                            val n = input.read(buf)
                            if (n <= 0) break
                            output.write(buf, 0, n)
                            copied += n
                            progress(copied, totalBytes)
                        }
                    }
                }
                target.setLastModified(file.lastModified())
            }
        } catch (e: Exception) {
            logE("Migrate copy failed", e)
            // Roll back partial dest tree (everything we just wrote).
            for (file in dst.walkBottomUp()) {
                if (file == dst) continue
                file.delete()
            }
            marker.delete()
            return MigrateResult.Failure("Copy failed: ${e.message}")
        }

        // Verify by total bytes (dst minus marker).
        val dstBytes =
            dst
                .walkTopDown()
                .filter { it.isFile && it.name != IN_PROGRESS_MARKER }
                .sumOf { it.length() }
        if (dstBytes != totalBytes) {
            logE("Migrate verify failed: src=$totalBytes dst=$dstBytes")
            for (file in dst.walkBottomUp()) {
                if (file == dst) continue
                file.delete()
            }
            return MigrateResult.Failure("Verification mismatch: src=$totalBytes dst=$dstBytes")
        }

        marker.delete()

        // Source removal -- if it fails we still report success since the
        // destination is verified; user can clear the leftover from
        // Storage Inspector.
        if (!src.deleteRecursively()) {
            logW("Could not fully delete source ${src.absolutePath}")
        }
        return MigrateResult.Success
    }

    /**
     * On startup, if any candidate volume has a stale [IN_PROGRESS_MARKER]
     * but isn't the active root, rename its tree to a `.partial-<ts>` sibling
     * so the user can investigate via Storage Inspector.  Best-effort.
     */
    fun handleStaleInProgressMarkers(ctx: Context) {
        val active = getActiveRoot().absolutePath
        for (option in listCandidateVolumes(ctx)) {
            if (option.path.absolutePath == active) continue
            val marker = File(option.path, IN_PROGRESS_MARKER)
            if (marker.exists()) {
                val parked = File(option.path.parentFile, option.path.name + ".partial-" + System.currentTimeMillis())
                if (option.path.renameTo(parked)) {
                    logW("Parked stale partial migration to ${parked.absolutePath}")
                } else {
                    marker.delete()
                }
            }
        }
    }

    companion object {
        const val PREF_FILENAME = "import_location.txt"
        const val DEFAULT_DIR_NAME = "imported"
        const val IN_PROGRESS_MARKER = ".migrate-in-progress"
        private const val SCHEMA_VERSION = 1
        private const val SAFETY_MARGIN_BYTES = 64L * 1024L * 1024L // 64 MB
    }
}
