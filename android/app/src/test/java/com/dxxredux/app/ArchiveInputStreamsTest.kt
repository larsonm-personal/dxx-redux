package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.InputStream
import java.io.SequenceInputStream
import java.nio.file.Files
import java.util.zip.CRC32
import java.util.zip.ZipEntry
import java.util.zip.ZipException
import java.util.zip.ZipOutputStream

class ArchiveInputStreamsTest {
    @Test
    fun opensZipAfterSelfExtractorPreamble() {
        val archive = "MZ fake self extractor".toByteArray(Charsets.US_ASCII) + makeZip()

        openZipInputStreamSkippingPreamble(ByteArrayInputStream(archive)).use { zip ->
            val entry = zip.nextEntry
            assertNotNull(entry)
            assertEquals("DESCENT1.SOW", entry.name)
            assertEquals("payload", zip.readBytes().toString(Charsets.US_ASCII))
        }
    }

    @Test
    fun rejectsStreamsWithoutZipLocalHeader() {
        assertThrows(ZipException::class.java) {
            openZipInputStreamSkippingPreamble(ByteArrayInputStream("not a zip".toByteArray()))
        }
    }

    @Test
    fun skipsInvalidAndPlausibleFalseLocalHeaders() {
        val invalidHeader = makeLocalHeader(method = 99, name = "bad")
        val plausibleEmptyHeader = makeLocalHeader(method = 0, name = "empty")
        val archive =
            "MZ".toByteArray(Charsets.US_ASCII) +
                invalidHeader +
                plausibleEmptyHeader +
                makeZip()

        openZipInputStreamSkippingPreamble(ByteArrayInputStream(archive)).use { zip ->
            val entry = zip.nextEntry
            assertNotNull(entry)
            assertEquals("DESCENT1.SOW", entry.name)
            assertEquals("payload", zip.readBytes().toString(Charsets.US_ASCII))
        }
    }

    @Test
    fun rejectsAnExcessiveSelfExtractorPreamble() {
        val archive = makeZip()
        val source = sourceAtSize(ExtractionLimits.MAX_ZIP_PREAMBLE_BYTES + 1 + archive.size, archive)
        val stageDir = Files.createTempDirectory("dxx-zip-test-").toFile()
        try {
            val error =
                assertThrows(ZipException::class.java) {
                    openZipInputStreamSkippingPreamble(source, stageDir)
                }
            assertTrue(error.message.orEmpty().startsWith("ZIP preamble exceeds"))
            assertTrue(stageDir.listFiles().orEmpty().isEmpty())
        } finally {
            stageDir.deleteRecursively()
        }
    }

    @Test
    fun acceptsExactSelfExtractorPreambleLimit() {
        val archive = makeZip()
        val source = sourceAtSize(ExtractionLimits.MAX_ZIP_PREAMBLE_BYTES + archive.size, archive)
        openZipInputStreamSkippingPreamble(source).use { zip ->
            assertEquals("DESCENT1.SOW", zip.nextEntry.name)
            assertEquals("payload", zip.readBytes().toString(Charsets.US_ASCII))
        }
    }

    @Test
    fun extractsOrdinaryZipLargerThanPreambleLimitByDefault() {
        val stageDir = Files.createTempDirectory("dxx-zip-test-").toFile()
        val archive = stageDir.resolve("game-data.zip")
        val payloadBytes = ExtractionLimits.MAX_ZIP_PREAMBLE_BYTES + 1
        val buffer = ByteArray(64 * 1024) { (it % 251).toByte() }
        val crc = CRC32()
        var remaining = payloadBytes
        while (remaining > 0) {
            val count = minOf(buffer.size.toLong(), remaining).toInt()
            crc.update(buffer, 0, count)
            remaining -= count
        }
        try {
            ZipOutputStream(archive.outputStream()).use { zip ->
                zip.putNextEntry(
                    ZipEntry("descent.hog").apply {
                        method = ZipEntry.STORED
                        size = payloadBytes
                        compressedSize = payloadBytes
                        this.crc = crc.value
                    },
                )
                remaining = payloadBytes
                while (remaining > 0) {
                    val count = minOf(buffer.size.toLong(), remaining).toInt()
                    zip.write(buffer, 0, count)
                    remaining -= count
                }
                zip.closeEntry()
                zip.writeEntry("DESCENT1.SOW", "payload")
            }
            assertTrue(archive.length() > ExtractionLimits.MAX_ZIP_PREAMBLE_BYTES)
            archive.inputStream().use { input ->
                openZipInputStreamSkippingPreamble(input, stageDir).use { zip ->
                    val entry = zip.nextEntry
                    assertEquals("descent.hog", entry.name)
                    val budget = ExtractionBudget()
                    budget.registerEntry(entry.size, entry.compressedSize, entry.name)
                    val extractedCrc = CRC32()
                    var extractedBytes = 0L
                    while (true) {
                        val count = zip.read(buffer)
                        if (count < 0) break
                        extractedBytes += count
                        budget.accountActual(count, extractedBytes, entry.compressedSize, entry.name)
                        extractedCrc.update(buffer, 0, count)
                    }
                    assertEquals(payloadBytes, extractedBytes)
                    assertEquals(crc.value, extractedCrc.value)
                    assertEquals("DESCENT1.SOW", zip.nextEntry.name)
                    assertEquals("payload", zip.readBytes().toString(Charsets.US_ASCII))
                    assertEquals(null, zip.nextEntry)
                }
            }
            assertEquals(listOf(archive), stageDir.listFiles().orEmpty().toList())
        } finally {
            stageDir.deleteRecursively()
        }
    }

    @Test
    fun forgedLocalHeaderDoesNotLiftStreamingLimit() {
        val source = PrefixThenZerosInputStream(makeLocalHeader(method = 0, name = "forged"))
        val stageDir = Files.createTempDirectory("dxx-zip-test-").toFile()
        try {
            assertThrows(ZipException::class.java) {
                openZipInputStreamSkippingPreamble(source, stageDir, maxSourceBytes = 64 * 1024L)
            }
            assertEquals(64 * 1024L + 1, source.bytesRead)
            assertTrue(source.closed)
            assertTrue(stageDir.listFiles().orEmpty().isEmpty())
        } finally {
            stageDir.deleteRecursively()
        }
    }

    @Test
    fun acceptsExactStreamingLimitAndCleansStageOnClose() {
        val archive = makeZip()
        val source = sourceAtSize(64 * 1024L, archive)
        val stageDir = Files.createTempDirectory("dxx-zip-test-").toFile()
        try {
            openZipInputStreamSkippingPreamble(source, stageDir, maxSourceBytes = 64 * 1024L).use { zip ->
                assertEquals("DESCENT1.SOW", zip.nextEntry.name)
                assertEquals("payload", zip.readBytes().toString(Charsets.US_ASCII))
                assertEquals(1, stageDir.listFiles().orEmpty().size)
            }
            assertTrue(stageDir.listFiles().orEmpty().isEmpty())
        } finally {
            stageDir.deleteRecursively()
        }
    }

    @Test
    fun rejectsOneByteOverStreamingLimitAndCleansStage() {
        val archive = makeZip()
        val exact = sourceAtSize(64 * 1024L, archive)
        val source = SequenceInputStream(exact, ByteArrayInputStream(byteArrayOf(0)))
        val stageDir = Files.createTempDirectory("dxx-zip-test-").toFile()
        try {
            assertThrows(ZipException::class.java) {
                openZipInputStreamSkippingPreamble(source, stageDir, maxSourceBytes = 64 * 1024L)
            }
            assertTrue(stageDir.listFiles().orEmpty().isEmpty())
        } finally {
            stageDir.deleteRecursively()
        }
    }

    @Test
    fun rejectsMalformedZipStructureAndCleansStage() {
        val stageDir = Files.createTempDirectory("dxx-zip-test-").toFile()
        try {
            assertThrows(ZipException::class.java) {
                openZipInputStreamSkippingPreamble(
                    ByteArrayInputStream(makeLocalHeader(method = 0, name = "forged")),
                    stageDir,
                )
            }
            assertTrue(stageDir.listFiles().orEmpty().isEmpty())
        } finally {
            stageDir.deleteRecursively()
        }
    }

    private fun ZipOutputStream.writeEntry(
        name: String,
        contents: String,
    ) {
        putNextEntry(java.util.zip.ZipEntry(name))
        write(contents.toByteArray(Charsets.US_ASCII))
        closeEntry()
    }

    private fun makeZip(): ByteArray {
        val bytes = ByteArrayOutputStream()
        ZipOutputStream(bytes).use { it.writeEntry("DESCENT1.SOW", "payload") }
        return bytes.toByteArray()
    }

    private fun makeLocalHeader(
        method: Int,
        name: String,
    ): ByteArray {
        val nameBytes = name.toByteArray(Charsets.US_ASCII)
        val header = ByteArray(30 + nameBytes.size)
        header[0] = 0x50
        header[1] = 0x4b
        header[2] = 0x03
        header[3] = 0x04
        header[4] = 20
        header[8] = method.toByte()
        header[26] = nameBytes.size.toByte()
        nameBytes.copyInto(header, 30)
        return header
    }

    private fun sourceAtSize(
        size: Long,
        suffix: ByteArray,
    ): InputStream {
        val prefixBytes = size - suffix.size
        check(prefixBytes >= 0)
        return SequenceInputStream(RepeatedZeroInputStream(prefixBytes), ByteArrayInputStream(suffix))
    }

    private class RepeatedZeroInputStream(
        private var remaining: Long,
    ) : InputStream() {
        override fun read(): Int {
            if (remaining == 0L) return -1
            remaining--
            return 0
        }

        override fun read(
            bytes: ByteArray,
            offset: Int,
            length: Int,
        ): Int {
            if (remaining == 0L) return -1
            val count = minOf(length.toLong(), remaining).toInt()
            bytes.fill(0, offset, offset + count)
            remaining -= count
            return count
        }
    }

    private class PrefixThenZerosInputStream(
        private val prefix: ByteArray,
    ) : InputStream() {
        var bytesRead = 0L
            private set
        var closed = false
            private set

        override fun read(): Int {
            val value = if (bytesRead < prefix.size) prefix[bytesRead.toInt()].toInt() and 0xff else 0
            bytesRead++
            return value
        }

        override fun read(
            bytes: ByteArray,
            offset: Int,
            length: Int,
        ): Int {
            repeat(length) { index ->
                bytes[offset + index] = if (bytesRead < prefix.size) prefix[bytesRead.toInt()] else 0
                bytesRead++
            }
            return length
        }

        override fun close() {
            closed = true
        }
    }
}
