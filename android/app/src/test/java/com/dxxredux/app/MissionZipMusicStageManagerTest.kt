package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayOutputStream
import java.io.File
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class MissionZipMusicStageManagerTest {
    @Test
    fun stagesTopLevelAudioTrack() {
        val payload = byteArrayOf(1, 3, 5, 7)
        val zip = createZip("stage-top-level") {
            writeEntry("song01.ogg", payload)
        }
        val catalog = MissionZipMusic.inspect(zip)
        val track = catalog!!.sources.single().tracks.single()

        val staged = MissionZipMusicStageManager(testCacheDir("stage-top-level")).stageCompressedAudioTrack(catalog, track)

        assertNotNull(staged)
        assertArrayEquals(payload, staged!!.readBytes())
    }

    @Test
    fun stagesNestedDxaAudioTrack() {
        val payload = byteArrayOf(2, 4, 6, 8)
        val zip = createZip("stage-dxa") {
            writeEntry(
                "music.dxa",
                createZipBytes {
                    writeEntry("song01.ogg", payload)
                },
            )
        }
        val catalog = MissionZipMusic.inspect(zip)
        val track = catalog!!.sources.single().tracks.single()

        val staged = MissionZipMusicStageManager(testCacheDir("stage-dxa")).stageCompressedAudioTrack(catalog, track)

        assertNotNull(staged)
        assertArrayEquals(payload, staged!!.readBytes())
    }

    @Test
    fun stagesHogContainedAudioTrack() {
        val payload = byteArrayOf(9, 7, 5, 3)
        val zip = createZip("stage-hog") {
            writeEntry(
                "mission.hog",
                createHogBytes(
                    "level01.rl2" to ByteArray(8),
                    "game01.ogg" to payload,
                ),
            )
        }
        val catalog = MissionZipMusic.inspect(zip)
        val track = catalog!!.sources.single().tracks.single()

        val staged = MissionZipMusicStageManager(testCacheDir("stage-hog")).stageCompressedAudioTrack(catalog, track)

        assertNotNull(staged)
        assertArrayEquals(payload, staged!!.readBytes())
    }

    @Test
    fun stagesSelectedAudioTrackFromLargeZipOnly() {
        val payload = byteArrayOf(11, 13, 17, 19)
        val zip = createZip("stage-large") {
            writeEntry("song01.ogg", payload)
            writeLargeEntry("padding.dat", MissionZip.SMALL_IN_MEMORY_LIMIT_BYTES + 1024L)
        }
        val catalog = MissionZipMusic.inspect(zip)
        val track = catalog!!.sources.single().tracks.single()
        val cacheDir = testCacheDir("stage-large")

        val staged = MissionZipMusicStageManager(cacheDir).stageCompressedAudioTrack(catalog, track)

        assertNotNull(staged)
        assertArrayEquals(payload, staged!!.readBytes())
        val stagedFiles = cacheDir.walkTopDown().filter { it.isFile }.toList()
        assertEquals(1, stagedFiles.size)
        assertEquals(payload.size.toLong(), stagedFiles.single().length())
        assertTrue(zip.length() < MissionZip.SMALL_IN_MEMORY_LIMIT_BYTES)
    }

    @Test
    fun readsTopLevelMidiTrackBytes() {
        val payload = byteArrayOf(0x4d, 0x54, 0x68, 0x64)
        val zip = createZip("stage-midi-top-level") {
            writeEntry("song01.mid", payload)
        }
        val catalog = MissionZipMusic.inspect(zip)
        val track = catalog!!.sources.single().tracks.single()

        val data = MissionZipMusicStageManager(testCacheDir("stage-midi-top-level")).readMidiTrackBytes(catalog, track)

        assertArrayEquals(payload, data)
    }

    @Test
    fun readsNestedDxaMidiTrackBytes() {
        val payload = byteArrayOf(0x48, 0x4d, 0x49, 0x4d)
        val zip = createZip("stage-midi-dxa") {
            writeEntry(
                "music.dxa",
                createZipBytes {
                    writeEntry("song01.hmp", payload)
                },
            )
        }
        val catalog = MissionZipMusic.inspect(zip)
        val track = catalog!!.sources.single().tracks.single()

        val data = MissionZipMusicStageManager(testCacheDir("stage-midi-dxa")).readMidiTrackBytes(catalog, track)

        assertArrayEquals(payload, data)
    }

    @Test
    fun readsHogContainedMidiTrackBytes() {
        val payload = byteArrayOf(0x48, 0x4d, 0x49, 0x51)
        val zip = createZip("stage-midi-hog") {
            writeEntry(
                "mission.hog",
                createHogBytes(
                    "level01.rl2" to ByteArray(8),
                    "game01.hmq" to payload,
                ),
            )
        }
        val catalog = MissionZipMusic.inspect(zip)
        val track = catalog!!.sources.single().tracks.single()

        val data = MissionZipMusicStageManager(testCacheDir("stage-midi-hog")).readMidiTrackBytes(catalog, track)

        assertArrayEquals(payload, data)
    }

    private fun testCacheDir(name: String): File =
        File("build/test-mission-zip-music-stage/$name")
            .absoluteFile
            .also {
                it.deleteRecursively()
                it.mkdirs()
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

    private fun ZipOutputStream.writeLargeEntry(
        name: String,
        sizeBytes: Long,
    ) {
        putNextEntry(ZipEntry(name))
        val chunk = ByteArray(1024 * 1024)
        var state = 0x13579bdf
        var remaining = sizeBytes
        while (remaining > 0) {
            for (index in 0 until 4096) {
                state = state * 1103515245 + 12345
                chunk[index] = (state ushr 16).toByte()
            }
            val count = minOf(chunk.size.toLong(), remaining).toInt()
            write(chunk, 0, count)
            remaining -= count.toLong()
        }
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
