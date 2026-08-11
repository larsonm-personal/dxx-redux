package com.dxxredux.app

import org.apache.commons.compress.archivers.sevenz.SevenZArchiveEntry
import org.apache.commons.compress.archivers.sevenz.SevenZOutputFile
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.File
import java.nio.charset.Charset
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class MissionDescriptorEncodingTest {
    @Test
    fun decodesUtf8AndWindows1252IdenticallyAcrossInspectionBackends() {
        val expectedName = "Caf\u00e9 \u00c9dition"
        val expectedAuthor = "Andr\u00e9"
        val text =
            """
            name = $expectedName
            author = $expectedAuthor
            num_levels = 1
            level01.rl2
            """.trimIndent()
        val encodings = listOf(Charsets.UTF_8, Charset.forName("windows-1252"))

        for (encoding in encodings) {
            val bytes = text.toByteArray(encoding)
            for (mission in inspectThroughEveryBackend(bytes)) {
                assertEquals(expectedName, mission.displayName)
                assertEquals(expectedAuthor, mission.author)
                assertEquals(listOf("level01.rl2"), mission.levelNames)
                assertFalse('\uFFFD' in mission.displayName)
            }
        }
    }

    @Test
    fun directDescriptorDetailsUseTheSameLegacyDecoder() {
        val setDir = File("build/test-mission-descriptor-legacy-details").absoluteFile
        setDir.deleteRecursively()
        setDir.mkdirs()
        File(setDir, "legacy.mn2").writeBytes(
            "name = Caf\u00e9 \u00c9dition\nauthor = Andr\u00e9\nnum_levels = 1\nlevel01.rl2\n"
                .toByteArray(Charset.forName("windows-1252")),
        )

        val mission =
            missionDescriptorForStatus(
                FileStatus(
                    info = GameFileInfo("legacy.mn2", "Descent 2 mission descriptor", required = false),
                    found = true,
                    foundName = "legacy.mn2",
                ),
                setDir,
            )

        assertEquals("Caf\u00e9 \u00c9dition", mission?.displayName)
        assertEquals("Andr\u00e9", mission?.author)
    }

    @Test
    fun truncatedUtf8DropsOnlyAnIncompleteFinalCodePoint() {
        val prefix = "Caf\u00e9 ".toByteArray(Charsets.UTF_8)
        for (character in listOf("\u00e9", "\u20ac", "\ud83d\ude80")) {
            val encoded = character.toByteArray(Charsets.UTF_8)
            for (kept in 1 until encoded.size) {
                assertEquals(
                    "Caf\u00e9 ",
                    MissionZip.decodeLegacyText(prefix + encoded.copyOf(kept), truncated = true),
                )
            }
            assertEquals("Caf\u00e9 $character", MissionZip.decodeLegacyText(prefix + encoded, truncated = true))
        }
    }

    @Test
    fun readmeLimitDoesNotReinterpretAValidUtf8Prefix() {
        val prefix = "Caf\u00e9 ".toByteArray(Charsets.UTF_8)
        val rocket = "\ud83d\ude80".toByteArray(Charsets.UTF_8)
        val archive = writeZip(listOf("README.txt" to (prefix + rocket + "tail".toByteArray())))

        val content = MissionZip.readTextFile(archive, "README.txt", (prefix.size + 2).toLong())

        assertEquals("Caf\u00e9 ", content.text)
        assertEquals(true, content.truncated)
    }

    @Test
    fun malformedUtf8FallsBackWithoutReplacementCharacters() {
        val malformed = "name = Bad ".toByteArray() + byteArrayOf(0xc3.toByte(), 0x28) + "\n".toByteArray()

        val decoded = MissionZip.decodeLegacyText(malformed)

        assertEquals("name = Bad \u00c3(\n", decoded)
        assertFalse('\uFFFD' in decoded)
    }

    private fun inspectThroughEveryBackend(descriptor: ByteArray): List<GameFileFormats.MissionDescriptor> {
        val entries =
            listOf(
                "legacy.hog" to hogBytes("level01.rl2"),
                "legacy.mn2" to descriptor,
            )
        val zip = writeZip(entries)
        val sevenZip = writeSevenZip(entries)
        return listOf(
            requireNotNull(MissionZip.inspect(zip)).mission,
            zip.inputStream().use { requireNotNull(MissionZip.inspect(it)).mission },
            requireNotNull(MissionZip.inspect(sevenZip)).mission,
            requireNotNull(inspectExtracted(entries)).mission,
        )
    }

    private fun writeZip(entries: List<Pair<String, ByteArray>>): File {
        val archive = File.createTempFile("mission-encoding", ".zip").apply { deleteOnExit() }
        ZipOutputStream(archive.outputStream()).use { zip ->
            for ((name, bytes) in entries) {
                zip.putNextEntry(ZipEntry(name))
                zip.write(bytes)
                zip.closeEntry()
            }
        }
        return archive
    }

    private fun writeSevenZip(entries: List<Pair<String, ByteArray>>): File {
        val archive = File.createTempFile("mission-encoding", ".7z").apply { deleteOnExit() }
        SevenZOutputFile(archive).use { sevenZip ->
            for ((name, bytes) in entries) {
                val entry =
                    SevenZArchiveEntry().apply {
                        this.name = name
                        size = bytes.size.toLong()
                    }
                sevenZip.putArchiveEntry(entry)
                sevenZip.write(bytes)
                sevenZip.closeArchiveEntry()
            }
        }
        return archive
    }

    private fun inspectExtracted(entries: List<Pair<String, ByteArray>>): MissionZip.ScanResult? {
        val root = File("build/test-mission-encoding/${System.nanoTime()}").absoluteFile.apply { mkdirs() }
        val files =
            entries.map { (name, bytes) ->
                File(root, name).writeBytes(bytes)
                MissionZipExtractedFile(name, name, bytes.size.toLong())
            }
        return MissionZip.inspectExtracted(
            MissionZipExtractionRecord(
                ownerFilename = "legacy.zip",
                ownerSizeBytes = entries.sumOf { it.second.size.toLong() },
                ownerLastModifiedMs = 0,
                rootDir = root,
                files = files,
            ),
        )
    }

    private fun hogBytes(vararg names: String): ByteArray {
        val output = ByteArrayOutputStream()
        output.write("DHF".toByteArray(Charsets.US_ASCII))
        for (name in names) {
            output.write(name.toByteArray(Charsets.US_ASCII).copyOf(13))
            output.write(byteArrayOf(1, 0, 0, 0))
            output.write(0)
        }
        return output.toByteArray()
    }
}
