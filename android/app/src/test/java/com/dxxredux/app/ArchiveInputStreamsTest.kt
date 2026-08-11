package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertThrows
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.InputStream
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
        val zeroStream =
            object : InputStream() {
                override fun read(): Int = 0
            }
        assertThrows(ZipException::class.java) {
            openZipInputStreamSkippingPreamble(zeroStream)
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
}
