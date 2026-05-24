package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
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

    private fun buildBinHex(dataFork: ByteArray): String {
        val decoded = ByteArrayOutputStream()
        decoded.write(byteArrayOf(4))
        decoded.write("test".toByteArray(Charsets.US_ASCII))
        decoded.write(0)
        decoded.write("SITD".toByteArray(Charsets.US_ASCII))
        decoded.write("SIT!".toByteArray(Charsets.US_ASCII))
        decoded.write(byteArrayOf(0, 0))
        writeBe32(decoded, dataFork.size)
        writeBe32(decoded, 0)
        decoded.write(byteArrayOf(0, 0))
        decoded.write(dataFork)
        decoded.write(byteArrayOf(0, 0, 0, 0))
        return "mail header\n(This file must be converted with BinHex 4.0)\n:${encodeSixBit(decoded.toByteArray())}:"
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
}
