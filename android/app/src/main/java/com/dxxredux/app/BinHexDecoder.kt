package com.dxxredux.app

import java.io.ByteArrayOutputStream
import java.io.File
import java.io.InputStream

internal object BinHexDecoder {
    private const val MARKER = "BinHex 4.0"
    private const val RLE_MARKER = 0x90
    private const val ALPHABET =
        "!\"#\$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr"
    private val reverse =
        IntArray(128) { -1 }.also { table ->
            ALPHABET.forEachIndexed { index, c -> table[c.code] = index }
        }

    fun decodeDataFork(
        input: InputStream,
        output: File,
    ): Long {
        val text = input.readBytes().toString(Charsets.US_ASCII)
        val markerIndex = text.indexOf(MARKER, ignoreCase = true).coerceAtLeast(0)
        val start = text.indexOf(':', markerIndex)
        val end = if (start >= 0) text.indexOf(':', start + 1) else -1
        require(start >= 0 && end > start) { "Missing BinHex payload delimiters" }

        val decoded = decodeRle(decodeSixBit(text, start + 1, end))
        require(decoded.isNotEmpty()) { "Empty BinHex payload" }

        val nameLen = decoded[0].toInt() and 0xff
        val header = 1 + nameLen + 1
        require(header + 20 <= decoded.size) { "Truncated BinHex header" }

        val dataLen = readBe32(decoded, header + 10)
        val dataStart = header + 20
        require(dataLen >= 0 && dataStart + dataLen <= decoded.size) { "Truncated BinHex data fork" }

        output.parentFile?.mkdirs()
        output.outputStream().use { it.write(decoded, dataStart, dataLen) }
        return dataLen.toLong()
    }

    private fun decodeSixBit(
        text: String,
        start: Int,
        end: Int,
    ): ByteArray {
        val out = ByteArrayOutputStream((end - start) * 3 / 4)
        var bits = 0
        var bitCount = 0
        for (i in start until end) {
            val c = text[i]
            if (c.isWhitespace()) continue
            val value = if (c.code < reverse.size) reverse[c.code] else -1
            require(value >= 0) { "Invalid BinHex character" }
            bits = (bits shl 6) or value
            bitCount += 6
            while (bitCount >= 8) {
                bitCount -= 8
                out.write((bits shr bitCount) and 0xff)
            }
        }
        return out.toByteArray()
    }

    private fun decodeRle(raw: ByteArray): ByteArray {
        val out = ByteArrayOutputStream(raw.size)
        var previous = -1
        var i = 0
        while (i < raw.size) {
            val b = raw[i].toInt() and 0xff
            i++
            if (b == RLE_MARKER) {
                require(i < raw.size) { "Truncated BinHex RLE marker" }
                val count = raw[i].toInt() and 0xff
                i++
                if (count == 0) {
                    out.write(RLE_MARKER)
                    previous = RLE_MARKER
                } else {
                    require(previous >= 0) { "BinHex RLE repeat without previous byte" }
                    repeat(count - 1) { out.write(previous) }
                }
            } else {
                out.write(b)
                previous = b
            }
        }
        return out.toByteArray()
    }

    private fun readBe32(
        bytes: ByteArray,
        offset: Int,
    ): Int =
        ((bytes[offset].toInt() and 0xff) shl 24) or
            ((bytes[offset + 1].toInt() and 0xff) shl 16) or
            ((bytes[offset + 2].toInt() and 0xff) shl 8) or
            (bytes[offset + 3].toInt() and 0xff)
}
