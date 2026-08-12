package com.dxxredux.app

import java.util.Locale

internal fun parseGogFileList(raw: Array<String>): List<GogImportBridge.GogFile>? {
    val files =
        raw.map { entry ->
            val parts = entry.split("|", limit = 3)
            val size = parts.getOrNull(1)?.toLongOrNull()
            val crc = parts.getOrNull(2)?.toLongOrNull()
            if (parts[0].isEmpty() || size == null || size < 0 ||
                (parts.size == 3 && (crc == null || crc !in 0..0xffffffffL))
            ) {
                return null
            }
            GogImportBridge.GogFile(parts[0], size, crc)
        }
    return files.takeIf {
        it.map { file -> file.name.lowercase(Locale.US) }.distinct().size == it.size
    }
}

/**
 * JNI bridge for GOG installer extraction (.exe InnoSetup, .pkg Mac).
 *
 * Wraps the native InnoSetup and Mac .pkg readers for use during
 * the GOG installer import flow in SetupActivity.
 */
object GogImportBridge {
    const val EXTRACT_CANCELLED = -3
    private const val TAG = "DXX-GogImport"

    init {
        System.loadLibrary("dxx-redux-d2")
    }

    /** A game file found inside a GOG installer */
    data class GogFile(
        val name: String,
        val size: Long,
        val crc32: Long? = null,
    )

    /** Check if a filename is a GOG audio file (.gog or .inst) */
    fun isAudioFile(name: String): Boolean = AndroidGameFileExtensions.isGogAudioFile(name)

    /**
     * Detect the installer format from a file path.
     *
     * @param path Filesystem path to the installer
     * @return "innosetup", "pkg", or "unknown"
     */
    fun detectFormat(path: String): String = nativeDetectFormat(path)

    /**
     * List game files in a GOG installer.
     *
     * @param path Filesystem path to the installer
     * @return List of game files, or null on error
     */
    fun listFiles(path: String): List<GogFile>? {
        val raw = nativeListFiles(path) ?: return null
        return parseFileList(raw)
    }

    /** List game files from an already-open InnoSetup installer fd */
    fun listFilesFromFd(fd: Int): List<GogFile>? {
        val raw = nativeListFilesFromFd(fd) ?: return null
        return parseFileList(raw)
    }

    internal fun parseFileList(raw: Array<String>): List<GogFile>? = parseGogFileList(raw)

    /** Callback interface for extraction progress (same as DiscImportBridge) */
    interface ExtractProgress {
        /** Called with current file name and byte progress. Return non-zero to cancel. */
        fun onProgress(
            currentFile: String,
            bytesDone: Long,
            bytesTotal: Long,
        ): Int
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
        path: String,
        outputDir: String,
        progress: ExtractProgress? = null,
        includeAudio: Boolean = true,
        expectedFiles: List<GogFile>? = null,
    ): Int {
        val currentFiles = listFiles(path) ?: return -1
        if (expectedFiles != null && currentFiles != expectedFiles) return -1
        val manifest = expectedFiles ?: currentFiles
        val expectedEntries =
            manifest
                .map { file ->
                    val crc = file.crc32 ?: -1L
                    "${file.name}|${file.size}|$crc"
                }.toTypedArray()
        return nativeExtractFiles(path, outputDir, progress, includeAudio, expectedEntries)
    }

    /** Extract game files from an already-open InnoSetup installer fd */
    fun extractFilesFromFd(
        fd: Int,
        outputDir: String,
        progress: ExtractProgress? = null,
        includeAudio: Boolean = true,
    ): Int = nativeExtractFilesFromFd(fd, outputDir, progress, includeAudio)

    // ── Native methods ────────────────────────────────────────────

    private external fun nativeDetectFormat(path: String): String

    private external fun nativeListFiles(path: String): Array<String>?

    private external fun nativeListFilesFromFd(fd: Int): Array<String>?

    private external fun nativeExtractFiles(
        path: String,
        outputDir: String,
        progress: ExtractProgress?,
        includeAudio: Boolean,
        expectedEntries: Array<String>,
    ): Int

    private external fun nativeExtractFilesFromFd(
        fd: Int,
        outputDir: String,
        progress: ExtractProgress?,
        includeAudio: Boolean,
    ): Int
}
