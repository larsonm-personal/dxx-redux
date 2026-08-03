package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import java.io.File

class CdAudioSourceVisibilityTest {
    @Test
    fun keepsSparseNamesBoundToPhysicalCueTracks() {
        val source =
            testSource(
                id = "sparse-names",
                trackCount = 6,
                audioTrackNumbers = listOf(2, 4, 5, 6),
                trackNames = mapOf(2 to "First", 5 to "Third", 6 to "Fourth"),
            )

        assertEquals(
            listOf(
                CdPreviewTrack("First", 1, 2),
                CdPreviewTrack("Track 8", 2, 4),
                CdPreviewTrack("Third", 3, 5),
                CdPreviewTrack("Fourth", 4, 6),
            ),
            buildCdPreviewTracks(source, firstDisplayTrackNumber = 7),
        )
    }

    @Test
    fun hidesSourcesWithBrokenSafUris() {
        val localSource = testSource(id = "local")
        val localPathFile = File.createTempFile("disc", ".bin")
        localPathFile.deleteOnExit()
        val localPathSource = testSource(id = "local-path", binContentUri = localPathFile.absolutePath)
        val goodSafSource = testSource(id = "good", binContentUri = "content://good-bin", cueContentUri = "content://good-cue")
        val goodMultiSafSource =
            testSource(
                id = "good-multi",
                binContentUris = listOf("content://good-bin-1", "content://good-bin-2"),
                cueContentUri = "content://good-cue",
            )
        val brokenBinSource = testSource(id = "broken-bin", binContentUri = "content://broken-bin", cueContentUri = "content://good-cue")
        val brokenMultiBinSource =
            testSource(
                id = "broken-multi-bin",
                binContentUris = listOf("content://good-bin", "content://broken-bin"),
                cueContentUri = "content://good-cue",
            )
        val brokenCueSource = testSource(id = "broken-cue", binContentUri = "content://good-bin", cueContentUri = "content://broken-cue")

        val visibleIds =
            listOf(localSource, localPathSource, goodSafSource, goodMultiSafSource, brokenBinSource, brokenMultiBinSource, brokenCueSource)
                .filter { source ->
                    shouldDisplayCdAudioSource(source) { uri, _ -> !uri.contains("broken") }
                }.map { it.id }

        assertEquals(listOf("local", "local-path", "good", "good-multi"), visibleIds)
    }

    @Test
    fun resolvesLocalPreviewPathForMergedAndRelativeSources() {
        val filesDir = File("build/test-files")
        val mergedBin = File.createTempFile("merged-disc", ".bin")
        mergedBin.deleteOnExit()
        val mergedBinB = File.createTempFile("merged-disc-b", ".bin")
        mergedBinB.deleteOnExit()

        val mergedSource = testSource(id = "merged", binContentUri = mergedBin.absolutePath)
        val multiMergedSource =
            testSource(
                id = "multi-merged",
                binContentUris = listOf(mergedBin.absolutePath, mergedBinB.absolutePath),
                binPaths = listOf("disc-a.bin", "disc-b.bin"),
            )
        val relativeSource = testSource(id = "relative")
        val safSource = testSource(id = "saf", binContentUri = "content://good-bin")
        val multiRelativeSource = testSource(id = "multi-relative", binPaths = listOf("disc-a.bin", "disc-b.bin"))

        assertEquals(mergedBin.absolutePath, resolveCdPreviewLocalBinPath(filesDir, mergedSource))
        assertEquals(File(filesDir, "disc.bin").absolutePath, resolveCdPreviewLocalBinPath(filesDir, relativeSource))
        assertNull(resolveCdPreviewLocalBinPath(filesDir, safSource))
        assertNull(resolveCdPreviewLocalBinPath(filesDir, multiRelativeSource))
        assertEquals(
            listOf(mergedBin.absolutePath, mergedBinB.absolutePath),
            resolveCdPreviewLocalBinPaths(filesDir, multiMergedSource),
        )
        assertEquals(
            listOf(File(filesDir, "disc-a.bin").absolutePath, File(filesDir, "disc-b.bin").absolutePath),
            resolveCdPreviewLocalBinPaths(filesDir, multiRelativeSource),
        )
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
    fun resolvesPlaylistCuePathToAbsoluteGeneratedCueOutsideFilesDir() {
        val testRoot = File("build/test-playlist-cue-import-root").absoluteFile
        val filesDir = File(testRoot, "files").also { it.mkdirs() }
        val cdAudioDir = File(testRoot, "external-import/cd_audio").also { it.mkdirs() }
        val localCue = File(cdAudioDir, "disc.cue")
        val localBin = File(cdAudioDir, "disc.bin")
        localCue.writeText("$GENERATED_MERGED_CUE_MARKER\nFILE \"disc.bin\" BINARY\n")
        localBin.writeText("test")
        val mergedSource =
            testSource(
                id = "merged-import-root",
                binContentUri = localBin.absolutePath,
                cuePath = localCue.absolutePath,
            )

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
        val multiSafSource = testSource(id = "multi-saf", binContentUris = listOf("content://good-bin-1", "content://good-bin-2"))

        assertEquals(false, hasSafLinkedCdContent(localSource))
        assertEquals(false, hasSafLinkedCdContent(mergedLocalSource))
        assertEquals(true, hasSafLinkedCdContent(safSource))
        assertEquals(true, hasSafLinkedCdContent(multiSafSource))
    }

    private fun testSource(
        id: String,
        binContentUri: String? = null,
        binContentUris: List<String> = emptyList(),
        cueContentUri: String? = null,
        cuePath: String = "disc.cue",
        binPaths: List<String> = listOf("disc.bin"),
        trackCount: Int = 10,
        audioTrackNumbers: List<Int> = emptyList(),
        trackNames: Map<Int, String> = emptyMap(),
    ) =
        AudioSourceManager.AudioSource(
            id = id,
            cuePath = cuePath,
            binPaths = binPaths,
            discLabel = id,
            discId = "unknown",
            trackCount = trackCount,
            audioTrackCount = if (audioTrackNumbers.isEmpty()) 9 else audioTrackNumbers.size,
            audioTrackNumbers = audioTrackNumbers,
            legacyDiscId = 0L,
            trackNames = trackNames,
            binContentUri = binContentUri,
            binContentUris = binContentUris,
            cueContentUri = cueContentUri,
        )
}
