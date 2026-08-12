package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class MissionZipMusicExtractedPreviewTest {
    @Test
    fun extractedCatalogKeepsOriginalSongListIdentityInsteadOfGeneratedAlias() {
        val root = File("build/test-mission-zip-music-extracted-preview/song-list-identity").absoluteFile
        root.deleteRecursively()
        root.mkdirs()
        val original = File(root, "obsidian.sng").apply { writeText("game01.hmp\n") }
        val generated = File(root, "descent.sng").apply { writeText("game01.hmp\n") }
        val record =
            MissionZipExtractionRecord(
                ownerFilename = "Obsidian.zip",
                ownerSizeBytes = original.length(),
                ownerLastModifiedMs = 0L,
                rootDir = root,
                files =
                    listOf(
                        MissionZipExtractedFile("obsidian.sng", "obsidian.sng", original.length()),
                        MissionZipExtractedFile("", "descent.sng", generated.length()),
                    ),
                archiveFormat = "zip",
            )

        val catalog = requireNotNull(MissionZipMusic.inspectExtracted(record))

        assertEquals("obsidian.sng", catalog.songLists.single().archiveEntryPath)
        assertEquals("obsidian.sng", catalog.songLists.single().displayName)
    }

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

    @Test
    fun sameNamedExtractedDirectoriesWithSameMetadataHaveDistinctContentIdentity() {
        val filesDir = File("build/test-mission-zip-music-extracted-preview/same-name").absoluteFile
        filesDir.deleteRecursively()
        val first = extractedRecord(File(filesDir, "first/Mission.7z"), byteArrayOf(1, 2, 3, 4))
        val second = extractedRecord(File(filesDir, "second/Mission.7z"), byteArrayOf(4, 3, 2, 1))
        val fixedMtime = 1_781_012_345_000L
        first.files.forEach { File(first.rootDir, it.relativePath).setLastModified(fixedMtime) }
        second.files.forEach { File(second.rootDir, it.relativePath).setLastModified(fixedMtime) }

        val firstCatalog = MissionZipMusic.inspectExtracted(first)!!
        val secondCatalog = MissionZipMusic.inspectExtracted(second)!!

        assertNotEquals(firstCatalog.sourceIdentity, secondCatalog.sourceIdentity)
        assertArrayEquals(
            byteArrayOf(1, 2, 3, 4),
            MissionZipMusicStageManager(File(filesDir, "cache"))
                .stageCompressedAudioTrack(firstCatalog, firstCatalog.sources.single().tracks.single())!!
                .readBytes(),
        )
        assertArrayEquals(
            byteArrayOf(4, 3, 2, 1),
            MissionZipMusicStageManager(File(filesDir, "cache"))
                .stageCompressedAudioTrack(secondCatalog, secondCatalog.sources.single().tracks.single())!!
                .readBytes(),
        )
    }

    @Test
    fun malformedOptionalDxaDoesNotHideIndependentExtractedMusic() {
        val root = File("build/test-mission-zip-music-extracted-preview/malformed/Mission.7z").absoluteFile
        root.deleteRecursively()
        val missionsDir = File(root, "missions").apply { mkdirs() }
        val malformed = File(missionsDir, "broken.dxa").apply { writeText("not a zip") }
        val playable = File(missionsDir, "level01.ogg").apply { writeBytes(byteArrayOf(1, 2, 3)) }
        val record =
            MissionZipExtractionRecord(
                ownerFilename = root.name,
                ownerSizeBytes = malformed.length() + playable.length(),
                ownerLastModifiedMs = 0L,
                rootDir = root,
                files =
                    listOf(
                        MissionZipExtractedFile("broken.dxa", "missions/broken.dxa", malformed.length()),
                        MissionZipExtractedFile("level01.ogg", "missions/level01.ogg", playable.length()),
                    ),
                archiveFormat = "7z",
            )

        val catalog = MissionZipMusic.inspectExtracted(record)

        assertNotNull(catalog)
        assertTrue(catalog!!.sources.flatMap { it.tracks }.single().playable)
    }

    private fun extractedRecord(
        root: File,
        trackBytes: ByteArray,
    ): MissionZipExtractionRecord {
        val missionsDir = File(root, "missions").apply { mkdirs() }
        val dxa = File(missionsDir, "music.dxa")
        dxa.writeBytes(
            createZipBytes {
                it.putNextEntry(ZipEntry("song01.ogg"))
                it.write(trackBytes)
                it.closeEntry()
            },
        )
        return MissionZipExtractionRecord(
            ownerFilename = root.name,
            ownerSizeBytes = dxa.length(),
            ownerLastModifiedMs = dxa.lastModified(),
            rootDir = root,
            files =
                listOf(
                    MissionZipExtractedFile(
                        entryPath = "music.dxa",
                        relativePath = "missions/music.dxa",
                        sizeBytes = dxa.length(),
                    ),
                ),
            archiveFormat = "7z",
        )
    }

    private fun createZipBytes(writeEntries: (ZipOutputStream) -> Unit): ByteArray =
        ByteArrayOutputStream().use { output ->
            ZipOutputStream(output).use(writeEntries)
            output.toByteArray()
        }
}
