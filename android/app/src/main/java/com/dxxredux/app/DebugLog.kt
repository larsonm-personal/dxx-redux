package com.dxxredux.app

import android.content.Context
import android.content.Intent
import android.os.Build
import android.util.Log
import androidx.core.content.pm.PackageInfoCompat
import java.io.BufferedWriter
import java.io.File
import java.io.FileWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.ArrayBlockingQueue
import java.util.concurrent.RejectedExecutionException
import java.util.concurrent.ThreadPoolExecutor
import java.util.concurrent.TimeUnit

/**
 * Multi-category debug logger. Each category has an independent toggle stored
 * in SharedPreferences. All enabled categories write to the same log file for
 * chronological interleaving. Maintains at most [MAX_FILES] log files.
 *
 * Category IDs match [DebugLogCategory] (Kotlin) and debug_log_categories.h (C).
 *
 * Backward-compatible: network logging goes through this as category NETWORK.
 */
object DebugLog {
    private const val TAG = "DebugLog"
    private const val PREFS_NAME = "dxx_prefs"
    private const val DIR_NAME = "debuglogs"
    private const val MAX_FILES = 5
    private const val PREF_ACTIVE_LOG_PATH = "debuglog_active_path"
    private const val PREF_ACTIVE_LOG_BUILD = "debuglog_active_build"
    private const val ACTIVE_LOG_REUSE_WINDOW_MS = 15 * 60 * 1000L

    private var writer: BufferedWriter? = null
    private var currentFile: File? = null
    private val enabledCategories = BooleanArray(DebugLogCategory.COUNT)
    private val lock = Any()
    private val tsFormat = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS", Locale.US)
    private val forcedBatchExecutor =
        ThreadPoolExecutor(
            1,
            1,
            0L,
            TimeUnit.MILLISECONDS,
            ArrayBlockingQueue(2),
            { runnable -> Thread(runnable, "slowdown-log-writer").apply { isDaemon = true } },
        )

    /** True if any category is enabled (controls file open/close). */
    private fun anyEnabled(): Boolean = enabledCategories.any { it }

    fun init(context: Context) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        for (i in 0 until DebugLogCategory.COUNT) {
            enabledCategories[i] = prefs.getBoolean(DebugLogCategory.prefKey(i), false)
        }

        // Migrate old net_logging_enabled pref to new category system
        if (prefs.contains("net_logging_enabled")) {
            val wasEnabled = prefs.getBoolean("net_logging_enabled", false)
            if (wasEnabled && !enabledCategories[DebugLogCategory.NETWORK]) {
                enabledCategories[DebugLogCategory.NETWORK] = true
                prefs
                    .edit()
                    .putBoolean(DebugLogCategory.prefKey(DebugLogCategory.NETWORK), true)
                    .remove("net_logging_enabled")
                    .apply()
            } else {
                prefs.edit().remove("net_logging_enabled").apply()
            }
        }

        if (anyEnabled()) openLog(context)
    }

    /** Open an existing log file for appending (used by :game process). */
    fun initAppend(
        context: Context,
        filePath: String,
    ) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        for (i in 0 until DebugLogCategory.COUNT) {
            enabledCategories[i] = prefs.getBoolean(DebugLogCategory.prefKey(i), false)
        }
        if (!anyEnabled()) return
        synchronized(lock) {
            closeLog()
            val file = File(filePath)
            if (!file.exists()) {
                openLog(context)
                return
            }
            try {
                writer = BufferedWriter(FileWriter(file, true))
                currentFile = file
                rememberActiveLog(context, file)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to open log file for append", e)
                writer = null
            }
        }
    }

    fun currentFilePath(): String? = synchronized(lock) { currentFile?.absolutePath }

    fun setCategoryEnabled(
        context: Context,
        category: Int,
        on: Boolean,
    ) {
        if (category < 0 || category >= DebugLogCategory.COUNT) return
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putBoolean(DebugLogCategory.prefKey(category), on).apply()
        synchronized(lock) {
            enabledCategories[category] = on
            if (on && writer == null) {
                openLog(context)
            } else if (!anyEnabled()) {
                closeLog()
            }
        }
    }

    fun isCategoryEnabled(
        context: Context,
        category: Int,
    ): Boolean {
        if (category < 0 || category >= DebugLogCategory.COUNT) return false
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getBoolean(DebugLogCategory.prefKey(category), false)
    }

    fun log(
        category: Int,
        message: String,
    ) {
        synchronized(lock) {
            writer ?: return
            if (category < 0 || category >= DebugLogCategory.COUNT) return
            if (!enabledCategories[category]) return
            try {
                val tag = DebugLogCategory.labels[category].uppercase()
                writeLine(tag, message)
            } catch (e: Exception) {
                Log.w(TAG, "Failed to write log line", e)
            }
        }
    }

    fun logForced(
        context: Context,
        category: Int,
        message: String,
    ) {
        synchronized(lock) {
            if (category < 0 || category >= DebugLogCategory.COUNT) return
            if (writer == null) openLog(context)
            writer ?: return
            try {
                val tag = DebugLogCategory.labels[category].uppercase()
                writeLine(tag, message)
            } catch (e: Exception) {
                Log.w(TAG, "Failed to write forced log line", e)
            }
        }
    }

    fun logBatch(
        category: Int,
        payload: String,
    ) {
        synchronized(lock) {
            writer ?: return
            if (category < 0 || category >= DebugLogCategory.COUNT) return
            if (!enabledCategories[category]) return
            try {
                val tag = DebugLogCategory.labels[category].uppercase()
                var wroteAny = false
                payload.lineSequence().forEach { line ->
                    if (line.isEmpty()) return@forEach
                    writeLineNoFlush(tag, line)
                    wroteAny = true
                }
                if (wroteAny) {
                    writer?.flush()
                }
            } catch (e: Exception) {
                Log.w(TAG, "Failed to write log batch", e)
            }
        }
    }

    /** Queue an automatic diagnostic batch without enabling its category. */
    fun logBatchForcedAsync(
        context: Context,
        category: Int,
        payload: String,
    ) {
        if (category < 0 || category >= DebugLogCategory.COUNT || payload.isBlank()) return
        val appContext = context.applicationContext
        try {
            forcedBatchExecutor.execute {
                synchronized(lock) {
                    if (writer == null) openLog(appContext)
                    writer ?: return@synchronized
                    try {
                        val tag = DebugLogCategory.labels[category].uppercase()
                        var wroteAny = false
                        payload.lineSequence().forEach { line ->
                            if (line.isEmpty()) return@forEach
                            writeLineNoFlush(tag, line)
                            wroteAny = true
                        }
                        if (wroteAny) writer?.flush()
                    } catch (e: Exception) {
                        Log.w(TAG, "Failed to write forced log batch", e)
                    }
                }
            }
        } catch (_: RejectedExecutionException) {
            Log.w(TAG, "Dropping automatic diagnostic batch because the writer queue is full")
        }
    }

    fun listLogFiles(context: Context): List<File> {
        flush()
        val dir = File(context.filesDir, DIR_NAME)
        if (!dir.isDirectory) return emptyList()
        return dir
            .listFiles()
            ?.filter { it.isFile && it.name.startsWith("debuglog_") }
            ?.sortedByDescending { it.lastModified() }
            ?: emptyList()
    }

    fun flush() {
        synchronized(lock) {
            try {
                writer?.flush()
            } catch (e: Exception) {
                Log.w(TAG, "Failed to flush log file", e)
            }
        }
    }

    fun shareLogFile(
        context: Context,
        file: File,
    ): Boolean =
        try {
            val uri = FileProviderGrantStore.copy(context, file, FileProviderGrantStore.DEBUG_LOG_EXPORTS)
            val intent =
                Intent(Intent.ACTION_SEND).apply {
                    type = "text/plain"
                    putExtra(Intent.EXTRA_STREAM, uri)
                    attachReadGrant(uri)
                }
            val chooser = Intent.createChooser(intent, "Share Debug Log")
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            context.startActivity(chooser)
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to share log file", e)
            false
        }

    fun deleteAllLogs(context: Context) {
        synchronized(lock) {
            val active = currentFile
            val dir = File(context.filesDir, DIR_NAME)
            dir.listFiles()?.forEach { f ->
                if (active == null || f.absolutePath != active.absolutePath) f.delete()
            }
        }
    }

    private fun openLog(context: Context) {
        closeLog()
        val dir = File(context.filesDir, DIR_NAME)
        dir.mkdirs()
        if (reuseActiveLog(context)) return
        pruneOldFiles(dir)
        val stamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date())
        val file = File(dir, "debuglog_$stamp.txt")
        try {
            writer = BufferedWriter(FileWriter(file, true))
            currentFile = file
            rememberActiveLog(context, file)
            val enabled =
                (0 until DebugLogCategory.COUNT)
                    .filter { enabledCategories[it] }
                    .joinToString(", ") { DebugLogCategory.labels[it] }
            writeLine(headerTag(), buildHeaderLine(context, enabled))
        } catch (e: Exception) {
            Log.e(TAG, "Failed to open log file", e)
            writer = null
        }
    }

    private fun reuseActiveLog(context: Context): Boolean {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val activePath = prefs.getString(PREF_ACTIVE_LOG_PATH, null) ?: return false
        val activeBuild = prefs.getString(PREF_ACTIVE_LOG_BUILD, null) ?: return false
        if (activeBuild != currentBuildStamp()) return false
        val file = File(activePath)
        if (!file.exists()) return false
        if (System.currentTimeMillis() - file.lastModified() > ACTIVE_LOG_REUSE_WINDOW_MS) return false
        return try {
            writer = BufferedWriter(FileWriter(file, true))
            currentFile = file
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to reuse active log file", e)
            writer = null
            currentFile = null
            false
        }
    }

    private fun rememberActiveLog(
        context: Context,
        file: File,
    ) {
        context
            .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putString(PREF_ACTIVE_LOG_PATH, file.absolutePath)
            .putString(PREF_ACTIVE_LOG_BUILD, currentBuildStamp())
            .apply()
    }

    private fun currentBuildStamp(): String = "${BuildInfo.GIT_COMMIT_COUNT}:${BuildInfo.GIT_SHORT_HASH}"

    private fun headerTag(): String {
        val firstEnabled = (0 until DebugLogCategory.COUNT).firstOrNull { enabledCategories[it] }
        return if (firstEnabled != null) DebugLogCategory.labels[firstEnabled].uppercase() else "DEBUG"
    }

    private fun buildHeaderLine(
        context: Context,
        enabled: String,
    ): String {
        val packageInfo =
            try {
                context.packageManager.getPackageInfo(context.packageName, 0)
            } catch (_: Exception) {
                null
            }
        val versionName = packageInfo?.versionName?.takeUnless { it.isNullOrBlank() } ?: "unknown"
        val versionCode = packageInfo?.let { PackageInfoCompat.getLongVersionCode(it).toString() } ?: "unknown"
        val primaryAbi = Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown"
        val osArch = System.getProperty("os.arch").takeUnless { it.isNullOrBlank() } ?: "unknown"
        return buildString {
            append("Log started -- app=$versionName ($versionCode)")
            append(" build=${BuildInfo.GIT_COMMIT_COUNT} (${BuildInfo.GIT_SHORT_HASH})")
            append(" ${BuildInfo.BUILD_TYPE}")
            append(" built=${BuildInfo.BUILD_DATE} ${BuildInfo.BUILD_TIME}")
            append(" abi=$primaryAbi arch=$osArch")
            append(" categories=$enabled")
        }
    }

    private fun writeLine(
        tag: String,
        message: String,
    ) {
        writeLineNoFlush(tag, message)
        writer?.flush()
    }

    private fun writeLineNoFlush(
        tag: String,
        message: String,
    ) {
        val w = writer ?: return
        val ts = tsFormat.format(Date())
        w.write("$ts [$tag] ${message.trimEnd()}")
        w.newLine()
    }

    private fun closeLog() {
        try {
            writer?.close()
        } catch (_: Exception) {
        }
        writer = null
        currentFile = null
    }

    private fun pruneOldFiles(dir: File) {
        val files =
            dir
                .listFiles()
                ?.filter { it.isFile && it.name.startsWith("debuglog_") }
                ?.sortedBy { it.lastModified() }
                ?: return
        val toDelete = files.size - (MAX_FILES - 1)
        if (toDelete > 0) {
            files.take(toDelete).forEach { it.delete() }
        }
    }
}
