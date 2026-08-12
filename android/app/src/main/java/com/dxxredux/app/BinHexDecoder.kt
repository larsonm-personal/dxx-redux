package com.dxxredux.app

import java.io.ByteArrayOutputStream
import java.io.File
import java.io.InputStream
import java.nio.file.Files
import java.nio.file.StandardCopyOption

internal object BinHexDecoder {
    private const val REQUIRED_COMMENT = "(This file must be converted with BinHex 4.0)"
    private const val RLE_MARKER = 0x90
    private const val MAX_DECODED_BYTES = 512 * 1024 * 1024
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
        val text = readBounded(input).toString(Charsets.US_ASCII)
        val marker = Regex("(?m)^\\Q$REQUIRED_COMMENT\\E\\r?$").find(text)
        require(marker != null) { "Missing BinHex identifying comment" }
        val startMarker = text.indexOf("\n:", marker.range.last + 1)
        val start = if (startMarker >= 0) startMarker + 1 else -1
        val end = if (start >= 0) text.indexOf(':', start + 1) else -1
        require(start >= 0 && end > start) { "Missing BinHex payload delimiters" }
        require(text.substring(end + 1).all { it.isWhitespace() }) { "Trailing data after BinHex payload" }

        val decoded = decodeRle(decodeSixBit(text, start + 1, end))
        require(decoded.isNotEmpty()) { "Empty BinHex payload" }

        val nameLen = decoded[0].toInt() and 0xff
        require(nameLen in 1..63) { "Invalid BinHex filename length" }
        val header = 1 + nameLen + 1
        require(header + 20 <= decoded.size) { "Truncated BinHex header" }
        require(decoded[1 + nameLen].toInt() == 0) { "Unsupported BinHex version" }

        val headerCrcOffset = header + 18
        require(readBe16(decoded, headerCrcOffset) == crc16(decoded, 0, headerCrcOffset)) {
            "BinHex header CRC mismatch"
        }
        val dataLen = readBe32(decoded, header + 10)
        val resourceLen = readBe32(decoded, header + 14)
        val dataStart = header + 20
        val dataEnd = Math.addExact(dataStart.toLong(), dataLen)
        val dataCrcOffset = dataEnd
        val resourceStart = Math.addExact(dataCrcOffset, 2L)
        val resourceEnd = Math.addExact(resourceStart, resourceLen)
        val resourceCrcOffset = resourceEnd
        val envelopeEnd = Math.addExact(resourceCrcOffset, 2L)
        require(envelopeEnd == decoded.size.toLong()) { "Invalid BinHex fork spans" }
        require(readBe16(decoded, dataCrcOffset.toInt()) == crc16(decoded, dataStart, dataLen.toInt())) {
            "BinHex data CRC mismatch"
        }
        require(
            readBe16(decoded, resourceCrcOffset.toInt()) ==
                crc16(decoded, resourceStart.toInt(), resourceLen.toInt()),
        ) { "BinHex resource CRC mismatch" }

        output.parentFile?.mkdirs()
        val temp = File.createTempFile("${output.name}.", ".tmp", output.parentFile)
        try {
            temp.outputStream().use { it.write(decoded, dataStart, dataLen.toInt()) }
            Files.move(
                temp.toPath(),
                output.toPath(),
                StandardCopyOption.ATOMIC_MOVE,
                StandardCopyOption.REPLACE_EXISTING,
            )
        } finally {
            temp.delete()
        }
        return dataLen
    }

    private fun readBounded(input: InputStream): ByteArray {
        val out = ByteArrayOutputStream()
        val buffer = ByteArray(DEFAULT_BUFFER_SIZE)
        var total = 0
        while (true) {
            val read = input.read(buffer)
            if (read < 0) break
            total = Math.addExact(total, read)
            require(total <= MAX_DECODED_BYTES) { "BinHex input is too large" }
            out.write(buffer, 0, read)
        }
        return out.toByteArray()
    }

    private fun decodeSixBit(
        text: String,
        start: Int,
        end: Int,
    ): ByteArray {
        val out = ByteArrayOutputStream((end - start) * 3 / 4)
        var bits = 0
        var bitCount = 0
        var lineCharacters = 0
        for (i in start until end) {
            val c = text[i]
            if (c == '\n' || c == '\r') {
                if (c == '\n') lineCharacters = 0
                continue
            }
            require(!c.isWhitespace()) { "Invalid BinHex whitespace" }
            lineCharacters++
            require(lineCharacters <= 64) { "BinHex line exceeds 64 characters" }
            val value = if (c.code < reverse.size) reverse[c.code] else -1
            require(value >= 0) { "Invalid BinHex character" }
            bits = (bits shl 6) or value
            bitCount += 6
            while (bitCount >= 8) {
                bitCount -= 8
                out.write((bits shr bitCount) and 0xff)
            }
        }
        require(bitCount == 0 || bits and ((1 shl bitCount) - 1) == 0) { "Nonzero BinHex tail bits" }
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
                    require(out.size().toLong() + count - 1 <= MAX_DECODED_BYTES) { "BinHex payload is too large" }
                    repeat(count - 1) { out.write(previous) }
                }
            } else {
                out.write(b)
                previous = b
            }
        }
        return out.toByteArray()
    }

    private fun readBe16(
        bytes: ByteArray,
        offset: Int,
    ): Int = ((bytes[offset].toInt() and 0xff) shl 8) or (bytes[offset + 1].toInt() and 0xff)

    private fun readBe32(
        bytes: ByteArray,
        offset: Int,
    ): Long =
        ((bytes[offset].toLong() and 0xff) shl 24) or
            ((bytes[offset + 1].toLong() and 0xff) shl 16) or
            ((bytes[offset + 2].toLong() and 0xff) shl 8) or
            (bytes[offset + 3].toLong() and 0xff)

    private fun crc16(
        bytes: ByteArray,
        offset: Int,
        length: Int,
    ): Int {
        var crc = 0
        repeat(length + 2) { index ->
            val value = if (index < length) bytes[offset + index].toInt() and 0xff else 0
            crc = crc xor (value shl 8)
            repeat(8) {
                crc = if (crc and 0x8000 != 0) (crc shl 1) xor 0x1021 else crc shl 1
                crc = crc and 0xffff
            }
        }
        return crc
    }
}
