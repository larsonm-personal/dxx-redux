package com.dxxredux.app.multiplayer

import android.content.Context
import android.content.Intent
import android.util.Log
import androidx.core.content.FileProvider
import java.io.BufferedWriter
import java.io.File
import java.io.FileWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Network debug logger. When enabled, writes timestamped events to a log file
 * in filesDir/netlogs/. Maintains at most [MAX_FILES] log files, deleting the
 * oldest when a new one would exceed the limit.
 */
object NetLog {
    private const val TAG = "NetLog"
    private const val PREFS_NAME = "dxx_prefs"
    private const val KEY_ENABLED = "net_logging_enabled"
    private const val DIR_NAME = "netlogs"
    private const val MAX_FILES = 10
    private const val AUTHORITY = "com.dxxredux.app.fileprovider"

    private var writer: BufferedWriter? = null
    private var enabled = false
    private val lock = Any()
    private val tsFormat = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS", Locale.US)

    fun init(context: Context) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        enabled = prefs.getBoolean(KEY_ENABLED, false)
        if (enabled) {
            openLog(context)
        }
    }

    fun setEnabled(
        context: Context,
        on: Boolean,
    ) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putBoolean(KEY_ENABLED, on).apply()
        synchronized(lock) {
            enabled = on
            if (on) {
                openLog(context)
            } else {
                closeLog()
            }
        }
    }

    fun isEnabled(context: Context): Boolean {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getBoolean(KEY_ENABLED, false)
    }

    fun log(
        category: String,
        message: String,
    ) {
        synchronized(lock) {
            val w = writer ?: return
            if (!enabled) return
            try {
                val ts = tsFormat.format(Date())
                w.write("$ts [$category] $message")
                w.newLine()
                w.flush()
            } catch (e: Exception) {
                Log.w(TAG, "Failed to write log line", e)
            }
        }
    }

    /** List existing log files, newest first. */
    fun listLogFiles(context: Context): List<File> {
        val dir = File(context.filesDir, DIR_NAME)
        if (!dir.isDirectory) return emptyList()
        return dir
            .listFiles()
            ?.filter { it.isFile && it.name.startsWith("netlog_") }
            ?.sortedByDescending { it.lastModified() }
            ?: emptyList()
    }

    /** Share a log file via system share sheet. */
    fun shareLogFile(
        context: Context,
        file: File,
    ): Boolean =
        try {
            // Copy to cache so FileProvider can serve it
            val exportDir = File(context.cacheDir, "netlog_exports")
            exportDir.mkdirs()
            val copy = File(exportDir, file.name)
            file.copyTo(copy, overwrite = true)

            val uri = FileProvider.getUriForFile(context, AUTHORITY, copy)
            val intent =
                Intent(Intent.ACTION_SEND).apply {
                    type = "text/plain"
                    putExtra(Intent.EXTRA_STREAM, uri)
                    addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                }
            val chooser = Intent.createChooser(intent, "Share Network Log")
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            context.startActivity(chooser)
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to share log file", e)
            false
        }

    /** Delete all log files. */
    fun deleteAllLogs(context: Context) {
        synchronized(lock) {
            closeLog()
        }
        val dir = File(context.filesDir, DIR_NAME)
        dir.listFiles()?.forEach { it.delete() }
    }

    private fun openLog(context: Context) {
        closeLog()
        val dir = File(context.filesDir, DIR_NAME)
        dir.mkdirs()
        pruneOldFiles(dir)
        val stamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date())
        val file = File(dir, "netlog_$stamp.txt")
        try {
            writer = BufferedWriter(FileWriter(file, true))
            log(
                "SYSTEM",
                "Log started -- build ${com.dxxredux.app.BuildInfo.GIT_COMMIT_COUNT}" +
                    " (${com.dxxredux.app.BuildInfo.GIT_SHORT_HASH})" +
                    " ${com.dxxredux.app.BuildInfo.BUILD_DATE}" +
                    " ${com.dxxredux.app.BuildInfo.BUILD_TYPE}",
            )
        } catch (e: Exception) {
            Log.e(TAG, "Failed to open log file", e)
            writer = null
        }
    }

    private fun closeLog() {
        try {
            writer?.close()
        } catch (_: Exception) {
        }
        writer = null
    }

    private fun pruneOldFiles(dir: File) {
        val files =
            dir
                .listFiles()
                ?.filter { it.isFile && it.name.startsWith("netlog_") }
                ?.sortedBy { it.lastModified() }
                ?: return
        // Keep at most MAX_FILES - 1 so the new file fits within the limit
        val toDelete = files.size - (MAX_FILES - 1)
        if (toDelete > 0) {
            files.take(toDelete).forEach { it.delete() }
        }
    }
}
