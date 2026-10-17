package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class DxaTextureScannerTest {
    @Test
    fun structureValidationBoundsDepthEntriesAndTextureCount() {
        val depth64 = (1..64).joinToString("/") { "d$it" } + "/texture.png"
        val depth65 = (1..65).joinToString("/") { "d$it" } + "/texture.png"

        assertNull(DxaTextureScanner.validateStructure(sequenceOf(depth64)))
        assertNotNull(DxaTextureScanner.validateStructure(sequenceOf(depth65)))
        assertNotNull(DxaTextureScanner.validateStructure(sequenceOf("safe/../texture.png")))
        assertNotNull(
            DxaTextureScanner.validateStructure(generateSequence(0) { it + 1 }.take(4_097).map { "f$it.txt" }),
        )
        assertNotNull(
            DxaTextureScanner.validateStructure(generateSequence(0) { it + 1 }.take(4_097).map { "f$it.png" }),
        )
        assertNotNull(
            DxaTextureScanner.validateStructure(
                (0 until 300).asSequence().map { index -> "f${index}_${"a".repeat(3_600)}.txt" },
            ),
        )
    }

    @Test
    fun admissionBoundsSourceEntryTotalAndExpansionRatio() {
        fun entry(
            name: String,
            expandedSize: Long,
            compressedSize: Long,
        ) = DxaTextureScanner.EntryMetadata(name, false, expandedSize, compressedSize)

        assertNull(
            DxaTextureScanner.validateAdmission(
                sourceBytes = 1,
                entries = sequenceOf(entry("exact-ratio.png", 1_000, 1)),
            ),
        )
        assertNotNull(
            DxaTextureScanner.validateAdmission(
                sourceBytes = ExtractionLimits.MAX_DXA_SOURCE_BYTES + 1,
                entries = emptySequence(),
            ),
        )
        assertNotNull(
            DxaTextureScanner.validateAdmission(
                sourceBytes = ExtractionLimits.MAX_DXA_SOURCE_BYTES,
                entries =
                    sequenceOf(
                        entry(
                            "too-large.png",
                            ExtractionLimits.MAX_ENTRY_BYTES + 1,
                            ExtractionLimits.MAX_ENTRY_BYTES + 1,
                        ),
                    ),
            ),
        )
        assertNotNull(
            DxaTextureScanner.validateAdmission(
                sourceBytes = ExtractionLimits.MAX_DXA_SOURCE_BYTES,
                entries =
                    (0 until 5).asSequence().map { index ->
                        entry("large-$index.png", ExtractionLimits.MAX_ENTRY_BYTES, ExtractionLimits.MAX_ENTRY_BYTES)
                    },
            ),
        )
        assertNotNull(
            DxaTextureScanner.validateAdmission(
                sourceBytes = 1,
                entries = sequenceOf(entry("bomb.png", 1_001, 1)),
            ),
        )
        assertNotNull(
            DxaTextureScanner.validateAdmission(
                sourceBytes = 1,
                entries = sequenceOf(entry("unknown.png", -1, -1)),
            ),
        )
    }

    @Test
    fun scanRejectsHighExpansionRatioBeforeTextureInspection() {
        val zipFile = File.createTempFile("dxa-scan-ratio", ".dxa")
        zipFile.deleteOnExit()

        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("bomb.png"))
            zip.write(ByteArray(2 * 1024 * 1024))
            zip.closeEntry()
        }

        val result = DxaTextureScanner.scan(zipFile)

        assertNotNull(result)
        assertFalse(result!!.canEnable)
        assertTrue(result.rejectedReason!!.contains("expansion ratio"))
    }

    @Test
    fun scanRejectsUnsafeHierarchy() {
        val zipFile = File.createTempFile("dxa-scan-depth", ".dxa")
        zipFile.deleteOnExit()
        val deepName = (1..65).joinToString("/") { "d$it" } + "/texture.png"

        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry(deepName))
            zip.write(makePngHeader(width = 64, height = 64))
            zip.closeEntry()
        }

        val result = DxaTextureScanner.scan(zipFile)

        assertNotNull(result)
        assertNotNull(result!!.rejectedReason)
        assertFalse(result.canEnable)
    }

    @Test
    fun scanReportsOversizedPngEntries() {
        val zipFile = File.createTempFile("dxa-scan", ".dxa")
        zipFile.deleteOnExit()

        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("small.png"))
            zip.write(makePngHeader(width = 640, height = 480))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("statusbx2.png"))
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
        assertTrue(result.canEnable)
        assertEquals(1, result.oversizedCount)
        assertEquals(1, result.oversizedEntries.size)
        assertEquals("statusbx2.png", result.oversizedEntries[0].name)
        assertEquals(4096, result.oversizedEntries[0].pow2Width)
        assertEquals(4096, result.oversizedEntries[0].pow2Height)
    }

    @Test
    fun scanCountsKtx2TexturesAndIgnoresMaskSidecars() {
        val zipFile = File.createTempFile("dxa-scan-ktx2", ".dxa")
        zipFile.deleteOnExit()

        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("rock001.ktx2"))
            zip.write(makeKtx2Header(width = 512, height = 512))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("statusb.ktx2"))
            zip.write(makeKtx2Header(width = 2048, height = 256))
            zip.closeEntry()

            zip.putNextEntry(ZipEntry("statusb_mask.png"))
            zip.write(makePngHeader(width = 2048, height = 256))
            zip.closeEntry()
        }

        val result = DxaTextureScanner.scan(zipFile)

        assertNotNull(result)
        result!!
        assertEquals(2048, result.maxWidth)
        assertEquals(512, result.maxHeight)
        assertEquals(2, result.textureCount)
        assertEquals(0, result.oversizedCount)
    }

    @Test
    fun scanReportsOversizedJpegEntries() {
        val zipFile = File.createTempFile("dxa-scan-jpeg", ".dxa")
        zipFile.deleteOnExit()

        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("large.jpg"))
            zip.write(makeJpegHeader(width = 4096, height = 1024))
            zip.closeEntry()
        }

        val result = DxaTextureScanner.scan(zipFile)

        assertNotNull(result)
        result!!
        assertEquals(1, result.textureCount)
        assertTrue(result.canEnable)
        assertEquals(1, result.oversizedCount)
        assertEquals("large.jpg", result.oversizedEntries.single().name)
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

    private fun makeKtx2Header(
        width: Int,
        height: Int,
    ): ByteArray {
        val bytes = ByteArray(28)
        val identifier =
            byteArrayOf(
                0xAB.toByte(),
                0x4B,
                0x54,
                0x58,
                0x20,
                0x32,
                0x30,
                0xBB.toByte(),
                0x0D,
                0x0A,
                0x1A,
                0x0A,
            )
        System.arraycopy(identifier, 0, bytes, 0, identifier.size)
        val sizeBytes = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN)
        sizeBytes.putInt(width)
        sizeBytes.putInt(height)
        System.arraycopy(sizeBytes.array(), 0, bytes, 20, 8)
        return bytes
    }

    private fun makeJpegHeader(
        width: Int,
        height: Int,
    ): ByteArray =
        byteArrayOf(
            0xFF.toByte(),
            0xD8.toByte(),
            0xFF.toByte(),
            0xC0.toByte(),
            0x00,
            0x0B,
            0x08,
            (height ushr 8).toByte(),
            height.toByte(),
            (width ushr 8).toByte(),
            width.toByte(),
            0x01,
            0x01,
        )
}
