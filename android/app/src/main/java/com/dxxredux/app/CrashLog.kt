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
 * If xCrash only produces the child exit stub, loses custom sections, or
 * falls back to emergency-only callback data, inspect the exported
 * tombstone's dxx-redux breadcrumbs section and any crash_error_*.txt
 * fallback report from the same export.
 */
object CrashLog {
    private const val TAG = "CrashLog"
    private const val TOMBSTONE_DIR_NAME = "tombstones"
    private const val AUTHORITY = "com.dxxredux.app.fileprovider"
    private const val HEADER_SECTION = "dxx-redux header"
    private const val BREADCRUMBS_SECTION = "dxx-redux breadcrumbs"
    private const val BREADCRUMB_SNAPSHOT_FILE_NAME = "crash_breadcrumbs_latest.txt"
    private const val BREADCRUMB_BACKFILL_WINDOW_MS = 5 * 60 * 1000L

    private var installed = false
    private var backfillAttempted = false

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
        maybeBackfillMissingXCrashSections(appContext)
        return listCrashFilesRaw(appContext)
    }

    private fun listCrashFilesRaw(context: Context): List<File> {
        val appContext = context.applicationContext
        val dir = getTombstoneDir(appContext)
        if (!dir.isDirectory) return emptyList()
        return dir
            .listFiles()
            ?.filter(::isCrashReportFile)
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
            LauncherFileCopy.copyFileToFile(file, copy)

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
        getTombstoneDir(appContext).listFiles()?.forEach { file ->
            if (file.name.startsWith("tombstone_")) {
                TombstoneManager.deleteTombstone(file)
            } else {
                file.delete()
            }
        }
    }

    fun appendXCrashSections(
        context: Context,
        logPath: String?,
        emergency: String?,
    ) {
        val appContext = context.applicationContext
        val header = buildInstalledCrashHeader(appContext).trimEnd()
        val reportFile = logPath?.takeUnless { it.isBlank() }?.let(::File)
        var appendedHeader = false
        var appendedBreadcrumbs = false

        if (reportFile != null) {
            if (header.isNotEmpty()) {
                appendedHeader = appendCustomSection(reportFile, HEADER_SECTION, header)
            }
        } else if (!emergency.isNullOrBlank()) {
            Log.w(TAG, "xCrash callback received emergency-only crash info")
        }

        val breadcrumbs =
            try {
                nativeGetBreadcrumbReport()?.trimEnd().orEmpty()
            } catch (_: UnsatisfiedLinkError) {
                ""
            }
        if (reportFile != null && breadcrumbs.isNotEmpty()) {
            appendedBreadcrumbs = appendCustomSection(reportFile, BREADCRUMBS_SECTION, breadcrumbs)
        }

        val fallbackReason =
            when {
                !emergency.isNullOrBlank() -> "xCrash supplied emergency crash info"
                reportFile != null -> getCrashFallbackReason(reportFile, breadcrumbs)
                else -> null
            }
        if (fallbackReason != null) {
            val fallback =
                writeCrashFallbackReport(appContext, reportFile, header, fallbackReason, emergency, breadcrumbs)
            if (fallback != null) {
                Log.w(TAG, "Wrote crash fallback report: ${fallback.absolutePath}")
            }
        }

        if (reportFile != null && (!appendedHeader || (breadcrumbs.isNotEmpty() && !appendedBreadcrumbs))) {
            Log.w(
                TAG,
                "xCrash callback could not append all custom sections to ${reportFile.absolutePath}",
            )
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

    fun backfillMissingXCrashSections(context: Context) {
        val appContext = context.applicationContext
        val header = buildCommonCrashHeader(appContext).trimEnd()
        val tombstones = listCrashFilesRaw(appContext).filter { it.name.startsWith("tombstone_") }
        val breadcrumbSnapshotFile = getBreadcrumbSnapshotFile(appContext)
        val breadcrumbSnapshot = readBreadcrumbSnapshot(breadcrumbSnapshotFile)
        if (header.isNotEmpty()) {
            tombstones.forEach { file ->
                if (!appendCustomSection(file, HEADER_SECTION, header)) {
                    Log.w(TAG, "Failed to backfill crash header into ${file.absolutePath}")
                }
            }
        }
        if (breadcrumbSnapshot.isEmpty()) return
        val snapshotTime = breadcrumbSnapshotFile.lastModified()
        val target =
            tombstones.firstOrNull { file ->
                !hasCustomSection(file, BREADCRUMBS_SECTION) &&
                    snapshotTime > 0L &&
                    kotlin.math.abs(file.lastModified() - snapshotTime) <= BREADCRUMB_BACKFILL_WINDOW_MS
            }
        if (target == null) return
        if (appendCustomSection(target, BREADCRUMBS_SECTION, breadcrumbSnapshot)) {
            if (!breadcrumbSnapshotFile.delete()) {
                Log.w(TAG, "Failed to delete consumed breadcrumb snapshot ${breadcrumbSnapshotFile.absolutePath}")
            }
        } else {
            Log.w(TAG, "Failed to backfill crash breadcrumbs into ${target.absolutePath}")
        }
    }

    private fun maybeBackfillMissingXCrashSections(context: Context) {
        if (backfillAttempted) return
        backfillAttempted = true
        try {
            backfillMissingXCrashSections(context)
        } catch (t: Throwable) {
            Log.w(TAG, "Deferred crash report backfill failed", t)
        }
    }

    private fun getBreadcrumbSnapshotFile(context: Context): File =
        File(getTombstoneDir(context), BREADCRUMB_SNAPSHOT_FILE_NAME)

    private fun readBreadcrumbSnapshot(file: File): String =
        try {
            if (!file.isFile) return ""
            file.readText(Charsets.UTF_8).trimEnd()
        } catch (e: Exception) {
            Log.w(TAG, "Failed to read breadcrumb snapshot ${file.absolutePath}", e)
            ""
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

    private fun buildInstalledCrashHeader(context: Context): String =
        try {
            nativeGetInstalledHeader()?.trimEnd().takeUnless { it.isNullOrBlank() }
                ?: buildCommonCrashHeader(context)
        } catch (_: UnsatisfiedLinkError) {
            buildCommonCrashHeader(context)
        }

    private fun appendCustomSection(
        file: File,
        key: String,
        content: String,
    ): Boolean {
        val trimmedContent = content.trimEnd()
        if (trimmedContent.isEmpty()) return true

        return try {
            file.parentFile?.mkdirs()
            if (!file.exists()) {
                file.writeText("", Charsets.UTF_8)
            }
            if (hasCustomSection(file, key)) {
                true
            } else {
                TombstoneManager.appendSection(file.absolutePath, key, trimmedContent)
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to append crash section '$key' to ${file.absolutePath}", e)
            false
        }
    }

    private fun hasCustomSection(
        file: File,
        key: String,
    ): Boolean =
        try {
            val existing = file.readText(Charsets.UTF_8)
            existing.contains("\n\n$key:\n") || existing.startsWith("$key:\n")
        } catch (_: Exception) {
            false
        }

    private fun getCrashFallbackReason(
        logFile: File,
        breadcrumbs: String,
    ): String? {
        val firstLine = readCrashReportFirstLine(logFile)
        if (firstLine != null && firstLine.startsWith("xcrash error:")) {
            return firstLine
        }
        if (!hasCustomSection(logFile, HEADER_SECTION)) {
            return "$HEADER_SECTION missing from xCrash report"
        }
        if (breadcrumbs.isNotBlank() && !hasCustomSection(logFile, BREADCRUMBS_SECTION)) {
            return "$BREADCRUMBS_SECTION missing from xCrash report"
        }
        return null
    }

    private fun readCrashReportFirstLine(file: File): String? =
        try {
            if (!file.isFile) return null
            file.bufferedReader(Charsets.UTF_8).use { reader ->
                reader.readLine()?.trim()?.takeUnless { it.isEmpty() }
            }
        } catch (_: Exception) {
            null
        }

    private fun writeCrashFallbackReport(
        context: Context,
        logFile: File?,
        header: String,
        reason: String,
        emergency: String?,
        breadcrumbs: String,
    ): File? {
        val crashDir = getTombstoneDir(context)
        val fallbackKind = if (emergency.isNullOrBlank()) "degraded" else "emergency"
        val fallback = File(crashDir, "crash_error_${fallbackKind}_${System.currentTimeMillis()}.txt")

        return try {
            crashDir.mkdirs()
            fallback.bufferedWriter(Charsets.UTF_8).use { writer ->
                if (header.isNotBlank()) {
                    writer.appendLine("$HEADER_SECTION:")
                    writer.appendLine(header.trimEnd())
                    writer.appendLine()
                }
                if (logFile != null) {
                    writer.appendLine("xcrash log path:")
                    writer.appendLine(logFile.absolutePath)
                    writer.appendLine()
                }
                writer.appendLine("xcrash fallback reason:")
                writer.appendLine(reason.trimEnd())
                writer.appendLine()
                if (!emergency.isNullOrBlank()) {
                    writer.appendLine("xcrash emergency:")
                    writer.appendLine(emergency.trimEnd())
                    writer.appendLine()
                }
                if (breadcrumbs.isNotBlank()) {
                    writer.appendLine("$BREADCRUMBS_SECTION:")
                    writer.appendLine(breadcrumbs.trimEnd())
                    writer.appendLine()
                }
            }
            fallback
        } catch (e: Exception) {
            Log.w(TAG, "Failed to write crash fallback report", e)
            null
        }
    }

    private fun isCrashReportFile(file: File): Boolean {
        if (!file.isFile) return false
        return file.name.startsWith("tombstone_") || file.name.startsWith("crash_error_")
    }

    // JNI declarations -- implemented in android_crash_handler.c
    private external fun nativeInstallCrashHandler(
        crashDir: String,
        installHeader: String,
    )

    private external fun nativeGetBreadcrumbReport(): String?

    private external fun nativeGetInstalledHeader(): String?
}
