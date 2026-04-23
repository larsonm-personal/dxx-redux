package com.dxxredux.app

import android.content.Context
import android.content.Intent
import android.os.Build
import android.util.Log
import androidx.core.content.FileProvider
import androidx.core.content.pm.PackageInfoCompat
import xcrash.TombstoneManager
import java.io.File

/**
 * Crash report utilities. xCrash owns Java/native/ANR capture and writes
 * tombstones into filesDir/tombstones/. We keep the native breadcrumb ring
 * and append it plus build metadata into the generated tombstone files.
 *
 * Reports are viewable and exportable from the Advanced Settings page.
 */
object CrashLog {
    private const val TAG = "CrashLog"
    private const val LEGACY_DIR_NAME = "crashlogs"
    private const val TOMBSTONE_DIR_NAME = "tombstones"
    private const val AUTHORITY = "com.dxxredux.app.fileprovider"

    private var installed = false

    /**
     * Legacy compatibility hook. xCrash is initialized from the Application,
     * so Activity call sites can safely keep calling this without installing
     * a second Java uncaught exception handler.
     */
    fun install(context: Context) {
        @Suppress("UNUSED_PARAMETER")
        val unused = context
        if (installed) return
        installed = true
    }

    /** List existing crash files, newest first. */
    fun listCrashFiles(context: Context): List<File> {
        val appContext = context.applicationContext
        return crashDirs(appContext)
            .flatMap { dir ->
                if (!dir.isDirectory) {
                    emptyList()
                } else {
                    dir.listFiles()?.filter(::isCrashReportFile).orEmpty()
                }
            }.sortedByDescending { it.lastModified() }
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
        val appContext = context.applicationContext
        crashDirs(appContext).forEach { dir -> dir.listFiles()?.forEach { it.delete() } }
    }

    fun appendXCrashSections(
        context: Context,
        logPath: String?,
        emergency: String?,
    ) {
        if (logPath.isNullOrBlank()) {
            if (!emergency.isNullOrBlank()) {
                Log.w(TAG, "xCrash callback received emergency-only crash info")
            }
            return
        }

        val appContext = context.applicationContext
        val header = buildCommonCrashHeader(appContext).trimEnd()
        if (header.isNotEmpty()) {
            TombstoneManager.appendSection(logPath, "dxx-redux header", header)
        }

        val breadcrumbs =
            try {
                nativeGetBreadcrumbReport()?.trimEnd().orEmpty()
            } catch (_: UnsatisfiedLinkError) {
                ""
            }
        if (breadcrumbs.isNotEmpty()) {
            TombstoneManager.appendSection(logPath, "dxx-redux breadcrumbs", breadcrumbs)
        }
    }

    /**
     * Initialize native breadcrumb storage after System.loadLibrary().
     * xCrash owns crash handling; this only provides the crash directory
     * for Error() and exposes breadcrumbs to the xCrash callback.
     */
    fun installNativeHandler(context: Context) {
        try {
            val appContext = context.applicationContext
            val crashDir = getTombstoneDir(appContext)
            crashDir.mkdirs()
            nativeInstallCrashHandler(crashDir.absolutePath, buildCommonCrashHeader(appContext))
        } catch (e: UnsatisfiedLinkError) {
            Log.w(TAG, "Native crash handler not available", e)
        }
    }

    fun getTombstoneDir(context: Context): File = File(context.filesDir, TOMBSTONE_DIR_NAME)

    fun buildCommonCrashHeader(context: Context): String {
        val packageInfo =
            try {
                context.packageManager.getPackageInfo(context.packageName, 0)
            } catch (_: Exception) {
                null
            }
        val versionName = packageInfo?.versionName?.takeUnless { it.isNullOrBlank() } ?: "unknown"
        val versionCode = packageInfo?.let { PackageInfoCompat.getLongVersionCode(it).toString() } ?: "unknown"
        val primaryAbi = Build.SUPPORTED_ABIS.firstOrNull() ?: "unknown"
        val supportedAbis = Build.SUPPORTED_ABIS.joinToString().ifBlank { "unknown" }
        val osArch = System.getProperty("os.arch").takeUnless { it.isNullOrBlank() } ?: "unknown"

        return buildString {
            append("Package: ${context.packageName}\n")
            append("App version: $versionName ($versionCode)\n")
            append("Build: ${BuildInfo.GIT_COMMIT_COUNT} (${BuildInfo.GIT_SHORT_HASH}) ${BuildInfo.BUILD_TYPE}\n")
            append("Built: ${BuildInfo.BUILD_DATE} ${BuildInfo.BUILD_TIME}\n")
            append("Model: ${Build.MODEL}\n")
            append("SDK: ${Build.VERSION.SDK_INT}\n")
            append("Primary ABI: $primaryAbi\n")
            append("Supported ABIs: $supportedAbis\n")
            append("OS arch: $osArch\n")
        }
    }

    private fun crashDirs(context: Context): List<File> =
        listOf(getTombstoneDir(context), File(context.filesDir, LEGACY_DIR_NAME))

    private fun isCrashReportFile(file: File): Boolean {
        if (!file.isFile) return false
        return file.name.startsWith("tombstone_") || file.name.startsWith("crash_")
    }

    // JNI declarations -- implemented in android_crash_handler.c
    private external fun nativeInstallCrashHandler(
        crashDir: String,
        installInfo: String,
    )

    private external fun nativeGetBreadcrumbReport(): String?
}
