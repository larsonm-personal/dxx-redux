package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class MissionZipMusicTest {
    @Test
    fun detectsTopLevelSongListReferences() {
        val zip = createZip("music-top-level") {
            writeEntry("mission.mn2", "name = Music Test\nnum_levels = 1\nlevel01.rl2\n".toByteArray())
            writeEntry("mission.hog", createHogBytes("level01.rl2" to ByteArray(8)))
            writeEntry("descent.sng", "descent.hmp\nbriefing.hmp\ngame01.hmp\n".toByteArray())
        }

        val catalog = MissionZipMusic.inspect(zip)

        assertNotNull(catalog)
        val archiveSource = catalog!!.sources.single { it.id == "archive" }
        assertEquals(listOf("descent.hmp", "briefing.hmp", "game01.hmp"), archiveSource.tracks.map { it.displayName })
        assertTrue(archiveSource.tracks.all { it.kind == MissionZipMusic.KIND_SONG_REFERENCE })
    }

    @Test
    fun usesTopLevelSongListToOrderPlayableTracksWithoutDuplicateReferences() {
        val zip = createZip("music-top-level-playable") {
            writeEntry("mission.mn2", "name = Music Test\nnum_levels = 1\nlevel01.rl2\n".toByteArray())
            writeEntry("mission.hog", createHogBytes("level01.rl2" to ByteArray(8)))
            writeEntry("briefing.hmp", ByteArray(16))
            writeEntry("game01.hmp", ByteArray(24))
            writeEntry("descent.sng", "game01.hmp\nbriefing.hmp\nmissing.hmp\n".toByteArray())
        }

        val catalog = MissionZipMusic.inspect(zip)

        assertNotNull(catalog)
        val archiveSource = catalog!!.sources.single { it.id == "archive" }
        assertEquals(listOf("game01.hmp", "briefing.hmp", "missing.hmp"), archiveSource.tracks.map { it.displayName })
        assertEquals(listOf(true, true, false), archiveSource.tracks.map { it.playable })
    }

    @Test
    fun detectsHogMidiTrack() {
        val zip = createZip("music-hog-midi") {
            writeEntry("mission.mn2", "name = Music Test\nnum_levels = 1\nlevel01.rl2\n".toByteArray())
            writeEntry(
                "mission.hog",
                createHogBytes(
                    "level01.rl2" to ByteArray(8),
                    "game01.hmp" to ByteArray(16),
                ),
            )
        }

        val catalog = MissionZipMusic.inspect(zip)

        assertNotNull(catalog)
        val track = catalog!!.sources.single().tracks.single()
        assertEquals("game01.hmp", track.displayName)
        assertEquals(MissionZipMusic.KIND_MIDI, track.kind)
        assertEquals("mission.hog", track.archiveEntryPath)
        assertEquals("game01.hmp", track.hogEntryName)
    }

    @Test
    fun detectsNestedDxaAudioTrack() {
        val zip = createZip("music-dxa-audio") {
            writeEntry("mission.mn2", "name = Music Test\nnum_levels = 1\nlevel01.rl2\n".toByteArray())
            writeEntry("mission.hog", createHogBytes("level01.rl2" to ByteArray(8)))
            writeEntry(
                "music.dxa",
                createZipBytes {
                    writeEntry("descent.sng", "briefing.ogg\nlevel01.ogg\n".toByteArray())
                    writeEntry("level01.ogg", ByteArray(24))
                },
            )
        }

        val catalog = MissionZipMusic.inspect(zip)

        assertNotNull(catalog)
        val dxaSource = catalog!!.sources.single { it.containerPath == "music.dxa" }
        assertTrue(dxaSource.tracks.any { it.displayName == "briefing.ogg" && !it.playable })
        assertTrue(
            dxaSource.tracks.any {
                it.displayName == "level01.ogg" &&
                    it.kind == MissionZipMusic.KIND_COMPRESSED_AUDIO &&
                    it.nestedEntryPath == "level01.ogg"
            },
        )
    }

    @Test
    fun detectsHogContainedSongListAndOggTracks() {
        val zip = createZip("music-hog-ogg") {
            writeEntry("trine2.mn2", "name = Trine 2\nnum_levels = 1\nlevel01.rl2\n".toByteArray())
            writeEntry(
                "trine2.hog",
                createHogBytes(
                    "briefing.ogg" to ByteArray(40),
                    "descent.sng" to "descent.ogg\nbriefing.ogg\ngame01.ogg\nmissing.ogg\n".toByteArray(),
                    "descent.ogg" to ByteArray(32),
                    "game01.ogg" to ByteArray(48),
                    "level01.rl2" to ByteArray(8),
                ),
            )
        }

        val catalog = MissionZipMusic.inspect(zip)

        assertNotNull(catalog)
        val hogSource = catalog!!.sources.single { it.containerPath == "trine2.hog" }
        assertEquals(4, hogSource.tracks.size)
        assertEquals(
            listOf("descent.ogg", "briefing.ogg", "game01.ogg", "missing.ogg"),
            hogSource.tracks.map { it.displayName },
        )
        assertEquals(
            listOf(true, true, true, false),
            hogSource.tracks.map { it.playable },
        )
    }

    @Test
    fun returnsNullForZipWithNoMusic() {
        val zip = createZip("music-none") {
            writeEntry("mission.mn2", "name = Music Test\nnum_levels = 1\nlevel01.rl2\n".toByteArray())
            writeEntry("mission.hog", createHogBytes("level01.rl2" to ByteArray(8)))
        }

        assertNull(MissionZipMusic.inspect(zip))
    }

    private fun createZip(
        prefix: String,
        writeEntries: ZipOutputStream.() -> Unit,
    ): File {
        val zipFile = File.createTempFile(prefix, ".zip")
        zipFile.deleteOnExit()
        ZipOutputStream(zipFile.outputStream()).use { it.writeEntries() }
        return zipFile
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
                output.write(fixedName(name, 13))
                output.write(leInt(data.size))
                output.write(data)
            }
            output.toByteArray()
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
