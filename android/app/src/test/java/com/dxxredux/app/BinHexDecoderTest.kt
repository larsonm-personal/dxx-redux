package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertThrows
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.File

class BinHexDecoderTest {
    @Test
    fun decodesDataForkFromBinHexTextWithPreamble() {
        val expected = "demo-data".toByteArray(Charsets.US_ASCII)
        val hqx = buildBinHex(dataFork = expected)
        val output = File.createTempFile("binhex-decoder", ".sit")
        output.deleteOnExit()

        val written = BinHexDecoder.decodeDataFork(hqx.byteInputStream(Charsets.US_ASCII), output)

        assertArrayEquals(expected, output.readBytes())
        org.junit.Assert.assertEquals(expected.size.toLong(), written)
    }

    @Test
    fun rejectsMissingCommentAndCorruptForksWithoutPublishing() {
        val expected = "demo-data".toByteArray(Charsets.US_ASCII)
        val valid = buildBinHex(dataFork = expected)
        val output = File.createTempFile("binhex-decoder", ".sit").apply { writeText("old") }
        output.deleteOnExit()

        val missingComment = valid.replace("(This file must be converted with BinHex 4.0)", "not binhex")
        assertThrows(IllegalArgumentException::class.java) {
            BinHexDecoder.decodeDataFork(missingComment.byteInputStream(Charsets.US_ASCII), output)
        }
        assertArrayEquals("old".toByteArray(), output.readBytes())

        val payloadStart = valid.indexOf(':') + 1
        val corrupt = valid.replaceRange(payloadStart + 20, payloadStart + 21, if (valid[payloadStart + 20] == '!') "\"" else "!")
        assertThrows(IllegalArgumentException::class.java) {
            BinHexDecoder.decodeDataFork(corrupt.byteInputStream(Charsets.US_ASCII), output)
        }
        assertArrayEquals("old".toByteArray(), output.readBytes())
    }

    @Test
    fun validatesResourceForkAndTerminalEnvelope() {
        val valid = buildBinHex("data".toByteArray(), "resource".toByteArray())
        val output = File.createTempFile("binhex-decoder", ".sit")
        output.deleteOnExit()
        assertArrayEquals(
            "data".toByteArray(),
            output.apply { BinHexDecoder.decodeDataFork(valid.byteInputStream(), this) }.readBytes(),
        )

        assertThrows(IllegalArgumentException::class.java) {
            BinHexDecoder.decodeDataFork("${valid}junk".byteInputStream(), output)
        }
    }

    private fun buildBinHex(
        dataFork: ByteArray,
        resourceFork: ByteArray = byteArrayOf(),
    ): String {
        val decoded = ByteArrayOutputStream()
        decoded.write(byteArrayOf(4))
        decoded.write("test".toByteArray(Charsets.US_ASCII))
        decoded.write(0)
        decoded.write("SITD".toByteArray(Charsets.US_ASCII))
        decoded.write("SIT!".toByteArray(Charsets.US_ASCII))
        decoded.write(byteArrayOf(0, 0))
        writeBe32(decoded, dataFork.size)
        writeBe32(decoded, resourceFork.size)
        writeBe16(decoded, crc16(decoded.toByteArray()))
        decoded.write(dataFork)
        writeBe16(decoded, crc16(dataFork))
        decoded.write(resourceFork)
        writeBe16(decoded, crc16(resourceFork))
        return "mail header\n(This file must be converted with BinHex 4.0)\n:${encodeSixBit(decoded.toByteArray())}:"
    }

    private fun writeBe16(
        out: ByteArrayOutputStream,
        value: Int,
    ) {
        out.write((value ushr 8) and 0xff)
        out.write(value and 0xff)
    }

    private fun writeBe32(
        out: ByteArrayOutputStream,
        value: Int,
    ) {
        out.write((value ushr 24) and 0xff)
        out.write((value ushr 16) and 0xff)
        out.write((value ushr 8) and 0xff)
        out.write(value and 0xff)
    }

    private fun encodeSixBit(bytes: ByteArray): String {
        val alphabet = "!\"#\$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr"
        val out = StringBuilder()
        var bits = 0
        var bitCount = 0
        for (b in bytes) {
            bits = (bits shl 8) or (b.toInt() and 0xff)
            bitCount += 8
            while (bitCount >= 6) {
                bitCount -= 6
                out.append(alphabet[(bits shr bitCount) and 0x3f])
            }
        }
        if (bitCount > 0) {
            out.append(alphabet[(bits shl (6 - bitCount)) and 0x3f])
        }
        return out.toString()
    }

    private fun crc16(bytes: ByteArray): Int {
        var crc = 0
        for (value in bytes.map { it.toInt() and 0xff } + listOf(0, 0)) {
            crc = crc xor (value shl 8)
            repeat(8) {
                crc = if (crc and 0x8000 != 0) (crc shl 1) xor 0x1021 else crc shl 1
                crc = crc and 0xffff
            }
        }
        return crc
    }
}
