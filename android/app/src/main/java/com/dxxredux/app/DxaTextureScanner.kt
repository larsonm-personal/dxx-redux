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
    // Shared constant: must match min(..., 2048) in d2/arch/ogl/gr.c
    const val ENGINE_TEXTURE_CAP = 2048

    data class ScanResult(
        val maxWidth: Int,
        val maxHeight: Int,
        val textureCount: Int,
        val oversizedCount: Int,
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
                for (entry in zip.entries()) {
                    val name = entry.name.lowercase()
                    if (entry.isDirectory) continue
                    val dims = when {
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
                        if (pow2w > ENGINE_TEXTURE_CAP || pow2h > ENGINE_TEXTURE_CAP) oversized++
                    }
                }
                ScanResult(maxW, maxH, count, oversized)
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to scan ${dxaFile.name}", e)
            null
        }
    }

    /** Read width/height from a PNG IHDR chunk (bytes 16-23). */
    private fun readPngDims(input: InputStream): Pair<Int, Int>? {
        val buf = ByteArray(24)
        var read = 0
        while (read < 24) {
            val n = input.read(buf, read, 24 - read)
            if (n <= 0) return null
            read += n
        }
        // PNG magic: 89 50 4E 47
        if (buf[0] != 0x89.toByte() || buf[1] != 0x50.toByte()) return null
        val w = (buf[16].toInt() and 0xFF shl 24) or
            (buf[17].toInt() and 0xFF shl 16) or
            (buf[18].toInt() and 0xFF shl 8) or
            (buf[19].toInt() and 0xFF)
        val h = (buf[20].toInt() and 0xFF shl 24) or
            (buf[21].toInt() and 0xFF shl 16) or
            (buf[22].toInt() and 0xFF shl 8) or
            (buf[23].toInt() and 0xFF)
        return if (w in 1..65536 && h in 1..65536) Pair(w, h) else null
    }

    /** Read width/height from a TGA header (bytes 12-15). */
    private fun readTgaDims(input: InputStream): Pair<Int, Int>? {
        val buf = ByteArray(18)
        var read = 0
        while (read < 18) {
            val n = input.read(buf, read, 18 - read)
            if (n <= 0) return null
            read += n
        }
        val w = (buf[12].toInt() and 0xFF) or (buf[13].toInt() and 0xFF shl 8)
        val h = (buf[14].toInt() and 0xFF) or (buf[15].toInt() and 0xFF shl 8)
        return if (w in 1..65536 && h in 1..65536) Pair(w, h) else null
    }

    /** Match the engine's pow2ize: round up to next power of 2. */
    private fun pow2ize(v: Int): Int {
        var p = 1
        while (p < v) p = p shl 1
        return p
    }
}
