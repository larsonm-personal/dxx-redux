package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream

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
}
