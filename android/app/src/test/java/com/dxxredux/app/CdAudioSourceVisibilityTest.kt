package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import java.io.File

class CdAudioSourceVisibilityTest {
    @Test
    fun hidesSourcesWithBrokenSafUris() {
        val localSource = testSource(id = "local")
        val localPathFile = File.createTempFile("disc", ".bin")
        localPathFile.deleteOnExit()
        val localPathSource = testSource(id = "local-path", binContentUri = localPathFile.absolutePath)
        val goodSafSource = testSource(id = "good", binContentUri = "content://good-bin", cueContentUri = "content://good-cue")
        val brokenBinSource = testSource(id = "broken-bin", binContentUri = "content://broken-bin", cueContentUri = "content://good-cue")
        val brokenCueSource = testSource(id = "broken-cue", binContentUri = "content://good-bin", cueContentUri = "content://broken-cue")

        val visibleIds =
            listOf(localSource, localPathSource, goodSafSource, brokenBinSource, brokenCueSource)
                .filter { source ->
                    shouldDisplayCdAudioSource(source) { uri, _ -> !uri.contains("broken") }
                }.map { it.id }

        assertEquals(listOf("local", "local-path", "good"), visibleIds)
    }

    @Test
    fun resolvesLocalPreviewPathForMergedAndRelativeSources() {
        val filesDir = File("build/test-files")
        val mergedBin = File.createTempFile("merged-disc", ".bin")
        mergedBin.deleteOnExit()

        val mergedSource = testSource(id = "merged", binContentUri = mergedBin.absolutePath)
        val relativeSource = testSource(id = "relative")
        val safSource = testSource(id = "saf", binContentUri = "content://good-bin")

        assertEquals(mergedBin.absolutePath, resolveCdPreviewLocalBinPath(filesDir, mergedSource))
        assertEquals(File(filesDir, "disc.bin").absolutePath, resolveCdPreviewLocalBinPath(filesDir, relativeSource))
        assertNull(resolveCdPreviewLocalBinPath(filesDir, safSource))
    }

    @Test
    fun resolvesPlaylistCuePathToAbsoluteLocalCueForMergedSources() {
        val filesDir = File("build/test-playlist-cue").absoluteFile
        filesDir.mkdirs()
        val localCue = File(filesDir, "disc.cue")
        localCue.writeText("FILE \"disc.bin\" BINARY\n")
        val mergedSource = testSource(id = "merged", binContentUri = File(filesDir, "merged.bin").absolutePath)

        assertEquals(localCue.absolutePath, resolvePlaylistCuePath(filesDir, mergedSource) { "fallback.cue" })
    }

    @Test
    fun fallsBackToStagedCuePathForSafPlaylistSources() {
        val filesDir = File("build/test-playlist-cue-fallback").absoluteFile
        filesDir.mkdirs()
        val safSource = testSource(id = "saf", binContentUri = "content://good-bin")

        assertEquals("fallback.cue", resolvePlaylistCuePath(filesDir, safSource) { "fallback.cue" })
    }

    @Test
    fun reservesSafEntriesForActualSafBackedCdSources() {
        val localSource = testSource(id = "local")
        val mergedLocalSource = testSource(id = "merged", binContentUri = File("/tmp/merged.bin").absolutePath)
        val safSource = testSource(id = "saf", binContentUri = "content://good-bin", cueContentUri = "content://good-cue")

        assertEquals(false, hasSafLinkedCdContent(localSource))
        assertEquals(false, hasSafLinkedCdContent(mergedLocalSource))
        assertEquals(true, hasSafLinkedCdContent(safSource))
    }

    private fun testSource(
        id: String,
        binContentUri: String? = null,
        cueContentUri: String? = null,
    ) =
        AudioSourceManager.AudioSource(
            id = id,
            cuePath = "disc.cue",
            binPaths = listOf("disc.bin"),
            discLabel = id,
            discId = "unknown",
            trackCount = 10,
            audioTrackCount = 9,
            legacyDiscId = 0L,
            binContentUri = binContentUri,
            cueContentUri = cueContentUri,
        )
}