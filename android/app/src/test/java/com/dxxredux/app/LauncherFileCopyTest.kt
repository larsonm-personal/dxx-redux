package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException

class LauncherFileCopyTest {
    @Test
    fun copyStream_reportsProgressAndCopiesAllBytes() {
        val inputBytes = ByteArray(2 * 1024 * 1024 + 17) { (it and 0xff).toByte() }
        val progress = mutableListOf<LauncherCopyProgress>()
        val output = ByteArrayOutputStream()

        val copied =
            LauncherFileCopy.copyStream(
                ByteArrayInputStream(inputBytes),
                output,
                inputBytes.size.toLong(),
                "test.bin",
            ) { progress.add(it) }

        assertEquals(inputBytes.size.toLong(), copied)
        assertArrayEquals(inputBytes, output.toByteArray())
        assertTrue(progress.isNotEmpty())
        assertEquals(0L, progress.first().bytesDone)
        assertEquals(inputBytes.size.toLong(), progress.last().bytesDone)
        assertEquals(inputBytes.size.toLong(), progress.last().bytesTotal)
        assertEquals("test.bin", progress.last().label)
    }

    @Test
    fun copyInputToFile_preservesPriorFileWhenKnownLengthIsShort() {
        val dir = File("build/test-launcher-copy-short").absoluteFile
        dir.deleteRecursively()
        dir.mkdirs()
        val destination = File(dir, "game.hog").apply { writeText("prior") }

        assertThrows(IOException::class.java) {
            LauncherFileCopy.copyInputToFile(destination, 8L) {
                ByteArrayInputStream("short".toByteArray())
            }
        }

        assertEquals("prior", destination.readText())
        assertTrue(dir.listFiles().orEmpty().none { it.name != destination.name })
    }

    @Test
    fun copyInputToFile_replacesPriorFileOnlyAfterExactCopy() {
        val dir = File("build/test-launcher-copy-exact").absoluteFile
        dir.deleteRecursively()
        dir.mkdirs()
        val destination = File(dir, "game.hog").apply { writeText("prior") }
        val replacement = "complete".toByteArray()

        LauncherFileCopy.copyInputToFile(destination, replacement.size.toLong()) {
            ByteArrayInputStream(replacement)
        }

        assertArrayEquals(replacement, destination.readBytes())
        assertTrue(dir.listFiles().orEmpty().none { it.name != destination.name })
    }

    @Test
    fun copyStream_handlesOneTransientZeroRead() {
        val bytes = "complete".toByteArray()
        val delegate = ByteArrayInputStream(bytes)
        var returnedZero = false
        val input =
            object : java.io.InputStream() {
                override fun read(): Int = delegate.read()

                override fun read(
                    buffer: ByteArray,
                    offset: Int,
                    length: Int,
                ): Int {
                    if (!returnedZero) {
                        returnedZero = true
                        return 0
                    }
                    return delegate.read(buffer, offset, length)
                }
            }
        val output = ByteArrayOutputStream()

        LauncherFileCopy.copyStream(input, output, bytes.size.toLong(), "transient.bin")

        assertArrayEquals(bytes, output.toByteArray())
    }

    @Test
    fun copyInputToFile_preservesPriorFileWhenProviderExceedsDeclaredSize() {
        val dir = File("build/test-launcher-copy-long").absoluteFile
        dir.deleteRecursively()
        dir.mkdirs()
        val destination = File(dir, "game.hog").apply { writeText("prior") }

        assertThrows(IOException::class.java) {
            LauncherFileCopy.copyInputToFile(destination, 4L) {
                ByteArrayInputStream("longer".toByteArray())
            }
        }

        assertEquals("prior", destination.readText())
        assertTrue(dir.listFiles().orEmpty().none { it.name != destination.name })
    }

    @Test
    fun copyInputToFile_enforcesKnownAndStreamingLimitsBeforePublication() {
        val dir = File("build/test-launcher-copy-limit").absoluteFile
        dir.deleteRecursively()
        dir.mkdirs()
        val destination = File(dir, "disc.cue").apply { writeText("prior") }

        assertThrows(IOException::class.java) {
            LauncherFileCopy.copyInputToFile(destination, 5L, maxBytes = 4L) {
                ByteArrayInputStream("12345".toByteArray())
            }
        }
        assertThrows(IOException::class.java) {
            LauncherFileCopy.copyInputToFile(destination, 0L, maxBytes = 4L) {
                ByteArrayInputStream("12345".toByteArray())
            }
        }

        assertEquals("prior", destination.readText())
        assertTrue(dir.listFiles().orEmpty().none { it.name != destination.name })
    }

    @Test
    fun cueLimit_acceptsExactSizeAndRejectsEmptyOrOversizedFiles() {
        requireCueSizeWithinLimit(CD_CUE_MAX_BYTES, "exact.cue")
        assertThrows(IOException::class.java) { requireCueSizeWithinLimit(0L, "empty.cue") }
        assertThrows(IOException::class.java) {
            requireCueSizeWithinLimit(CD_CUE_MAX_BYTES + 1L, "large.cue")
        }
    }
}
