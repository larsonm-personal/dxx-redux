package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.json.JSONObject
import java.io.File

class AudioSourceManagerPersistenceTest {
    @Test
    fun fileSetScopedRegistriesKeepSourcesIsolated() {
        val filesDir = File("build/test-audiosrc-file-set-isolation").absoluteFile
        filesDir.deleteRecursively()
        val setA = File(filesDir, "sets/a").apply { mkdirs() }
        val setB = File(filesDir, "sets/b").apply { mkdirs() }
        val managerA = AudioSourceManager(filesDir, setA)
        val managerB = AudioSourceManager(filesDir, setB)
        val source =
            AudioSourceManager.AudioSource(
                id = "set-a-disc",
                cuePath = "disc.cue",
                binPaths = listOf("disc.bin"),
                discLabel = "Set A",
                discId = "unknown",
                trackCount = 2,
                audioTrackCount = 1,
                legacyDiscId = 0L,
            )
        AudioSourceManager::class.java.getDeclaredField("sources").apply {
            isAccessible = true
            set(managerA, mutableListOf(source))
        }
        AudioSourceManager::class.java.getDeclaredMethod("save").apply {
            isAccessible = true
            invoke(managerA)
        }

        assertEquals(listOf("set-a-disc"), AudioSourceManager(filesDir, setA).getSources().map { it.id })
        assertTrue(managerB.getSources().isEmpty())
        assertTrue(File(setA, ".content/audio/audio_sources.json").isFile)
        assertEquals(false, File(setB, ".content/audio/audio_sources.json").exists())
        assertEquals(false, File(filesDir, "audio_sources.json").exists())
    }

    @Test
    fun managedCueBundleRemainsRegisteredWhenItsContentEntryIsDisabled() {
        val filesDir = File("build/test-audiosrc-managed-content-toggle").absoluteFile
        filesDir.deleteRecursively()
        val setDir = File(filesDir, "sets/default").apply { mkdirs() }
        File(setDir, "disc.cue").writeText("FILE disc.bin BINARY\n  TRACK 01 MODE1/2352\n")
        File(setDir, "disc.bin").writeText("disc")
        val contentManager = FileSetContentManager(setDir)
        val contentId = contentManager.reconcile().entries.single().id
        val audioManager = AudioSourceManager(filesDir, setDir)
        val source =
            AudioSourceManager.AudioSource(
                id = "managed-disc",
                cuePath = "disc.cue",
                binPaths = listOf("disc.bin"),
                discLabel = "Managed disc",
                discId = "unknown",
                trackCount = 2,
                audioTrackCount = 1,
                legacyDiscId = 0L,
            )
        AudioSourceManager::class.java.getDeclaredField("sources").apply {
            isAccessible = true
            set(audioManager, mutableListOf(source))
        }
        AudioSourceManager::class.java.getDeclaredMethod("save").apply {
            isAccessible = true
            invoke(audioManager)
        }

        assertTrue(audioManager.writePlaylist())
        contentManager.setEnabled(contentId, false)
        assertTrue(audioManager.pruneMissingSources(setDir).isEmpty())
        assertEquals(listOf("managed-disc"), audioManager.getSources().map { it.id })
        assertFalse(audioManager.writePlaylist())
    }

    @Test
    fun savesMultipleBinContentUris() {
        val filesDir = File("build/test-audiosrc-persistence-multibin").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()

        val manager = AudioSourceManager(filesDir)
        val source =
            AudioSourceManager.AudioSource(
                id = "multi-bin-saf",
                cuePath = "disc.cue",
                binPaths = listOf("disc-a.bin", "disc-b.bin"),
                discLabel = "Multi",
                discId = "unknown",
                trackCount = 12,
                audioTrackCount = 11,
                audioTrackNumbers = (2..12).toList(),
                legacyDiscId = 0L,
                binContentUris = listOf("content://disc-a", "content://disc-b"),
                cueContentUri = "content://disc-cue",
            )
        AudioSourceManager::class.java.getDeclaredField("sources").apply {
            isAccessible = true
            set(manager, mutableListOf(source))
        }
        AudioSourceManager::class.java.getDeclaredMethod("save").apply {
            isAccessible = true
            invoke(manager)
        }

        val saved = JSONObject(File(filesDir, "audio_sources.json").readText())
        val savedSource = saved.getJSONArray("sources").getJSONObject(0)
        val savedUris = savedSource.getJSONArray("bin_content_uris")
        val savedTrackNumbers = savedSource.getJSONArray("audio_track_numbers")

        assertEquals(listOf("content://disc-a", "content://disc-b"), (0 until savedUris.length()).map(savedUris::getString))
        assertEquals((2..12).toList(), (0 until savedTrackNumbers.length()).map(savedTrackNumbers::getInt))
        assertEquals("content://disc-a", savedSource.getString("bin_content_uri"))
        assertEquals("content://disc-cue", savedSource.getString("cue_content_uri"))
        assertEquals((2..12).toList(), AudioSourceManager(filesDir).getSources().single().audioTrackNumbers)
    }

    @Test
    fun removingSourceRetainsUrisOwnedByRemainingSources() {
        val filesDir = File("build/test-audiosrc-remove-shared-tree").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()
        val tree = "content://provider/tree/music"
        val firstBin = "$tree/document/music%2Ffirst.bin"
        val secondBin = "$tree/document/music%2Fsecond.bin"
        val manager = AudioSourceManager(filesDir)

        fun source(id: String, binUri: String) =
            AudioSourceManager.AudioSource(
                id = id,
                cuePath = "$id.cue",
                binPaths = listOf("$id.bin"),
                discLabel = id,
                discId = "unknown",
                trackCount = 2,
                audioTrackCount = 1,
                legacyDiscId = 0L,
                binContentUris = listOf(binUri),
            )

        AudioSourceManager::class.java.getDeclaredField("sources").apply {
            isAccessible = true
            set(manager, mutableListOf(source("first", firstBin), source("second", secondBin)))
        }

        var removedUris: Collection<String> = emptyList()
        var retainedUris: Collection<String> = emptyList()
        manager.removeSource("first") { removed, retained ->
            removedUris = removed
            retainedUris = retained
        }

        assertEquals(listOf(firstBin), removedUris)
        assertEquals(listOf(secondBin), retainedUris)
        assertTrue(collectPersistedPermissionUrisToRelease(listOf(tree), removedUris, retainedUris).isEmpty())
        assertEquals(listOf("second"), AudioSourceManager(filesDir).getSources().map { it.id })

        manager.removeSource("second") { removed, retained ->
            removedUris = removed
            retainedUris = retained
        }
        assertEquals(setOf(tree), collectPersistedPermissionUrisToRelease(listOf(tree), removedUris, retainedUris))
    }

    @Test
    fun emptyPlaylistGenerationPreservesLastPublishedPlaylist() {
        val filesDir = File("build/test-audiosrc-preserve-playlist").absoluteFile
        filesDir.deleteRecursively()
        filesDir.mkdirs()
        val playlist = File(filesDir, "audio_playlist.json")
        playlist.writeText("last-good\n")

        assertEquals(false, AudioSourceManager(filesDir).writePlaylist())
        assertEquals("last-good\n", playlist.readText())
    }
}
