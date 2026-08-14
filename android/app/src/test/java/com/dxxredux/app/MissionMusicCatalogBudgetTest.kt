package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.File
import java.nio.file.Files
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class MissionMusicCatalogBudgetTest {
    @Test
    fun streamingDescriptorPreservesExactTrackSize() {
        val payload = ByteArray(513) { it.toByte() }
        val archive =
            createZip("catalog-streaming-size") {
                writeEntry("music.dxa", createZipBytes { writeEntry("track.mid", payload) })
            }

        val track = MissionZipMusic.inspect(archive)!!.sources.single().tracks.single()

        assertEquals(payload.size.toLong(), track.sizeBytes)
    }

    @Test
    fun nestedStreamingOutputUsesOneExpansionBudget() {
        val payload = ByteArray(48) { it.toByte() }
        val archive =
            createZip("catalog-streaming-output") {
                writeEntry("music.dxa", createZipBytes { writeEntry("track.mid", payload) })
            }

        assertNotNull(
            MissionZipMusic.inspect(
                archive,
                MissionMusicCatalogBudget(maxExpandedBytes = payload.size.toLong()),
            ),
        )
        assertNull(
            MissionZipMusic.inspect(
                archive,
                MissionMusicCatalogBudget(maxExpandedBytes = payload.size - 1L),
            ),
        )
    }

    @Test
    fun nestedStreamingRatioAcceptsExactAndRejectsOneOver() {
        val payload = ByteArray(4096)
        val archive =
            createZip("catalog-streaming-ratio") {
                writeEntry("music.dxa", createZipBytes { writeEntry("track.mid", payload) })
            }

        assertNotNull(MissionZipMusic.inspect(archive))
        assertNull(MissionZipMusic.inspect(archive, MissionMusicCatalogBudget(maxExpansionRatio = 1)))
    }

    @Test
    fun outerWorkLimitAcceptsExactAndRejectsOneOver() {
        val archive =
            createZip("catalog-outer-work") {
                writeEntry("first.txt", byteArrayOf(1))
                writeEntry("second.txt", byteArrayOf(2))
                writeEntry("music.ogg", byteArrayOf(3))
            }

        assertNotNull(MissionZipMusic.inspect(archive, MissionMusicCatalogBudget(maxWorkEntries = 3)))
        assertNull(MissionZipMusic.inspect(archive, MissionMusicCatalogBudget(maxWorkEntries = 2)))
    }

    @Test
    fun nestedContainersShareOneOuterAndInnerWorkLimit() {
        val archive =
            createZip("catalog-nested-work") {
                writeEntry("first.dxa", createZipBytes { writeEntry("first.ogg", byteArrayOf(1)) })
                writeEntry("second.dxa", createZipBytes { writeEntry("second.ogg", byteArrayOf(2)) })
            }

        val exact = MissionZipMusic.inspect(archive, MissionMusicCatalogBudget(maxWorkEntries = 4))

        assertEquals(2, exact!!.sources.size)
        assertNull(MissionZipMusic.inspect(archive, MissionMusicCatalogBudget(maxWorkEntries = 3)))
    }

    @Test
    fun nestedDxaAndHogEntriesShareTheSameWorkLimit() {
        val archive =
            createZip("catalog-dxa-hog-work") {
                writeEntry(
                    "music.dxa",
                    createZipBytes {
                        writeEntry("music.hog", createHogBytes("first.ogg" to byteArrayOf(1), "second.ogg" to byteArrayOf(2)))
                    },
                )
            }

        assertNotNull(MissionZipMusic.inspect(archive, MissionMusicCatalogBudget(maxWorkEntries = 4)))
        assertNull(MissionZipMusic.inspect(archive, MissionMusicCatalogBudget(maxWorkEntries = 3)))
    }

    @Test
    fun retainedTrackLimitAcceptsExactAndRejectsOneOver() {
        val archive =
            createZip("catalog-retained-count") {
                writeEntry("first.ogg", byteArrayOf(1))
                writeEntry("second.ogg", byteArrayOf(2))
            }

        assertNotNull(MissionZipMusic.inspect(archive, MissionMusicCatalogBudget(maxRetainedEntries = 4)))
        assertNull(MissionZipMusic.inspect(archive, MissionMusicCatalogBudget(maxRetainedEntries = 3)))
    }

    @Test
    fun retainedMemoryLimitAcceptsExactAndRejectsOneOver() {
        MissionMusicCatalogBudget(maxRetainedBytes = 264).retain("record", "four")

        assertThrows(MissionMusicCatalogRejected::class.java) {
            MissionMusicCatalogBudget(maxRetainedBytes = 263).retain("record", "four")
        }
    }

    @Test
    fun cancellationStopsTheAttemptWithoutChangingTheArchive() {
        val directory = Files.createTempDirectory("catalog-cancel-").toFile().apply { deleteOnExit() }
        val archive =
            createZip("catalog-cancel", directory) {
                writeEntry("first.ogg", byteArrayOf(1))
                writeEntry("second.ogg", byteArrayOf(2))
            }
        val originalLength = archive.length()
        val originalFiles = directory.list()!!.toSet()
        var checks = 0

        val catalog =
            MissionZipMusic.inspect(
                archive,
                MissionMusicCatalogBudget(isCancelled = { checks++ >= 1 }),
            )

        assertNull(catalog)
        assertEquals(originalLength, archive.length())
        assertEquals(originalFiles, directory.list()!!.toSet())
    }

    @Test
    fun sequentialAttemptsReceiveIndependentCatalogBudgets() {
        val archive =
            createZip("catalog-sequential") {
                writeEntry("first.ogg", byteArrayOf(1))
                writeEntry("second.ogg", byteArrayOf(2))
            }

        assertNull(MissionZipMusic.inspect(archive, MissionMusicCatalogBudget(maxWorkEntries = 1)))
        assertNotNull(MissionZipMusic.inspect(archive))
        assertNotNull(MissionZipMusic.inspect(archive))
    }

    @Test
    fun extractedContainerUsesTheSameOuterAndInnerWorkLimit() {
        val root = Files.createTempDirectory("catalog-extracted-").toFile().apply { deleteOnExit() }
        val dxa =
            File(root, "music.dxa").apply {
                writeBytes(createZipBytes { writeEntry("first.ogg", byteArrayOf(1)) })
            }
        val record =
            MissionZipExtractionRecord(
                ownerFilename = "mission.7z",
                ownerSizeBytes = dxa.length(),
                ownerLastModifiedMs = dxa.lastModified(),
                rootDir = root,
                files = listOf(MissionZipExtractedFile("music.dxa", "music.dxa", dxa.length())),
                archiveFormat = "7z",
            )

        assertNotNull(MissionZipMusic.inspectExtracted(record, MissionMusicCatalogBudget(maxWorkEntries = 2)))
        assertNull(MissionZipMusic.inspectExtracted(record, MissionMusicCatalogBudget(maxWorkEntries = 1)))
    }

    private fun createZip(
        prefix: String,
        directory: File? = null,
        writeEntries: ZipOutputStream.() -> Unit,
    ): File {
        val archive = File.createTempFile(prefix, ".zip", directory).apply { deleteOnExit() }
        ZipOutputStream(archive.outputStream()).use { it.writeEntries() }
        return archive
    }

    private fun createZipBytes(writeEntries: ZipOutputStream.() -> Unit): ByteArray =
        ByteArrayOutputStream().use { output ->
            ZipOutputStream(output).use { it.writeEntries() }
            output.toByteArray()
        }

    private fun ZipOutputStream.writeEntry(
        name: String,
        bytes: ByteArray,
    ) {
        putNextEntry(ZipEntry(name))
        write(bytes)
        closeEntry()
    }

    private fun createHogBytes(vararg entries: Pair<String, ByteArray>): ByteArray =
        ByteArrayOutputStream().use { output ->
            output.write("DHF".toByteArray(Charsets.US_ASCII))
            entries.forEach { (name, data) ->
                output.write(name.toByteArray(Charsets.US_ASCII).copyOf(13))
                output.write(leInt(data.size))
                output.write(data)
            }
            output.toByteArray()
        }

    private fun leInt(value: Int): ByteArray =
        byteArrayOf(
            value.toByte(),
            (value shr 8).toByte(),
            (value shr 16).toByte(),
            (value shr 24).toByte(),
        )
}
