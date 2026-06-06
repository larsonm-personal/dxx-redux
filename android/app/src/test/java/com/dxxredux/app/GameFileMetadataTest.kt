package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class GameFileMetadataTest {
    @Test
    fun summarizesUneasyStyleHog() {
        val hog = File.createTempFile("Uneasy4", ".hog")
        hog.deleteOnExit()
        hog.writeBytes(
            createHog(
                "descent.txb" to ByteArray(12),
                "descent2.ham" to ByteArray(32),
                "Uneasy4.rl2" to ByteArray(24),
                "Uneasy4.ied" to ByteArray(8),
                "Uneasy4.pog" to ByteArray(16),
            ),
        )

        val summary = GameFileMetadata.summarizeLocalFile(hog)

        assertNotNull(summary)
        summary!!
        assertEquals("HOG", summary.format)
        assertEquals("Mission archive", summary.scope)
        assertEquals("D2", summary.game)
        assertEquals("5", summary.detailRows.first { it.first == "Entries" }.second)
        assertTrue(summary.categories.any { it.label == "D2 level" })
        assertTrue(summary.notes.contains("Includes Inferno editor data"))
    }

    @Test
    fun reportsMalformedHog() {
        val hog = File.createTempFile("broken", ".hog")
        hog.deleteOnExit()
        hog.writeBytes("NOPE".toByteArray(Charsets.US_ASCII))

        val summary = GameFileMetadata.summarizeLocalFile(hog)

        assertNotNull(summary)
        summary!!
        assertEquals("HOG", summary.format)
        assertEquals(listOf("Invalid HOG magic"), summary.problems)
    }

    @Test
    fun summarizesDxaConstituentFromMissionZip() {
        val zipFile = File.createTempFile("Uneasy4", ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("Uneasy4.dxa"))
            zip.write(
                createZip(
                    "descent.sng" to ByteArray(4),
                    "descent2.s22" to ByteArray(12),
                    "Uneasy4.ogg" to ByteArray(16),
                ),
            )
            zip.closeEntry()
        }

        val summary = GameFileMetadata.summarizeZipConstituent(zipFile, "Uneasy4.dxa", "Uneasy4.dxa")

        assertNotNull(summary)
        summary!!
        assertEquals("DXA", summary.format)
        assertEquals("Mod archive", summary.scope)
        assertEquals("D2", summary.game)
        assertTrue(summary.categories.any { it.label == "Audio" })
        assertTrue(summary.categories.any { it.label == "Sound effects" })
        assertTrue(summary.examples.any { it.name == "descent.sng" })
    }

    @Test
    fun summarizesD2PigHeader() {
        val pig = File.createTempFile("groupa", ".pig")
        pig.deleteOnExit()
        pig.writeBytes(
            createD2Pig(
                BitmapHeader("rbot010", 64, 64, flags = 8),
                BitmapHeader("door001", 128, 64, flags = 1),
            ),
        )

        val summary = GameFileMetadata.summarizeLocalFile(pig)

        assertNotNull(summary)
        summary!!
        assertEquals("PIG", summary.format)
        assertEquals("D2", summary.game)
        assertEquals("2", summary.detailRows.first { it.first == "Bitmaps" }.second)
        assertEquals("1", summary.detailRows.first { it.first == "RLE bitmaps" }.second)
        assertTrue(summary.examples.any { it.name == "rbot010" && it.role == "64x64 bitmap" })
    }

    @Test
    fun summarizesD1PigHeaderWithSounds() {
        val pig = File.createTempFile("descent", ".pig")
        pig.deleteOnExit()
        pig.writeBytes(createD1Pig(BitmapHeader("cockpit", 255, 100, flags = 8), soundCount = 1))

        val summary = GameFileMetadata.summarizeLocalFile(pig)

        assertNotNull(summary)
        summary!!
        assertEquals("PIG", summary.format)
        assertEquals("D1", summary.game)
        assertEquals("1", summary.detailRows.first { it.first == "Bitmaps" }.second)
        assertEquals("1", summary.detailRows.first { it.first == "Sounds" }.second)
        assertTrue(summary.notes.any { it.startsWith("Pig data starts at ") })
    }

    private data class BitmapHeader(
        val name: String,
        val width: Int,
        val height: Int,
        val flags: Int,
    )

    private fun createHog(vararg entries: Pair<String, ByteArray>): ByteArray {
        val out = ByteArrayOutputStream()
        out.write("DHF".toByteArray(Charsets.US_ASCII))
        entries.forEach { (name, data) ->
            out.write(fixedName(name, 13))
            out.write(leInt(data.size))
            out.write(data)
        }
        return out.toByteArray()
    }

    private fun createZip(vararg entries: Pair<String, ByteArray>): ByteArray {
        val out = ByteArrayOutputStream()
        ZipOutputStream(out).use { zip ->
            entries.forEach { (name, data) ->
                zip.putNextEntry(ZipEntry(name))
                zip.write(data)
                zip.closeEntry()
            }
        }
        return out.toByteArray()
    }

    private fun createD2Pig(vararg bitmaps: BitmapHeader): ByteArray {
        val out = ByteArrayOutputStream()
        out.write("PPIG".toByteArray(Charsets.US_ASCII))
        out.write(leInt(2))
        out.write(leInt(bitmaps.size))
        bitmaps.forEachIndexed { index, bitmap ->
            out.write(fixedName(bitmap.name, 8))
            out.write(0)
            out.write(bitmap.width and 0xff)
            out.write(bitmap.height and 0xff)
            out.write(((bitmap.width shr 8) and 0x0f) or ((bitmap.height shr 4) and 0xf0))
            out.write(bitmap.flags)
            out.write(0)
            out.write(leInt(index * 16))
        }
        out.write(ByteArray(64))
        return out.toByteArray()
    }

    private fun createD1Pig(
        bitmap: BitmapHeader,
        soundCount: Int,
    ): ByteArray {
        val out = ByteArrayOutputStream()
        val pigDataStart = 16
        out.write(leInt(pigDataStart))
        out.write(ByteArray(pigDataStart - 4))
        out.write(leInt(1))
        out.write(leInt(soundCount))
        out.write(fixedName(bitmap.name, 8))
        out.write(0)
        out.write(bitmap.width and 0xff)
        out.write(bitmap.height and 0xff)
        out.write(bitmap.flags)
        out.write(0)
        out.write(leInt(0))
        repeat(soundCount) {
            out.write(fixedName("laser", 8))
            out.write(leInt(16))
            out.write(leInt(16))
            out.write(leInt(0))
        }
        out.write(ByteArray(64))
        return out.toByteArray()
    }

    private fun fixedName(
        name: String,
        size: Int,
    ): ByteArray {
        val out = ByteArray(size)
        val bytes = name.toByteArray(Charsets.US_ASCII)
        bytes.copyInto(out, endIndex = minOf(bytes.size, size))
        return out
    }

    private fun leInt(value: Int): ByteArray =
        byteArrayOf(
            (value and 0xff).toByte(),
            ((value shr 8) and 0xff).toByte(),
            ((value shr 16) and 0xff).toByte(),
            ((value shr 24) and 0xff).toByte(),
        )
}
