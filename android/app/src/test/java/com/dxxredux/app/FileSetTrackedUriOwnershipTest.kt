package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class FileSetTrackedUriOwnershipTest {
    @Test
    fun setOwnershipIncludesBaseCdAndCustomMusicUrisWithoutRevokingSharedTrees() {
        val filesDir = File("build/test-file-set-tracked-uri-ownership").absoluteFile
        filesDir.deleteRecursively()
        val importRoot = File(filesDir, "imported")
        val fileSets = FileSetManager(filesDir, importRoot)
        val setA = fileSets.createSet("a")
        val setB = fileSets.createSet("b")
        val baseDocument = "content://provider/tree/base/document/base%2Fdescent2.hog"
        val sharedBin = "content://provider/tree/shared/document/shared%2Fdisc.bin"
        val sharedTrack = "content://provider/tree/shared/document/shared%2Ftrack.ogg"
        SafManifest.forDir(setA).write(listOf(SafManifest.SafFileEntry("descent2.hog", baseDocument, 1)))

        val audio = AudioSourceManager(filesDir, setA)
        val source =
            AudioSourceManager.AudioSource(
                id = "shared-disc",
                cuePath = "disc.cue",
                binPaths = listOf("disc.bin"),
                discLabel = "Shared disc",
                discId = "unknown",
                trackCount = 2,
                audioTrackCount = 1,
                legacyDiscId = 0L,
                binContentUris = listOf(sharedBin),
            )
        AudioSourceManager::class.java.getDeclaredField("sources").apply {
            isAccessible = true
            set(audio, mutableListOf(source))
        }
        AudioSourceManager::class.java.getDeclaredMethod("save").apply {
            isAccessible = true
            invoke(audio)
        }
        CustomAudioSetManager(filesDir, setB).addSet(
            CustomAudioSetManager.AudioSet(
                id = "shared-music",
                label = "Shared music",
                files = listOf("track.ogg"),
                referencedUris = mapOf("track.ogg" to sharedTrack),
            ),
        )

        val removed = fileSets.trackedContentUrisForSet("a")
        val retained = fileSets.trackedContentUrisForSet("b")
        assertEquals(setOf(baseDocument, sharedBin), removed.toSet())
        assertEquals(setOf(sharedTrack), retained.toSet())
        assertEquals(
            setOf("content://provider/tree/base"),
            collectPersistedPermissionUrisToRelease(
                listOf("content://provider/tree/base", "content://provider/tree/shared"),
                removed,
                retained,
            ),
        )

        fileSets.deleteSet("a")
        assertFalse(setA.exists())
        assertTrue(setB.isDirectory)
        assertEquals(setOf(sharedTrack), fileSets.trackedContentUrisForSet("b").toSet())
    }
}
