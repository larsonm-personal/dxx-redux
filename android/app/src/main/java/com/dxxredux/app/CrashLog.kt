package com.dxxredux.app

import android.content.Context
import android.content.Intent
import android.util.Log
import androidx.core.content.FileProvider
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Crash logger. Captures uncaught Java exceptions and native signals,
 * writing crash reports to filesDir/crashlogs/. Reports are viewable
 * and exportable from the Advanced Settings page.
 *
 * Call [install] once from each Activity's onCreate.
 */
object CrashLog {
    private const val TAG = "CrashLog"
    private const val DIR_NAME = "crashlogs"
    private const val MAX_FILES = 5
    private const val AUTHORITY = "com.dxxredux.app.fileprovider"

    private var installed = false

    /**
     * Install crash handlers. Safe to call multiple times (idempotent after first).
     * Sets a Java UncaughtExceptionHandler and installs native signal handlers.
     */
    fun install(context: Context) {
        if (installed) return
        installed = true

        val crashDir = File(context.filesDir, DIR_NAME)
        crashDir.mkdirs()

        // -- Java uncaught exception handler --
        val oldHandler = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            writeCrashFile(crashDir, thread, throwable)
            // Chain to previous handler (Android default) so the system still shows crash dialog
            oldHandler?.uncaughtException(thread, throwable)
        }

        // -- Native signal handler --
        try {
            nativeInstallCrashHandler(crashDir.absolutePath)
        } catch (e: UnsatisfiedLinkError) {
            Log.w(TAG, "Native crash handler not available (JNI not loaded yet?)", e)
        }
    }

    /**
     * Write a crash report for a Java exception.
     * Called from the UncaughtExceptionHandler -- must not throw.
     */
    private fun writeCrashFile(
        crashDir: File,
        thread: Thread,
        throwable: Throwable,
    ) {
        try {
            crashDir.mkdirs()
            pruneOldFiles(crashDir)
            val stamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date())
            val file = File(crashDir, "crash_$stamp.txt")
            val sw = StringWriter()
            sw.append("Crash at: ${SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US).format(Date())}\n")
            sw.append("Thread: ${thread.name} (id=${thread.id})\n")
            sw.append("Exception: ${throwable.javaClass.name}: ${throwable.message}\n\n")
            sw.append("Stack trace:\n")
            val pw = PrintWriter(sw)
            throwable.printStackTrace(pw)
            pw.flush()
            // Include cause chain
            var cause = throwable.cause
            while (cause != null) {
                sw.append("\nCaused by: ${cause.javaClass.name}: ${cause.message}\n")
                cause.printStackTrace(pw)
                pw.flush()
                cause = cause.cause
            }
            sw.append("\n--- Device Info ---\n")
            sw.append("Model: ${android.os.Build.MODEL}\n")
            sw.append("SDK: ${android.os.Build.VERSION.SDK_INT}\n")
            sw.append("ABI: ${android.os.Build.SUPPORTED_ABIS.joinToString()}\n")
            file.writeText(sw.toString())
            Log.i(TAG, "Crash report written: ${file.name}")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to write crash report", e)
        }
    }

    /** List existing crash files, newest first. */
    fun listCrashFiles(context: Context): List<File> {
        val dir = File(context.filesDir, DIR_NAME)
        if (!dir.isDirectory) return emptyList()
        return dir
            .listFiles()
            ?.filter { it.isFile && it.name.startsWith("crash_") }
            ?.sortedByDescending { it.lastModified() }
            ?: emptyList()
    }

    /** Share a crash file via system share sheet. */
    fun shareCrashFile(
        context: Context,
        file: File,
    ): Boolean =
        try {
            val exportDir = File(context.cacheDir, "crashlog_exports")
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
            val chooser = Intent.createChooser(intent, "Share Crash Report")
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            context.startActivity(chooser)
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to share crash file", e)
            false
        }

    /** Delete all crash files. */
    fun deleteAllCrashFiles(context: Context) {
        val dir = File(context.filesDir, DIR_NAME)
        dir.listFiles()?.forEach { it.delete() }
    }

    private fun pruneOldFiles(dir: File) {
        val files =
            dir
                .listFiles()
                ?.filter { it.isFile && it.name.startsWith("crash_") }
                ?.sortedBy { it.lastModified() }
                ?: return
        val toDelete = files.size - (MAX_FILES - 1)
        if (toDelete > 0) {
            files.take(toDelete).forEach { it.delete() }
        }
    }

    /**
     * Install native signal handlers. Call after System.loadLibrary().
     * Safe to call multiple times (native side is idempotent).
     */
    fun installNativeHandler(context: Context) {
        try {
            val crashDir = File(context.filesDir, DIR_NAME)
            crashDir.mkdirs()
            nativeInstallCrashHandler(crashDir.absolutePath)
        } catch (e: UnsatisfiedLinkError) {
            Log.w(TAG, "Native crash handler not available", e)
        }
    }

    // JNI declaration -- implemented in android_crash_handler.c
    private external fun nativeInstallCrashHandler(crashDir: String)
}
