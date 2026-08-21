package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.json.JSONObject
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
                    embeddedTrackNames = mapOf("one.ogg" to "Embedded One", "two.ogg" to "Embedded Two"),
                    trackConfidences = mapOf("one.ogg" to 0.9f, "two.ogg" to 0.8f),
                    trackNumbers = mapOf("one.ogg" to 1, "two.ogg" to 2),
                    referencedUris = mapOf("one.ogg" to "content://one", "two.ogg" to "content://two"),
                ),
                "one.ogg",
            )!!

        assertEquals(listOf("two.ogg"), remaining.files)
        assertEquals(mapOf("two.ogg" to "Two"), remaining.trackNames)
        assertEquals(mapOf("two.ogg" to "Embedded Two"), remaining.embeddedTrackNames)
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

    @Test
    fun embeddedNamesPersistAndPopulatePlaylistSidecar() {
        val filesDir = File("build/test-custom-audio-embedded").absoluteFile
        filesDir.deleteRecursively()
        val trackDir = File(filesDir, "${CustomAudioSetManager.MUSIC_DIR}/set-a")
        trackDir.mkdirs()
        File(trackDir, "fallback.ogg").writeText("fixture")
        File(trackDir, "matched.ogg").writeText("fixture")
        CustomAudioSetManager(filesDir).addSet(
            CustomAudioSetManager.AudioSet(
                id = "set-a",
                label = "Set A",
                files = listOf("fallback.ogg", "matched.ogg"),
                trackNames = mapOf("matched.ogg" to "Database Match"),
                embeddedTrackNames =
                    mapOf(
                        "fallback.ogg" to "Embedded Fallback (Composer)",
                        "matched.ogg" to "Embedded Matched",
                    ),
            ),
        )

        val reloaded = CustomAudioSetManager(filesDir)
        assertEquals("Embedded Fallback (Composer)", reloaded.getSets().single().embeddedTrackNames["fallback.ogg"])
        reloaded.writeM3U()

        val records = JSONObject(File(filesDir, CustomAudioSetManager.NAMES_FILE).readText()).getJSONArray("records")
        val names = (0 until records.length()).map { records.getJSONObject(it).getString("name") }
        assertEquals(listOf("Embedded Fallback (Composer)", "Database Match"), names)
    }

    @Test
    fun referencedTrackIsStagedParsedPersistedAndUsesMatchFirst() {
        val filesDir = File("build/test-custom-audio-referenced").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()
        val manager = CustomAudioSetManager(filesDir)
        manager.addSet(
            CustomAudioSetManager.AudioSet(
                id = "linked",
                label = "Linked",
                files = listOf("fallback.flac", "matched.ogg"),
                trackNames = mapOf("matched.ogg" to "Database Match"),
                referencedUris =
                    mapOf(
                        "fallback.flac" to "content://test/fallback",
                        "matched.ogg" to "content://test/matched",
                    ),
            ),
        )
        val stagedUris = mutableListOf<String>()

        manager.writeM3UWith(
            referenceStager = { uri, target, _ ->
                stagedUris += uri
                target.writeText("staged")
            },
            embeddedNameReader = { _, extension -> "Embedded ${extension.uppercase()}" },
        )

        assertEquals(listOf("content://test/fallback", "content://test/matched"), stagedUris)
        assertEquals(
            mapOf("fallback.flac" to "Embedded FLAC", "matched.ogg" to "Embedded OGG"),
            CustomAudioSetManager(filesDir).getSets().single().embeddedTrackNames,
        )
        val records = JSONObject(File(filesDir, CustomAudioSetManager.NAMES_FILE).readText()).getJSONArray("records")
        val names = (0 until records.length()).map { records.getJSONObject(it).getString("name") }
        assertEquals(listOf("Embedded FLAC", "Database Match"), names)
        val playlist = File(filesDir, CustomAudioSetManager.PLAYLIST_FILE).readLines()
        assertTrue(playlist.any { it.endsWith("linked_fallback.flac") })
        assertTrue(playlist.any { it.endsWith("linked_matched.ogg") })
    }
}
