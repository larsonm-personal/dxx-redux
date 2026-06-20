package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class MissionZipMusicExtractedPreviewTest {
    @Test
    fun extractedDirectoryCatalogStagesDxaTrackReferencedByPath() {
        val filesDir = File("build/test-mission-zip-music-extracted-preview").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()
        val root = File(filesDir, "mods/.extracted_mission_zips/PathMusic.7z")
        val missionsDir = File(root, "missions").apply { mkdirs() }
        val trackBytes = byteArrayOf(1, 2, 3, 4, 5)
        val dxa = File(missionsDir, "pathmusic.dxa")
        dxa.writeBytes(
            createZipBytes {
                it.putNextEntry(ZipEntry("descent.sng"))
                it.write("music/level01.ogg\n".toByteArray())
                it.closeEntry()

                it.putNextEntry(ZipEntry("music/level01.ogg"))
                it.write(trackBytes)
                it.closeEntry()
            },
        )
        val record =
            MissionZipExtractionRecord(
                ownerFilename = "PathMusic.7z",
                ownerSizeBytes = 123L,
                ownerLastModifiedMs = 0L,
                rootDir = root,
                files =
                    listOf(
                        MissionZipExtractedFile(
                            entryPath = "pathmusic.dxa",
                            relativePath = "missions/pathmusic.dxa",
                            sizeBytes = dxa.length(),
                        ),
                    ),
                archiveFormat = "7z",
            )

        val catalog = MissionZipMusic.inspectExtracted(record)
        assertNotNull(catalog)
        val track = catalog!!.sources.flatMap { it.tracks }.single()
        assertTrue(track.playable)

        val staged = MissionZipMusicStageManager(File(filesDir, "cache")).stageCompressedAudioTrack(catalog, track)
        assertNotNull(staged)
        assertArrayEquals(trackBytes, staged!!.readBytes())
    }

    private fun createZipBytes(writeEntries: (ZipOutputStream) -> Unit): ByteArray =
        ByteArrayOutputStream().use { output ->
            ZipOutputStream(output).use(writeEntries)
            output.toByteArray()
        }
}
