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

    @Test
    fun transferRateUsesElapsedTime() {
        assertEquals(
            16L * 1024L * 1024L,
            MissionTransferService.averageBytesPerSecond(16L * 1024L * 1024L, 1_000_000_000L),
        )
        assertEquals(0L, MissionTransferService.averageBytesPerSecond(0L, 1_000_000_000L))
    }

    @Test
    fun completedTransferReportRetainsOrderingIdentity() {
        val requirement =
            MissionRequirement(
                revision = "revision",
                game = "d2",
                missionKey = "castaway",
                displayName = "Castaway Redux",
                kind = MissionRequirement.KIND_WRAPPER,
                wrapperFilename = "castaway.zip",
                sizeBytes = 180L,
                sha256 = "0".repeat(64),
                offerAvailable = true,
            )

        val report =
            MissionTransferService.transferReport(
                requirement,
                MissionCompatibilityStatus.MATCH,
                verifiedBytes = 180L,
                transferId = "transfer-token",
                attempt = 2,
            )

        assertEquals(MissionCompatibilityStatus.MATCH, report.status)
        assertEquals("transfer-token", report.transferId)
        assertEquals(2, report.attempt)
        assertEquals(180L, report.verifiedBytes)
    }
}
