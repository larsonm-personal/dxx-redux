package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class DxaTextureScannerTest {
    @Test
    fun scanReportsOversizedPngEntries() {
        val zipFile = File.createTempFile("dxa-scan", ".dxa")
        zipFile.deleteOnExit()

        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("small.png"))
            zip.write(makePngHeader(width = 640, height = 480))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("statusbx2_mask.png"))
            zip.write(makePngHeader(width = 2560, height = 3072))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("ignored.ktx2"))
            zip.write(byteArrayOf(0x00, 0x01, 0x02))
            zip.closeEntry()
        }

        val result = DxaTextureScanner.scan(zipFile)

        assertNotNull(result)
        result!!
        assertEquals(2560, result.maxWidth)
        assertEquals(3072, result.maxHeight)
        assertEquals(2, result.textureCount)
        assertEquals(1, result.oversizedCount)
        assertEquals(1, result.oversizedEntries.size)
        assertEquals("statusbx2_mask.png", result.oversizedEntries[0].name)
        assertEquals(4096, result.oversizedEntries[0].pow2Width)
        assertEquals(4096, result.oversizedEntries[0].pow2Height)
    }

    private fun makePngHeader(
        width: Int,
        height: Int,
    ): ByteArray {
        val bytes = ByteArray(24)
        bytes[0] = 0x89.toByte()
        bytes[1] = 0x50.toByte()
        bytes[2] = 0x4E.toByte()
        bytes[3] = 0x47.toByte()
        bytes[4] = 0x0D.toByte()
        bytes[5] = 0x0A.toByte()
        bytes[6] = 0x1A.toByte()
        bytes[7] = 0x0A.toByte()
        bytes[12] = 0x49.toByte()
        bytes[13] = 0x48.toByte()
        bytes[14] = 0x44.toByte()
        bytes[15] = 0x52.toByte()
        val sizeBytes = ByteBuffer.allocate(8).order(ByteOrder.BIG_ENDIAN)
        sizeBytes.putInt(width)
        sizeBytes.putInt(height)
        System.arraycopy(sizeBytes.array(), 0, bytes, 16, 8)
        return bytes
    }
}