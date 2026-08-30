package com.dxxredux.app.multiplayer

import com.dxxredux.app.MissionContentIdentity
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.RandomAccessFile

class MissionTransferResumeTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    @Test
    fun completedUnalignedFinalChunkRemainsResumable() {
        val chunkSize = 1024
        val bytes = ByteArray(chunkSize + 37) { index -> index.toByte() }
        val partial = temporaryFolder.newFile("complete.partial").apply { writeBytes(bytes) }
        val identity = MissionContentIdentity.compute(partial, chunkSize)

        val verified =
            MissionTransferService.validatePartial(
                partial,
                bytes.size.toLong(),
                bytes.size.toLong(),
                chunkSize,
                identity.chunkSha256,
            )

        assertEquals(bytes.size.toLong(), verified)
        assertEquals(bytes.size.toLong(), partial.length())
    }

    @Test
    fun corruptChunkRollsBackToLastVerifiedBoundary() {
        val chunkSize = 1024
        val bytes = ByteArray(chunkSize * 2 + 31) { index -> index.toByte() }
        val source = temporaryFolder.newFile("source.zip").apply { writeBytes(bytes) }
        val identity = MissionContentIdentity.compute(source, chunkSize)
        val partial = temporaryFolder.newFile("corrupt.partial").apply { writeBytes(bytes) }
        RandomAccessFile(partial, "rw").use { file ->
            file.seek((chunkSize + 7).toLong())
            file.write(0xff)
        }

        val verified =
            MissionTransferService.validatePartial(
                partial,
                bytes.size.toLong(),
                bytes.size.toLong(),
                chunkSize,
                identity.chunkSha256,
            )

        assertEquals(chunkSize.toLong(), verified)
        assertEquals(chunkSize.toLong(), partial.length())
    }
}
