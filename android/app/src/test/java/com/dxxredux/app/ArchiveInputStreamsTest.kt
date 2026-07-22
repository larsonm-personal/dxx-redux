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
        val zipBytes = ByteArrayOutputStream()
        ZipOutputStream(zipBytes).use { zip ->
            zip.writeEntry("DESCENT1.SOW", "payload")
        }
        val archive = "MZ fake self extractor".toByteArray(Charsets.US_ASCII) + zipBytes.toByteArray()

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
}
