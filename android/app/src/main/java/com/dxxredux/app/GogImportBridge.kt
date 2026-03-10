package com.dxxredux.app

import android.util.Log

/**
 * JNI bridge for GOG installer extraction (.exe InnoSetup, .pkg Mac).
 *
 * Wraps the native InnoSetup and Mac .pkg readers for use during
 * the GOG installer import flow in SetupActivity.
 */
object GogImportBridge {

    private const val TAG = "DXX-GogImport"

    init {
        System.loadLibrary("d2x-redux")
    }

    /** A game file found inside a GOG installer */
    data class GogFile(val name: String, val size: Long)

    /** Audio extensions extracted from GOG installers (.gog/.inst) */
    private val audioExtensions = setOf("gog", "inst")

    /** Check if a filename is a GOG audio file (.gog or .inst) */
    fun isAudioFile(name: String): Boolean {
        val ext = name.substringAfterLast('.', "").lowercase()
        return ext in audioExtensions
    }

    /**
     * Detect the installer format from a file path.
     *
     * @param path Filesystem path to the installer
     * @return "innosetup", "pkg", or "unknown"
     */
    fun detectFormat(path: String): String {
        return nativeDetectFormat(path)
    }

    /**
     * List game files in a GOG installer.
     *
     * @param path Filesystem path to the installer
     * @return List of game files, or null on error
     */
    fun listFiles(path: String): List<GogFile>? {
        val raw = nativeListFiles(path) ?: return null
        return raw.mapNotNull { entry ->
            val parts = entry.split("|", limit = 2)
            if (parts.size == 2) {
                GogFile(parts[0], parts[1].toLongOrNull() ?: 0L)
            } else null
        }
    }

    /** Callback interface for extraction progress (same as DiscImportBridge) */
    interface ExtractProgress {
        /** Called with current file name and byte progress. Return non-zero to cancel. */
        fun onProgress(currentFile: String, bytesDone: Long, bytesTotal: Long): Int
    }

    /**
     * Extract game files from a GOG installer.
     *
     * @param path         Filesystem path to the installer (.exe or .pkg)
     * @param outputDir    Directory to extract game files into (must exist)
     * @param progress     Optional progress callback
     * @param includeAudio Whether to extract .gog/.inst CD audio files
     * @return Number of files extracted, or -1 on error
     */
    fun extractFiles(
        path: String, outputDir: String,
        progress: ExtractProgress? = null,
        includeAudio: Boolean = true
    ): Int {
        return nativeExtractFiles(path, outputDir, progress, includeAudio)
    }

    /* ── Native methods ──────────────────────────────────────────── */

    private external fun nativeDetectFormat(path: String): String
    private external fun nativeListFiles(path: String): Array<String>?
    private external fun nativeExtractFiles(
        path: String, outputDir: String, progress: ExtractProgress?,
        includeAudio: Boolean
    ): Int
}
