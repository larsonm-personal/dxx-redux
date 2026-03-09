package com.dxxredux.app

import android.os.ParcelFileDescriptor
import android.util.Log
import java.io.File

/**
 * JNI bridge for BIN/CUE disc import operations.
 *
 * Wraps the native CUE parser and ISO 9660 reader for use during
 * the CD image import flow in SetupActivity.
 */
object DiscImportBridge {

    private const val TAG = "DXX-DiscImport"

    init {
        System.loadLibrary("d2x-redux")
    }

    /* ── CUE parsing ─────────────────────────────────────────────── */

    /** Parsed track from a CUE sheet */
    data class CueTrack(
        val trackNum: Int,
        val type: Int,        // 0 = data, 1 = audio
        val fileIndex: Int,
        val startSector: Int,
        val numSectors: Int,
        val title: String
    ) {
        val isData get() = type == 0
        val isAudio get() = type == 1
    }

    /**
     * Parse a CUE file and return its tracks.
     *
     * @param cuePath  Filesystem path to the .cue file
     * @param binSizes Array of BIN file sizes (one per FILE directive in the CUE)
     * @return List of tracks, or null on parse failure
     */
    fun parseCue(cuePath: String, binSizes: LongArray): List<CueTrack>? {
        val raw = nativeParseCue(cuePath, binSizes) ?: return null
        val titles = nativeGetCueTitles(cuePath, binSizes)
        val numTracks = raw.size / 5
        return (0 until numTracks).map { i ->
            CueTrack(
                trackNum = raw[i * 5],
                type = raw[i * 5 + 1],
                fileIndex = raw[i * 5 + 2],
                startSector = raw[i * 5 + 3],
                numSectors = raw[i * 5 + 4],
                title = titles?.getOrNull(i) ?: ""
            )
        }
    }

    /* ── ISO 9660 listing ────────────────────────────────────────── */

    /** A file found on an ISO 9660 data track */
    data class IsoFile(val path: String, val size: Long)

    /**
     * List files on an ISO 9660 data track within a BIN file.
     *
     * @param binFd        Open file descriptor for the BIN file
     * @param trackStart   Start sector of the data track
     * @param trackSectors Number of sectors in the data track
     * @return List of files, or null on error
     */
    fun listIsoFiles(binFd: Int, trackStart: Int, trackSectors: Int): List<IsoFile>? {
        val raw = nativeListIsoFiles(binFd, trackStart, trackSectors) ?: return null
        return raw.mapNotNull { entry ->
            val parts = entry.split("|", limit = 2)
            if (parts.size == 2) {
                IsoFile(parts[0], parts[1].toLongOrNull() ?: 0L)
            } else null
        }
    }

    /**
     * List files from a BIN file path (opens the fd internally).
     */
    fun listIsoFiles(binPath: String, trackStart: Int, trackSectors: Int): List<IsoFile>? {
        val pfd = ParcelFileDescriptor.open(
            File(binPath), ParcelFileDescriptor.MODE_READ_ONLY
        )
        return try {
            listIsoFiles(pfd.fd, trackStart, trackSectors)
        } finally {
            pfd.close()
        }
    }

    /* ── ISO 9660 extraction ─────────────────────────────────────── */

    /** Callback interface for extraction progress */
    interface ExtractProgress {
        /** Called with current file name and byte progress. Return non-zero to cancel. */
        fun onProgress(currentFile: String, bytesDone: Long, bytesTotal: Long): Int
    }

    /**
     * Extract game files from an ISO 9660 data track.
     *
     * @param binFd        Open file descriptor for the BIN file
     * @param trackStart   Start sector of the data track
     * @param trackSectors Number of sectors in the data track
     * @param outputDir    Directory to extract files into (must exist)
     * @param progress     Optional progress callback
     * @return Number of files extracted, or -1 on error
     */
    fun extractIsoFiles(
        binFd: Int, trackStart: Int, trackSectors: Int,
        outputDir: String, progress: ExtractProgress? = null
    ): Int {
        return nativeExtractIsoFiles(binFd, trackStart, trackSectors, outputDir, progress)
    }

    /**
     * Extract from a BIN file path (opens the fd internally).
     */
    fun extractIsoFiles(
        binPath: String, trackStart: Int, trackSectors: Int,
        outputDir: String, progress: ExtractProgress? = null
    ): Int {
        val pfd = ParcelFileDescriptor.open(
            File(binPath), ParcelFileDescriptor.MODE_READ_ONLY
        )
        return try {
            extractIsoFiles(pfd.fd, trackStart, trackSectors, outputDir, progress)
        } finally {
            pfd.close()
        }
    }

    /* ── SOW (ARJ) archive operations ────────────────────────────── */

    /**
     * Scan a directory tree for .sow files.
     *
     * @param dirPath Directory to scan recursively
     * @return List of absolute paths to .sow files found, or null on error
     */
    fun scanSowFiles(dirPath: String): List<String>? {
        val raw = nativeScanSowFiles(dirPath) ?: return null
        return raw.toList()
    }

    /**
     * Extract game files from a .sow (ARJ) archive.
     * Internal archive paths are flattened to just filenames.
     *
     * @param sowPath   Path to the .sow file
     * @param outputDir Directory to extract game files into
     * @param progress  Optional progress callback
     * @return Number of files extracted, or -1 on error
     */
    fun extractSowFiles(
        sowPath: String, outputDir: String,
        progress: ExtractProgress? = null
    ): Int {
        return nativeExtractSowFiles(sowPath, outputDir, progress)
    }

    /* ── Native methods ──────────────────────────────────────────── */

    private external fun nativeParseCue(cuePath: String, binSizes: LongArray): IntArray?
    private external fun nativeGetCueTitles(cuePath: String, binSizes: LongArray): Array<String>?
    private external fun nativeListIsoFiles(binFd: Int, trackStart: Int, trackSectors: Int): Array<String>?
    private external fun nativeExtractIsoFiles(
        binFd: Int, trackStart: Int, trackSectors: Int,
        outputDir: String, progress: ExtractProgress?
    ): Int

    private external fun nativeScanSowFiles(dirPath: String): Array<String>?
    private external fun nativeExtractSowFiles(
        sowPath: String, outputDir: String, progress: ExtractProgress?
    ): Int
}
