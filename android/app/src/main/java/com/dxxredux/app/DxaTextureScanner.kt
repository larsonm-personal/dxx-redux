package com.dxxredux.app

import android.util.Log
import java.io.File
import java.io.InputStream
import java.util.zip.ZipFile

/**
 * Scans DXA (ZIP) archives for image file dimensions.
 * Used by the launcher to warn about oversized textures that the engine
 * will silently skip (engine caps at ogl_max_texture_size = min(GL_MAX, 2048)).
 *
 * ENGINE_TEXTURE_CAP must match the cap in d2/arch/ogl/gr.c ogl_get_verinfo().
 */
object DxaTextureScanner {
    private const val TAG = "DXX-DxaScan"
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
    )

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
                for (entry in zip.entries()) {
                    val name = entry.name.lowercase()
                    if (entry.isDirectory) continue
                    if (name.endsWith("_mask.png")) continue
                    val dims =
                        when {
                            name.endsWith(".ktx2") -> zip.getInputStream(entry).use { readKtx2Dims(it) }
                            name.endsWith(".png") -> zip.getInputStream(entry).use { readPngDims(it) }
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
}
