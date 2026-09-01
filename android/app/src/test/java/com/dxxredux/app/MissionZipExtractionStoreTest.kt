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
            zip.write(missionHog())
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
    fun freshRecordRejectsChangedArchiveWithRestoredSizeAndModificationTime() {
        val filesDir = File("build/test-mission-zip-extraction/source-content").absoluteFile
        filesDir.deleteRecursively()
        val modsDir = File(filesDir, "mods").apply { mkdirs() }
        val archive = File(modsDir, "preview.zip")
        writeMissionArchive(archive, listOf("docs/readme.txt" to "original"))
        val store = MissionZipExtractionStore(filesDir)
        val originalRecord = store.ensureExtracted(archive.name, archive, requireNotNull(MissionZip.inspect(archive)))
        val originalLength = archive.length()

        writeMissionArchive(archive, listOf("docs/readme.txt" to "changed!"))
        assertEquals(originalLength, archive.length())
        assertTrue(archive.setLastModified(originalRecord.ownerLastModifiedMs))

        assertNull(store.freshRecord(archive.name, archive))
        val replacement = store.ensureExtracted(archive.name, archive, requireNotNull(MissionZip.inspect(archive)))
        assertEquals("changed!", File(replacement.rootDir, "docs/readme.txt").readText())
    }

    @Test
    fun freshRecordRejectsSameSizeExtractedFileReplacement() {
        val filesDir = File("build/test-mission-zip-extraction/output-content").absoluteFile
        filesDir.deleteRecursively()
        val modsDir = File(filesDir, "mods").apply { mkdirs() }
        val archive = File(modsDir, "preview.zip")
        writeMissionArchive(archive, listOf("docs/readme.txt" to "original"))
        val store = MissionZipExtractionStore(filesDir)
        val originalRecord = store.ensureExtracted(archive.name, archive, requireNotNull(MissionZip.inspect(archive)))
        val extracted = File(originalRecord.rootDir, "docs/readme.txt")

        extracted.writeText("changed!")
        assertEquals(8L, extracted.length())

        assertNull(store.freshRecord(archive.name, archive))
        val repaired = store.ensureExtracted(archive.name, archive, requireNotNull(MissionZip.inspect(archive)))
        assertEquals("original", File(repaired.rootDir, "docs/readme.txt").readText())
    }

    @Test
    fun extractedTargetPreservesParsedMissionDisplayName() {
        val filesDir = File("build/test-mission-zip-extraction/display-name").absoluteFile
        filesDir.deleteRecursively()
        val modsDir = File(filesDir, "mods").apply { mkdirs() }
        val archive = File(modsDir, "preview.zip")
        ZipOutputStream(archive.outputStream()).use { zip ->
            zip.writeEntry("preview.mn2", missionDescriptor("Detailed Preview Title"))
            zip.writeEntry("preview.hog", missionHog())
        }
        val scan = requireNotNull(MissionZip.inspect(archive))
        val store = MissionZipExtractionStore(filesDir)
        store.ensureExtracted(archive.name, archive, scan)

        val target = store.extractedTarget(archive.absolutePath, File(filesDir, "set"), scan, scan.missionSets.single())

        assertNotNull(target)
        assertEquals("Detailed Preview Title", target!!.missionDisplayName)
        assertEquals(setOf("normal", "coop"), target.missionModeFlags)
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
        val originalReadme = File(originalRecord.rootDir, "docs/readme.txt")
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
    fun generatedRootSongAliasDoesNotCollideWithMissionDirectoryFile() {
        val filesDir = File("build/test-mission-zip-extraction/generated-alias-collision").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()
        val archive = File(filesDir, "rooted.zip")
        ZipOutputStream(archive.outputStream()).use { zip ->
            zip.writeEntry("missions/rooted.mn2", missionDescriptor("Rooted"))
            zip.writeEntry("missions/rooted.hog", missionHog())
            zip.writeEntry("custom.sng", "level01.ogg")
            zip.writeEntry("missions/descent.sng", "existing.ogg")
        }
        val scan = requireNotNull(MissionZip.inspect(archive))
        val target = File(filesDir, "target")

        extractZipToRoot(archive, target, scan)

        assertEquals("level01.ogg", File(target, "descent.sng").readText())
        assertEquals("existing.ogg", File(target, "missions/descent.sng").readText())
    }

    @Test
    fun exposesOnlySelectedRebirthSiblingToMissionDirectory() {
        val filesDir = File("build/test-mission-zip-extraction/rebirth-variant").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()
        val archive = File(filesDir, "variants.zip")
        ZipOutputStream(archive.outputStream()).use { zip ->
            for (variant in listOf("D2X", "DOS", "REBIRTH")) {
                zip.writeEntry("$variant/ULTERIOR.mn2", missionDescriptor("Ulterior"))
                zip.writeEntry("$variant/ULTERIOR.hog", missionHog())
            }
            zip.writeEntry("BONUS.mn2", missionDescriptor("Bonus mission"))
            zip.writeEntry("BONUS.hog", missionHog())
            zip.writeEntry("TEST/EXITD2V.mn2", missionDescriptor("Test mission"))
            zip.writeEntry("TEST/EXITD2V.hog", missionHog())
        }
        val scan = requireNotNull(MissionZip.inspect(archive))
        val target = File(filesDir, "target")

        extractZipToRoot(archive, target, scan)

        assertTrue(File(target, "missions/REBIRTH/ULTERIOR.mn2").isFile)
        assertTrue(File(target, "missions/REBIRTH/ULTERIOR.hog").isFile)
        assertTrue(File(target, "missions/BONUS.mn2").isFile)
        assertTrue(File(target, "missions/BONUS.hog").isFile)
        assertFalse(File(target, "missions/D2X").exists())
        assertFalse(File(target, "missions/DOS").exists())
        assertFalse(File(target, "missions/TEST").exists())
        assertTrue(File(target, "D2X/ULTERIOR.mn2").isFile)
        assertTrue(File(target, "DOS/ULTERIOR.mn2").isFile)
        assertTrue(File(target, "TEST/EXITD2V.mn2").isFile)
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
            zip.writeEntry("preview.hog", missionHog())
            for ((path, contents) in extras) zip.writeEntry(path, contents)
        }
    }

    private fun missionDescriptor(name: String): String =
        """
        name = $name
        type = normal
        normal = yes
        coop = yes
        num_levels = 1
        preview.rl2
        """.trimIndent()

    private fun ZipOutputStream.writeEntry(
        path: String,
        contents: String,
    ) = writeEntry(path, contents.toByteArray())

    private fun ZipOutputStream.writeEntry(
        path: String,
        contents: ByteArray,
    ) {
        putNextEntry(ZipEntry(path))
        write(contents)
        closeEntry()
    }

    private fun missionHog(): ByteArray =
        java.io.ByteArrayOutputStream().use { output ->
            output.write("DHF".toByteArray(Charsets.US_ASCII))
            output.write("preview.rl2".toByteArray(Charsets.US_ASCII).copyOf(13))
            output.write(byteArrayOf(1, 0, 0, 0, 0))
            output.toByteArray()
        }
}
