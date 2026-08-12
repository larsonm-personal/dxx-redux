package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class CustomAudioSetManagerTest {
    @Test
    fun removeReferencedFileFromSetDropsOnlyThatTrackMetadata() {
        val remaining =
            removeReferencedFileFromSet(
                CustomAudioSetManager.AudioSet(
                    id = "set-a",
                    label = "Set A",
                    files = listOf("one.ogg", "two.ogg"),
                    trackNames = mapOf("one.ogg" to "One", "two.ogg" to "Two"),
                    trackConfidences = mapOf("one.ogg" to 0.9f, "two.ogg" to 0.8f),
                    trackNumbers = mapOf("one.ogg" to 1, "two.ogg" to 2),
                    referencedUris = mapOf("one.ogg" to "content://one", "two.ogg" to "content://two"),
                ),
                "one.ogg",
            )!!

        assertEquals(listOf("two.ogg"), remaining.files)
        assertEquals(mapOf("two.ogg" to "Two"), remaining.trackNames)
        assertEquals(mapOf("two.ogg" to 0.8f), remaining.trackConfidences)
        assertEquals(mapOf("two.ogg" to 2), remaining.trackNumbers)
        assertEquals(mapOf("two.ogg" to "content://two"), remaining.referencedUris)
    }

    @Test
    fun removeReferencedFileFromSetReturnsNullForLastTrack() {
        val remaining =
            removeReferencedFileFromSet(
                CustomAudioSetManager.AudioSet(
                    id = "set-a",
                    label = "Set A",
                    files = listOf("one.ogg"),
                    referencedUris = mapOf("one.ogg" to "content://one"),
                ),
                "one.ogg",
            )

        assertNull(remaining)
    }

    @Test
    fun failedPlaylistGenerationPreservesLastPublishedPlaylist() {
        val filesDir = File("build/test-custom-audio-preserve").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()
        val playlist = File(filesDir, CustomAudioSetManager.PLAYLIST_FILE)
        val names = File(filesDir, CustomAudioSetManager.NAMES_FILE)
        playlist.writeText("last-good\n")
        names.writeText("last-good-names\n")
        CustomAudioSetManager(filesDir).addSet(
            CustomAudioSetManager.AudioSet("missing", "Missing", listOf("missing.ogg")),
        )

        assertNull(CustomAudioSetManager(filesDir).writeM3U())
        assertEquals("last-good\n", playlist.readText())
        assertEquals("last-good-names\n", names.readText())
        assertTrue(playlist.isFile)
    }
}
