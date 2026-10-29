package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.FilterInputStream
import java.io.IOException
import java.io.InputStream

class NativeHogArchiveTest {
    @Test
    fun readsMixedHeadersThroughProductionNativeBridge() {
        for (signature in listOf("DHF", "D2X")) {
            for (width in listOf(1, 12, 13, 14, 254, 255)) {
                val name = "L".repeat(width)
                val data =
                    hog(
                        signature,
                        listOf(
                            Triple("empty", false, byteArrayOf()),
                            Triple(name, true, byteArrayOf(42)),
                            Triple("ABCDEFGHIJKLM", false, byteArrayOf(9)),
                        ),
                    )
                val entries = mutableListOf<NativeHogArchive.Entry>()
                val payloads = mutableListOf<List<Byte>>()
                val oneByteReads =
                    object : FilterInputStream(data.inputStream()) {
                        override fun read(
                            bytes: ByteArray,
                            offset: Int,
                            length: Int,
                        ): Int = `in`.read(bytes, offset, minOf(1, length))
                    }
                NativeHogArchive.visit(oneByteReads, "mixed", expectedSize = data.size.toLong()) { entry, input ->
                    entries += entry
                    payloads += input.readBytes().toList()
                }
                assertEquals(listOf("empty", name, "ABCDEFGHIJKLM"), entries.map { it.name })
                assertEquals(listOf(false, true, false), entries.map { it.extended })
                assertEquals(listOf(17, 273, 17), entries.map { it.headerBytes })
                assertEquals(listOf(emptyList<Byte>(), listOf(42.toByte()), listOf(9.toByte())), payloads)
            }
        }
    }

    @Test
    fun rejectsEveryPartialHeaderAndPayloadAndPreservesEntryBoundaries() {
        val data =
            hog("D2X", listOf(Triple("first", true, byteArrayOf(1, 2, 3)), Triple("last", false, byteArrayOf(4, 5))))
        for (cut in 0 until data.size) {
            if (cut == 3 || cut == 279) continue
            assertThrows("cut=$cut", IOException::class.java) {
                NativeHogArchive.visit(data.copyOf(cut).inputStream(), "truncated") { _, _ -> }
            }
        }
        var retained: InputStream? = null
        NativeHogArchive.visit(data.inputStream(), "bounds") { entry, input ->
            if (entry.name == "first") {
                retained = input
                assertEquals(1, input.read())
                input.close()
            } else {
                assertThrows(IOException::class.java) { retained!!.read() }
                assertArrayEquals(byteArrayOf(4, 5), input.readBytes())
                assertEquals(-1, input.read())
            }
        }
        assertThrows(IOException::class.java) { retained!!.read() }
    }

    @Test
    fun rejectsMalformedNamesDuplicateAliasesAndInvalidSignedLengths() {
        for (name in listOf("", ".", "..", "bad/name", "bad\\name", "bad:name", "bad\nname", "x".repeat(256))) {
            assertThrows(name, IOException::class.java) {
                NativeHogArchive.visit(
                    hog("DHF", listOf(Triple(name, true, byteArrayOf(1)))).inputStream(),
                    "name",
                ) { _, _ -> }
            }
        }
        val duplicate =
            hog("D2X", listOf(Triple("track.mid", false, byteArrayOf(1)), Triple("TRACK.MID", true, byteArrayOf(2))))
        assertThrows(
            IOException::class.java,
        ) { NativeHogArchive.visit(duplicate.inputStream(), "duplicate") { _, _ -> } }
        val invalid = hog("D2X", listOf(Triple("valid", true, byteArrayOf(1))))
        invalid[16] = 0
        invalid[17] = 0
        invalid[18] = 0
        invalid[19] = 0x80.toByte()
        assertThrows(IOException::class.java) { NativeHogArchive.visit(invalid.inputStream(), "size") { _, _ -> } }
        invalid[0] = 'X'.code.toByte()
        assertThrows(IOException::class.java) { NativeHogArchive.visit(invalid.inputStream(), "signature") { _, _ -> } }
    }

    @Test
    fun keepsNativeStreamErrorsAndBudgetsAndDoesNotCloseCaller() {
        val data = hog("DHF", listOf(Triple("first", false, byteArrayOf(1, 2)), Triple("last", true, byteArrayOf(3))))
        for (budget in listOf(
            ExtractionBudget(maxEntries = 1),
            ExtractionBudget(maxEntryBytes = 1),
            ExtractionBudget(maxTotalBytes = 2),
        )) {
            assertThrows(
                IOException::class.java,
            ) { NativeHogArchive.visit(data.inputStream(), "budget", budget) { _, _ -> } }
        }
        var closed = false
        val input =
            object : FilterInputStream(data.inputStream()) {
                override fun close() {
                    closed = true
                }
            }
        NativeHogArchive.visit(input, "lifetime") { _, payload -> payload.close() }
        assertFalse(closed)
        val failure = IOException("source failed")
        val broken =
            object : InputStream() {
                override fun read(): Int = throw failure
            }
        assertEquals(
            failure,
            assertThrows(IOException::class.java) { NativeHogArchive.visit(broken, "failure") { _, _ -> } },
        )
        val stuck =
            object : InputStream() {
                override fun read() = 0

                override fun read(
                    bytes: ByteArray,
                    offset: Int,
                    length: Int,
                ) = 0
            }
        assertThrows(IOException::class.java) { NativeHogArchive.visit(stuck, "zero read") { _, _ -> } }
        assertThrows(IOException::class.java) {
            NativeHogArchive.visit(
                data.inputStream(),
                "changed",
                expectedSize =
                    data.size + 1L,
            ) { _, _ -> }
        }
    }

    @Test
    fun scansLargePayloadWithoutWholeArchiveAllocation() {
        val payloadSize = 196_505_200L
        val header =
            ByteArrayOutputStream()
                .apply {
                    write("D2X".toByteArray())
                    write(ByteArray(13).also { "large.bin".toByteArray().copyInto(it) })
                    repeat(4) { write((payloadSize ushr (it * 8)).toInt() and 255) }
                }.toByteArray()
        var position = 0L
        var largestRead = 0
        val source =
            object : InputStream() {
                override fun read(): Int = error("Unexpected unbuffered read")

                override fun read(
                    bytes: ByteArray,
                    offset: Int,
                    length: Int,
                ): Int {
                    largestRead = maxOf(largestRead, length)
                    val count = minOf(length.toLong(), header.size + payloadSize - position).toInt()
                    if (count == 0) return -1
                    repeat(count) {
                        bytes[offset + it] =
                            if (position + it < header.size) header[(position + it).toInt()] else 0
                    }
                    position += count
                    return count
                }
            }
        var count = 0
        NativeHogArchive.visit(source, "large", expectedSize = header.size + payloadSize) { entry, _ ->
            assertEquals(payloadSize, entry.size)
            count++
        }
        assertEquals(1, count)
        assertTrue("Largest read was $largestRead", largestRead <= 64 * 1024)
    }

    companion object {
        internal fun hog(
            signature: String,
            members: List<Triple<String, Boolean, ByteArray>>,
        ): ByteArray =
            ByteArrayOutputStream()
                .apply {
                    write(signature.toByteArray(Charsets.US_ASCII))
                    for ((name, extended, payload) in members) {
                        write(
                            ByteArray(
                                13,
                            ).also { (if (extended) "alias" else name).toByteArray(Charsets.ISO_8859_1).copyInto(it) },
                        )
                        val size = if (extended) -payload.size else payload.size
                        repeat(4) { write(size ushr (it * 8) and 255) }
                        if (extended) write(ByteArray(256).also { name.toByteArray(Charsets.ISO_8859_1).copyInto(it) })
                        write(payload)
                    }
                }.toByteArray()
    }
}
