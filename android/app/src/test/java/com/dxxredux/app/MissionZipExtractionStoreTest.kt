package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class MissionZipExtractionStoreTest {
    @Test
    fun freshRecordRejectsSameSizeArchiveWithChangedModificationTime() {
        val filesDir = File("build/test-mission-zip-extraction/freshness").absoluteFile
        filesDir.deleteRecursively()
        val modsDir = File(filesDir, "mods").apply { mkdirs() }
        val archive = File(modsDir, "preview.zip")
        ZipOutputStream(archive.outputStream()).use { zip ->
            zip.putNextEntry(ZipEntry("preview.mn2"))
            zip.write(
                """
                name = Preview
                type = normal
                num_levels = 1
                preview.rl2
                """.trimIndent().toByteArray(),
            )
            zip.closeEntry()
            zip.putNextEntry(ZipEntry("preview.hog"))
            zip.write(byteArrayOf('D'.code.toByte(), 'H'.code.toByte(), 'F'.code.toByte()))
            zip.closeEntry()
        }
        val scan = requireNotNull(MissionZip.inspect(archive))
        val store = MissionZipExtractionStore(filesDir)
        val record = store.ensureExtracted(archive.name, archive, scan)

        assertNotNull(store.freshRecord(archive.name, archive))
        val changedTime = record.ownerLastModifiedMs + 2_000L
        check(archive.setLastModified(changedTime))
        assertNull(store.freshRecord(archive.name, archive))
    }

    @Test
    fun outputProjectionRejectsPortablePathCollisionsInEitherOrder() {
        val collisions =
            listOf(
                listOf(file("first", "same.bin"), file("second", "same.bin")),
                listOf(file("upper", "Music/Track.ogg"), file("lower", "music/track.ogg")),
                listOf(file("slash", "docs/readme.txt"), file("backslash", "docs\\readme.txt")),
                listOf(file("dot", "a/../readme.txt"), file("plain", "readme.txt")),
                listOf(file("unicode upper", "\u00c4udio.ogg"), file("unicode lower", "\u00e4udio.ogg")),
                listOf(file("parent file", "data"), file("child", "data/level.rl2")),
                listOf(file("archive song", "missions/descent.sng"), file("generated song", "missions/descent.sng")),
            )
        for (collision in collisions) {
            val forward = collisionMessage(collision)
            val reverse = collisionMessage(collision.reversed())
            assertEquals(forward, reverse)
        }
    }

    @Test
    fun collidingReplacementPreservesPreviousPublishedGeneration() {
        val filesDir = File("build/test-mission-zip-extraction/collision-preserves-generation").absoluteFile
        filesDir.deleteRecursively()
        val modsDir = File(filesDir, "mods").apply { mkdirs() }
        val archive = File(modsDir, "preview.zip")
        writeMissionArchive(archive, listOf("docs/readme.txt" to "original"))
        val originalScan = requireNotNull(MissionZip.inspect(archive))
        val store = MissionZipExtractionStore(filesDir)
        val originalRecord = store.ensureExtracted(archive.name, archive, originalScan)
        val originalReadme = File(originalRecord.rootDir, "missions/docs/readme.txt")
        assertEquals("original", originalReadme.readText())

        writeMissionArchive(
            archive,
            listOf(
                "docs/readme.txt" to "replacement",
                "DOCS/README.TXT" to "collision",
            ),
        )
        assertTrue(archive.setLastModified(originalRecord.ownerLastModifiedMs + 2_000L))
        val collidingScan = requireNotNull(MissionZip.inspect(archive))
        collisionMessage { store.ensureExtracted(archive.name, archive, collidingScan) }

        assertTrue(originalRecord.rootDir.isDirectory)
        assertEquals("original", originalReadme.readText())
        assertFalse(File(originalRecord.rootDir.parentFile, "preview.zip.tmp").exists())
    }

    @Test
    fun generatedSongAliasCollisionRejectsBeforeTargetMutation() {
        val filesDir = File("build/test-mission-zip-extraction/generated-alias-collision").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()
        val archive = File(filesDir, "rooted.zip")
        ZipOutputStream(archive.outputStream()).use { zip ->
            zip.writeEntry("missions/rooted.mn2", missionDescriptor("Rooted"))
            zip.writeEntry("missions/rooted.hog", "DHF")
            zip.writeEntry("custom.sng", "level01.ogg")
            zip.writeEntry("missions/descent.sng", "existing.ogg")
        }
        val scan = requireNotNull(MissionZip.inspect(archive))
        val target = File(filesDir, "target").apply { mkdirs() }
        val sentinel = File(target, "sentinel.txt").apply { writeText("keep") }

        collisionMessage { extractZipToRoot(archive, target, scan) }

        assertEquals("keep", sentinel.readText())
        assertEquals(listOf("sentinel.txt"), target.list()?.sorted()?.toList())
    }

    private fun file(
        sourcePath: String,
        relativePath: String,
    ): ArchiveOutputProjection = ArchiveOutputProjection(sourcePath, relativePath, false)

    private fun collisionMessage(projections: List<ArchiveOutputProjection>): String =
        collisionMessage { validateArchiveOutputProjections(projections, "test archive") }

    private fun collisionMessage(block: () -> Unit): String =
        try {
            block()
            fail("Expected an archive output collision")
            ""
        } catch (expected: IllegalArgumentException) {
            requireNotNull(expected.message)
        }

    private fun writeMissionArchive(
        archive: File,
        extras: List<Pair<String, String>>,
    ) {
        ZipOutputStream(archive.outputStream()).use { zip ->
            zip.writeEntry("preview.mn2", missionDescriptor("Preview"))
            zip.writeEntry("preview.hog", "DHF")
            for ((path, contents) in extras) zip.writeEntry(path, contents)
        }
    }

    private fun missionDescriptor(name: String): String =
        """
        name = $name
        type = normal
        num_levels = 1
        preview.rl2
        """.trimIndent()

    private fun ZipOutputStream.writeEntry(
        path: String,
        contents: String,
    ) {
        putNextEntry(ZipEntry(path))
        write(contents.toByteArray())
        closeEntry()
    }
}
