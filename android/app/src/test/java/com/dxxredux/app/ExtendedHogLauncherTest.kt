package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.nio.file.Files
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class ExtendedHogLauncherTest {
    private val song = "Mission soundtrack with long name.mid"
    private val audio = "Mission soundtrack with long name.ogg"
    private val midiBytes = byteArrayOf(77, 84, 104, 100, 0, 0, 0, 6)
    private val audioBytes = byteArrayOf(79, 103, 103, 83, 1, 2, 3)

    @Test
    fun importsCatalogsPreviewsStagesAndCleansUpBothHeaderVariants() {
        for (signature in listOf("DHF", "D2X")) {
            val root = Files.createTempDirectory("extended-hog-launcher-").toFile()
            try {
                val hogBytes = NativeHogArchiveTest.hog(signature, members())
                val archive = packageFile(root, hogBytes)
                val before = archive.readBytes()
                val scan = requireNotNull(MissionZip.inspect(archive))
                assertEquals("Extended", scan.mission.displayName)
                assertEquals(
                    scan.mission.levelNames,
                    requireNotNull(
                        archive.inputStream().use {
                            MissionZip.inspect(it)
                        },
                    ).mission.levelNames,
                )
                assertTrue(archive.inputStream().use { MissionZip.isImportCandidate(it) })
                val manager = ModManager(File(root, "files"))
                val imported = requireNotNull(manager.importMissionZipFile(archive, "Extended.zip"))
                assertEquals("Extended.zip", imported.filename)
                assertEquals(1, manager.listMods().size)
                assertTrue(manager.hasEnabledMissionZipSoundtrack("d2"))
                val hogFile = File(root, "original.hog").apply { writeBytes(hogBytes) }
                val summary = requireNotNull(GameFileMetadata.summarizeLocalFile(hogFile))
                assertTrue(summary.problems.joinToString(), summary.problems.isEmpty())
                assertEquals(members().map { it.first }, summary.contents.map { it.name })
                assertEquals(hogBytes.size.toLong(), hogFile.length())

                val catalog = requireNotNull(MissionZipMusic.inspect(archive))
                val tracks = catalog.sources.flatMap { it.tracks }
                val midi = tracks.single { it.hogEntryName == song }
                val compressed = tracks.single { it.hogEntryName == audio }
                val stage = MissionZipMusicStageManager(File(root, "cache"))
                assertArrayEquals(midiBytes, stage.readMidiTrackBytes(catalog, midi))
                val output = requireNotNull(stage.stageCompressedAudioTrack(catalog, compressed))
                assertArrayEquals(audioBytes, output.readBytes())
                assertEquals(output, stage.stageCompressedAudioTrack(catalog, compressed))
                stage.cleanupOldFiles(maxAgeMs = 0, maxBytes = 0)
                assertFalse(output.exists())
                assertArrayEquals(before, archive.readBytes())
                assertArrayEquals(hogBytes, hogFile.readBytes())
            } finally {
                root.deleteRecursively()
            }
        }
    }

    @Test
    fun rejectsMalformedTailBeforePublishingImportedOrStagedData() {
        val root = Files.createTempDirectory("extended-hog-malformed-").toFile()
        try {
            val valid = NativeHogArchiveTest.hog("D2X", members())
            val archive = packageFile(root, valid)
            val catalog = requireNotNull(MissionZipMusic.inspect(archive))
            val tracks = catalog.sources.flatMap { it.tracks }
            val stage = MissionZipMusicStageManager(File(root, "cache"))
            val manager = ModManager(File(root, "files"))
            requireNotNull(manager.importMissionZipFile(archive, "Prior.zip"))
            val prior = manager.listMods()
            for (bad in listOf(
                valid + byteArrayOf(1),
                NativeHogArchiveTest.hog(
                    "D2X",
                    members() + Triple(song.uppercase(), true, byteArrayOf(9)),
                ),
            )) {
                packageFile(root, bad)
                assertNull(MissionZip.inspect(archive))
                assertNull(manager.importMissionZipFile(archive, "Broken.zip"))
                assertEquals(prior, manager.listMods())
                assertNull(stage.readMidiTrackBytes(catalog, tracks.single { it.hogEntryName == song }))
                assertNull(stage.stageCompressedAudioTrack(catalog, tracks.single { it.hogEntryName == audio }))
                assertFalse(File(root, "cache").walkTopDown().any { it.isFile })
            }
        } finally {
            root.deleteRecursively()
        }
    }

    private fun members() =
        listOf(
            Triple("level01.rl2", false, byteArrayOf(1)),
            Triple("readme with long name.txt", true, "metadata".toByteArray()),
            Triple(song, true, midiBytes),
            Triple(audio, true, audioBytes),
            Triple("last.bin", false, byteArrayOf(3)),
        )

    private fun packageFile(
        root: File,
        hog: ByteArray,
    ): File =
        File(root, "extended.zip").apply {
            ZipOutputStream(outputStream()).use { zip ->
                zip.putNextEntry(ZipEntry("extended.mn2"))
                zip.write("name = Extended\nnum_levels = 1\nlevel01.rl2\n".toByteArray())
                zip.closeEntry()
                zip.putNextEntry(ZipEntry("extended.hog"))
                zip.write(hog)
                zip.closeEntry()
            }
        }
}
