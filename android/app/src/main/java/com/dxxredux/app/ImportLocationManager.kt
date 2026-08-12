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
        check(newRoot.isDirectory || newRoot.mkdirs()) { "Could not create import root ${newRoot.absolutePath}" }
        // Flat key=value format keeps the parser trivial and avoids depending
        // on org.json (which is an unmocked Android stub in JVM unit tests).
        AtomicFilePublication.writeUtf8(
            prefFile,
            "schema=$SCHEMA_VERSION\n" +
                "import_root=${newRoot.absolutePath}\n",
        )
        logI("Import root override set to ${newRoot.absolutePath}")
    }

    /** Remove the override; subsequent [getActiveRoot] returns the default. */
    fun clearOverride() {
        AtomicFilePublication.transaction {
            if (prefFile.exists()) {
                val retired = AtomicFilePublication.uniqueSibling(prefFile, "retired")
                check(prefFile.renameTo(retired)) { "Failed to retire pref file ${prefFile.absolutePath}" }
                if (!retired.delete()) logW("Failed to delete retired pref file ${retired.absolutePath}")
            }
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
     * Copy everything under [src] into [dst], verify each file, commit the
     * active-root change through [beforeSourceRetire], then delete the source.
     * Existing identical files and destination-only files are preserved;
     * differing collisions fail before publication. [progress] is invoked
     * with `(bytesCopied, totalBytes)` periodically.
     *
     * On any failure the source and every pre-existing destination entry are
     * left untouched. Only entries created by this attempt are rolled back.
     *
     * Free-space check requires `dstVolumeFree >= totalBytes + SAFETY_MARGIN`.
     */
    fun migrate(
        src: File,
        dst: File,
        beforeSourceRetire: () -> Unit = {},
        progress: (Long, Long) -> Unit = { _, _ -> },
    ): MigrateResult {
        if (!src.exists()) {
            return try {
                check(dst.isDirectory || dst.mkdirs()) { "Could not create destination" }
                beforeSourceRetire()
                MigrateResult.Success
            } catch (e: Exception) {
                MigrateResult.Failure("Cannot activate destination: ${e.message}")
            }
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

        if (!dst.isDirectory && !dst.mkdirs()) return MigrateResult.Failure("Cannot create destination")
        // In-progress marker so a future launch can detect a crashed migrate.
        val marker = File(dst, IN_PROGRESS_MARKER)
        if (marker.exists()) return MigrateResult.Failure("Destination contains an incomplete migration")
        val createdFiles = mutableListOf<File>()
        val createdDirs = mutableListOf<File>()
        try {
            marker.writeText(System.currentTimeMillis().toString())
            createdFiles.add(marker)
        } catch (e: Exception) {
            return MigrateResult.Failure("Cannot write progress marker: ${e.message}")
        }

        var copied = 0L
        try {
            // Reject every differing collision before publishing any source byte.
            for (file in src.walkTopDown()) {
                val rel = file.relativeTo(src).path
                if (rel.isEmpty()) continue
                val target = File(dst, rel)
                if (file.isDirectory) {
                    if (target.exists() && !target.isDirectory) {
                        throw IllegalStateException("Destination path is not a directory: $rel")
                    }
                    continue
                }
                if (target.exists() && (!target.isFile || !sameFileContents(file, target))) {
                    throw IllegalStateException("Destination file differs: $rel")
                }
            }

            for (directory in src.walkTopDown().filter(File::isDirectory).drop(1)) {
                ensureMigrationDirectory(File(dst, directory.relativeTo(src).path), dst, createdDirs)
            }

            for (file in src.walkTopDown().filter(File::isFile)) {
                val rel = file.relativeTo(src).path
                val target = File(dst, rel)
                if (target.exists()) {
                    copied += file.length()
                    progress(copied, totalBytes)
                    continue
                }
                ensureMigrationDirectory(target.parentFile, dst, createdDirs)
                val temporary = AtomicFilePublication.uniqueSibling(target, "migration")
                try {
                    file.inputStream().use { input ->
                        java.io.FileOutputStream(temporary).use { output ->
                            val buf = ByteArray(64 * 1024)
                            while (true) {
                                val n = input.read(buf)
                                if (n <= 0) break
                                output.write(buf, 0, n)
                                copied += n
                                progress(copied, totalBytes)
                            }
                            output.flush()
                            output.fd.sync()
                        }
                    }
                    check(sameFileContents(file, temporary)) { "Staged file verification failed: $rel" }
                    check(temporary.renameTo(target)) { "Could not publish migrated file: $rel" }
                    createdFiles.add(target)
                    target.setLastModified(file.lastModified())
                } finally {
                    temporary.delete()
                }
            }

            // The active-root pointer is part of the same logical commit.  If
            // this fails, retain the source and remove only files we created.
            beforeSourceRetire()
            marker.delete()
            createdFiles.remove(marker)
        } catch (e: Exception) {
            logE("Migrate copy or activation failed", e)
            rollbackMigrationFiles(createdFiles, createdDirs)
            return MigrateResult.Failure("Migration failed: ${e.message}")
        }

        // Source removal follows the durable root switch.  Failure leaves a
        // harmless duplicate for Storage Inspector rather than losing data.
        if (!src.deleteRecursively()) {
            logW("Could not fully delete source ${src.absolutePath}")
        }
        return MigrateResult.Success
    }

    private fun ensureMigrationDirectory(
        directory: File?,
        root: File,
        createdDirs: MutableList<File>,
    ) {
        if (directory == null || directory == root || directory.exists()) return
        ensureMigrationDirectory(directory.parentFile, root, createdDirs)
        check(directory.mkdir()) { "Could not create destination directory ${directory.absolutePath}" }
        createdDirs.add(directory)
    }

    private fun rollbackMigrationFiles(
        createdFiles: List<File>,
        createdDirs: List<File>,
    ) {
        createdFiles.asReversed().forEach { it.delete() }
        createdDirs.asReversed().forEach { it.delete() }
    }

    private fun sameFileContents(
        first: File,
        second: File,
    ): Boolean {
        if (first.length() != second.length()) return false
        first.inputStream().buffered().use { left ->
            second.inputStream().buffered().use { right ->
                val leftBuffer = ByteArray(64 * 1024)
                val rightBuffer = ByteArray(64 * 1024)
                while (true) {
                    val leftCount = left.read(leftBuffer)
                    val rightCount = right.read(rightBuffer)
                    if (leftCount != rightCount) return false
                    if (leftCount < 0) return true
                    for (index in 0 until leftCount) {
                        if (leftBuffer[index] != rightBuffer[index]) return false
                    }
                }
            }
        }
    }

    /**
     * Clear stale transaction markers without moving or deleting the root.
     * Migration publishes only new, verified files and rejects differing
     * collisions, so an interrupted inactive destination is safe to retry;
     * an active marked destination crossed the root-switch commit and is a
     * complete generation whose old source can be inspected separately.
     */
    fun handleStaleInProgressMarkers(ctx: Context) {
        val active = getActiveRoot().absolutePath
        for (option in listCandidateVolumes(ctx)) {
            val marker = File(option.path, IN_PROGRESS_MARKER)
            if (marker.exists()) {
                if (marker.delete()) {
                    val state = if (option.path.absolutePath == active) "committed" else "retryable"
                    logW("Recovered $state interrupted migration at ${option.path.absolutePath}")
                } else {
                    logW("Could not clear stale migration marker ${marker.absolutePath}")
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
