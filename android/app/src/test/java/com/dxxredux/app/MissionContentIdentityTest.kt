package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.security.MessageDigest

class MissionContentIdentityTest {
    @get:Rule val temporaryFolder = TemporaryFolder()

    @Test
    fun computeProducesStableWholeAndChunkHashesInOneManifest() {
        val bytes = ByteArray(MissionContentIdentity.DEFAULT_CHUNK_SIZE_BYTES + 17) { index -> index.toByte() }
        val archive = temporaryFolder.newFile("mission.zip").apply { writeBytes(bytes) }

        val progress = mutableListOf<Long>()
        val identity = MissionContentIdentity.compute(archive) { progress += it }

        assertEquals(bytes.size.toLong(), identity.sizeBytes)
        assertEquals(sha256(bytes), identity.sha256)
        assertEquals(MissionContentIdentity.DEFAULT_CHUNK_SIZE_BYTES, identity.chunkSizeBytes)
        assertEquals(2, identity.chunkSha256.size)
        assertEquals(
            listOf(MissionContentIdentity.DEFAULT_CHUNK_SIZE_BYTES.toLong(), bytes.size.toLong()),
            progress,
        )
        assertEquals(sha256(bytes.copyOfRange(0, MissionContentIdentity.DEFAULT_CHUNK_SIZE_BYTES)), identity.chunkSha256[0])
        assertEquals(sha256(bytes.copyOfRange(MissionContentIdentity.DEFAULT_CHUNK_SIZE_BYTES, bytes.size)), identity.chunkSha256[1])
    }

    private fun sha256(bytes: ByteArray): String =
        MessageDigest.getInstance("SHA-256").digest(bytes).joinToString("") { "%02x".format(it) }
}
