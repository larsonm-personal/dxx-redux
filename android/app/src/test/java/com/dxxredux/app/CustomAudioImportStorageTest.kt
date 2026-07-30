package com.dxxredux.app

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.rules.TemporaryFolder
import java.io.File

class CustomAudioImportStorageTest {
    @get:Rule
    val temporaryFolder = TemporaryFolder()

    @Test
    fun failedAttemptCleanupPreservesExistingSet() {
        val setDir = File(temporaryFolder.root, "custom_music/set-a")
        assertTrue(setDir.mkdirs())
        val existingTrack = File(setDir, "existing.ogg")
        val existingBytes = byteArrayOf(1, 2, 3, 4)
        existingTrack.writeBytes(existingBytes)

        val attemptDir = createCopiedAudioImportAttemptDir(setDir)
        assertTrue(attemptDir.isDirectory)
        File(attemptDir, "partial.ogg").writeBytes(byteArrayOf(9, 9))
        assertTrue(attemptDir.deleteRecursively())

        assertTrue(setDir.isDirectory)
        assertArrayEquals(existingBytes, existingTrack.readBytes())
        assertFalse(File(setDir, "partial.ogg").exists())
    }

    @Test
    fun emptyAttemptDoesNotPublishNewSet() {
        val setDir = File(temporaryFolder.root, "custom_music/set-new")
        val attemptDir = createCopiedAudioImportAttemptDir(setDir)

        assertTrue(attemptDir.deleteRecursively())

        assertFalse(setDir.exists())
    }

    @Test
    fun publishingAttemptAppendsWithoutRemovingExistingTrack() {
        val setDir = File(temporaryFolder.root, "custom_music/set-a")
        assertTrue(setDir.mkdirs())
        val existingTrack = File(setDir, "existing.ogg")
        val existingBytes = byteArrayOf(1, 2, 3, 4)
        existingTrack.writeBytes(existingBytes)
        val attemptDir = createCopiedAudioImportAttemptDir(setDir)
        File(attemptDir, "added.ogg").writeBytes(byteArrayOf(5, 6, 7))

        publishCopiedAudioImport(attemptDir, setDir, listOf("added.ogg"))
        attemptDir.deleteRecursively()

        assertArrayEquals(existingBytes, existingTrack.readBytes())
        assertArrayEquals(byteArrayOf(5, 6, 7), File(setDir, "added.ogg").readBytes())
        assertFalse(attemptDir.exists())
    }

    @Test
    fun collisionPreflightRejectsExistingAndAttemptNamesCaseInsensitively() {
        val setDir = File(temporaryFolder.root, "custom_music/set-a")
        assertTrue(setDir.mkdirs())
        File(setDir, "existing.ogg").writeBytes(byteArrayOf(1))

        assertEquals(
            "EXISTING.OGG",
            findCopiedAudioImportCollision(setDir, emptyList(), listOf("EXISTING.OGG")),
        )
        assertEquals(
            "track.OGG",
            findCopiedAudioImportCollision(setDir, emptyList(), listOf("Track.ogg", "track.OGG")),
        )
        assertEquals(
            "linked.flac",
            findCopiedAudioImportCollision(setDir, listOf("Linked.FLAC"), listOf("linked.flac")),
        )
        assertNull(findCopiedAudioImportCollision(setDir, listOf("linked.flac"), listOf("new.mp3")))
    }
}
