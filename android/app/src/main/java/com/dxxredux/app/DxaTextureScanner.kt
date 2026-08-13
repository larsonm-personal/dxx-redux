package com.dxxredux.app

import android.util.Log
import java.io.File
import java.io.InputStream
import java.util.zip.ZipFile

/**
 * Scans DXA (ZIP) archives for image file dimensions.
 * Used by the launcher to reject oversized textures before enabling a mod
 * (engine caps at ogl_max_texture_size = min(GL_MAX, 2048)).
 *
 * ENGINE_TEXTURE_CAP must match the cap in d2/arch/ogl/gr.c ogl_get_verinfo().
 */
object DxaTextureScanner {
    private const val TAG = "DXX-DxaScan"

    // Traversal limits must match pngfile.c native texture indexing.
    private const val MAX_ARCHIVE_ENTRIES = 65_536
    private const val MAX_DIRECTORIES = 16_384
    private const val MAX_TEXTURE_ENTRIES = 49_152
    private const val MAX_PATH_DEPTH = 64
    private const val MAX_PATH_BYTES = 4_096
    private const val MAX_TOTAL_PATH_BYTES = 16 * 1024 * 1024
    private val KTX2_IDENTIFIER =
        byteArrayOf(
            0xAB.toByte(),
            0x4B,
            0x54,
            0x58,
            0x20,
            0x32,
            0x30,
            0xBB.toByte(),
            0x0D,
            0x0A,
            0x1A,
            0x0A,
        )

    // Shared constant: must match min(..., 2048) in d2/arch/ogl/gr.c
    const val ENGINE_TEXTURE_CAP = 2048

    data class OversizedTexture(
        val name: String,
        val width: Int,
        val height: Int,
        val pow2Width: Int,
        val pow2Height: Int,
    )

    data class ScanResult(
        val maxWidth: Int,
        val maxHeight: Int,
        val textureCount: Int,
        val oversizedCount: Int,
        val oversizedEntries: List<OversizedTexture>,
        val rejectedReason: String? = null,
    ) {
        val canEnable: Boolean
            get() = rejectedReason == null && oversizedCount == 0
    }

    internal fun validateStructure(entryNames: Sequence<String>): String? {
        var entryCount = 0
        var textureCount = 0
        var totalPathBytes = 0L
        val directories = HashSet<String>()

        for (name in entryNames) {
            entryCount++
            if (entryCount > MAX_ARCHIVE_ENTRIES) return "archive has more than $MAX_ARCHIVE_ENTRIES entries"
            val pathBytes = name.toByteArray(Charsets.UTF_8).size
            if (pathBytes == 0 || pathBytes > MAX_PATH_BYTES) return "archive path exceeds $MAX_PATH_BYTES bytes"
            totalPathBytes += pathBytes.toLong() + 1L
            if (totalPathBytes > MAX_TOTAL_PATH_BYTES) return "archive paths exceed $MAX_TOTAL_PATH_BYTES bytes"
            if (name.startsWith('/') || '\\' in name || '\u0000' in name) return "archive contains an invalid path"

            val trimmed = name.trimEnd('/')
            var depth = 0
            var segmentStart = 0
            for (index in trimmed.indices) {
                if (trimmed[index] != '/') continue
                val segment = trimmed.substring(segmentStart, index)
                if (segment.isEmpty() || segment == "." || segment == "..") return "archive contains an invalid path"
                depth++
                if (depth > MAX_PATH_DEPTH) return "archive path exceeds $MAX_PATH_DEPTH directory components"
                directories += trimmed.substring(0, index)
                if (directories.size > MAX_DIRECTORIES) return "archive has more than $MAX_DIRECTORIES directories"
                segmentStart = index + 1
            }
            val leaf = trimmed.substring(segmentStart)
            if (leaf.isEmpty() || leaf == "." || leaf == "..") return "archive contains an invalid path"
            if (name.endsWith('/')) {
                depth++
                if (depth > MAX_PATH_DEPTH) return "archive path exceeds $MAX_PATH_DEPTH directory components"
                directories += trimmed
                if (directories.size > MAX_DIRECTORIES) return "archive has more than $MAX_DIRECTORIES directories"
            } else if (GameFileFormats.isTextureReplacement(trimmed)) {
                textureCount++
                if (textureCount >
                    MAX_TEXTURE_ENTRIES
                ) {
                    return "archive has more than $MAX_TEXTURE_ENTRIES texture entries"
                }
            }
        }
        return null
    }

    /** Scan a DXA file and return the max texture dimensions found. */
    fun scan(dxaFile: File): ScanResult? {
        if (!dxaFile.exists()) return null
        return try {
            ZipFile(dxaFile).use { zip ->
                var maxW = 0
                var maxH = 0
                var count = 0
                var oversized = 0
                val oversizedEntries = mutableListOf<OversizedTexture>()
                val rejectedReason = validateStructure(zip.entries().asSequence().map { it.name })
                if (rejectedReason != null) {
                    return@use ScanResult(0, 0, 0, 0, emptyList(), rejectedReason)
                }
                for (entry in zip.entries()) {
                    val name = entry.name.lowercase()
                    if (entry.isDirectory) continue
                    if (name.endsWith("_mask.png")) continue
                    if (!GameFileFormats.isTextureReplacement(name)) continue
                    val dims =
                        when {
                            name.endsWith(".ktx2") -> zip.getInputStream(entry).use { readKtx2Dims(it) }
                            name.endsWith(".png") -> zip.getInputStream(entry).use { readPngDims(it) }
                            name.endsWith(".jpg") -> zip.getInputStream(entry).use { readJpegDims(it) }
                            name.endsWith(".tga") -> zip.getInputStream(entry).use { readTgaDims(it) }
                            else -> null
                        }
                    if (dims != null) {
                        count++
                        if (dims.first > maxW) maxW = dims.first
                        if (dims.second > maxH) maxH = dims.second
                        val pow2w = pow2ize(dims.first)
                        val pow2h = pow2ize(dims.second)
                        if (pow2w > ENGINE_TEXTURE_CAP || pow2h > ENGINE_TEXTURE_CAP) {
                            oversized++
                            oversizedEntries +=
                                OversizedTexture(
                                    name = entry.name,
                                    width = dims.first,
                                    height = dims.second,
                                    pow2Width = pow2w,
                                    pow2Height = pow2h,
                                )
                        }
                    }
                }
                ScanResult(maxW, maxH, count, oversized, oversizedEntries)
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to scan ${dxaFile.name}", e)
            null
        }
    }

    /** Read width/height from a PNG IHDR chunk (bytes 16-23). */
    private fun readPngDims(input: InputStream): Pair<Int, Int>? {
        val buf = readPrefix(input, 24) ?: return null
        // PNG magic: 89 50 4E 47
        if (buf[0] != 0x89.toByte() || buf[1] != 0x50.toByte()) return null
        val w = readBe32(buf, 16)
        val h = readBe32(buf, 20)
        return if (w in 1..65536 && h in 1..65536) Pair(w, h) else null
    }

    /** Read width/height from a TGA header (bytes 12-15). */
    private fun readTgaDims(input: InputStream): Pair<Int, Int>? {
        val buf = readPrefix(input, 18) ?: return null
        val w = readLe16(buf, 12)
        val h = readLe16(buf, 14)
        return if (w in 1..65536 && h in 1..65536) Pair(w, h) else null
    }

    /** Read dimensions from a JPEG start-of-frame segment without decoding pixels. */
    private fun readJpegDims(input: InputStream): Pair<Int, Int>? {
        if (input.read() != 0xFF || input.read() != 0xD8) return null
        var scanned = 2
        while (scanned < 64 * 1024) {
            var markerStart = input.read()
            scanned++
            while (markerStart != -1 && markerStart != 0xFF) {
                markerStart = input.read()
                scanned++
            }
            if (markerStart == -1) return null
            var marker = input.read()
            scanned++
            while (marker == 0xFF) {
                marker = input.read()
                scanned++
            }
            if (marker == -1 || marker == 0xD9 || marker == 0xDA) return null
            if (marker == 0x01 || marker in 0xD0..0xD8) continue
            val length = readBe16(input) ?: return null
            scanned += 2
            if (length < 2) return null
            if (marker in JPEG_SOF_MARKERS) {
                if (length < 7 || input.read() == -1) return null
                val height = readBe16(input) ?: return null
                val width = readBe16(input) ?: return null
                return if (width > 0 && height > 0) Pair(width, height) else null
            }
            if (!skipExact(input, length - 2)) return null
            scanned += length - 2
        }
        return null
    }

    /** Read width/height from the fixed-width KTX2 header. */
    private fun readKtx2Dims(input: InputStream): Pair<Int, Int>? {
        val buf = readPrefix(input, 28) ?: return null
        if (!buf.copyOfRange(0, KTX2_IDENTIFIER.size).contentEquals(KTX2_IDENTIFIER)) return null
        val w = readLe32(buf, 20)
        val h = readLe32(buf, 24)
        return if (w in 1..65536 && h in 1..65536) Pair(w, h) else null
    }

    private fun readPrefix(
        input: InputStream,
        size: Int,
    ): ByteArray? {
        val buf = ByteArray(size)
        var read = 0
        while (read < size) {
            val n = input.read(buf, read, size - read)
            if (n <= 0) return null
            read += n
        }
        return buf
    }

    private fun readBe16(input: InputStream): Int? {
        val high = input.read()
        val low = input.read()
        return if (high >= 0 && low >= 0) (high shl 8) or low else null
    }

    private fun skipExact(
        input: InputStream,
        count: Int,
    ): Boolean {
        var remaining = count
        while (remaining > 0) {
            val skipped = input.skip(remaining.toLong()).toInt()
            if (skipped > 0) {
                remaining -= skipped
            } else if (input.read() >= 0) {
                remaining--
            } else {
                return false
            }
        }
        return true
    }

    private fun readBe32(
        buf: ByteArray,
        offset: Int,
    ): Int =
        ((buf[offset].toInt() and 0xFF) shl 24) or
            ((buf[offset + 1].toInt() and 0xFF) shl 16) or
            ((buf[offset + 2].toInt() and 0xFF) shl 8) or
            (buf[offset + 3].toInt() and 0xFF)

    private fun readLe32(
        buf: ByteArray,
        offset: Int,
    ): Int =
        (buf[offset].toInt() and 0xFF) or
            ((buf[offset + 1].toInt() and 0xFF) shl 8) or
            ((buf[offset + 2].toInt() and 0xFF) shl 16) or
            ((buf[offset + 3].toInt() and 0xFF) shl 24)

    private fun readLe16(
        buf: ByteArray,
        offset: Int,
    ): Int = (buf[offset].toInt() and 0xFF) or ((buf[offset + 1].toInt() and 0xFF) shl 8)

    /** Match the engine's pow2ize: round up to next power of 2. */
    private fun pow2ize(v: Int): Int {
        var p = 1
        while (p < v) p = p shl 1
        return p
    }

    private val JPEG_SOF_MARKERS =
        setOf(0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7, 0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF)
}
