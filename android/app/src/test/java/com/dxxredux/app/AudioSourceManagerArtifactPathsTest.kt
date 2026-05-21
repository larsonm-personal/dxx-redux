package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test
import java.io.File

class AudioSourceManagerArtifactPathsTest {
    @Test
    fun reportsManagedInternalArtifactPathsForMergedLocalSource() {
        val filesDir = File("build/test-audiosrc-artifacts").absoluteFile
        val sources =
            listOf(
                AudioSourceManager.AudioSource(
                    id = "custom-123",
                    cuePath = "custom-123.cue",
                    binPaths = listOf("custom-123.bin"),
                    discLabel = "Test",
                    discId = "unknown",
                    trackCount = 2,
                    audioTrackCount = 1,
                    legacyDiscId = 0L,
                    binContentUri = File(filesDir, "custom-123.bin").absolutePath,
                ),
            )

        assertEquals(
            setOf(
                File(filesDir, "custom-123.cue").absolutePath,
                File(filesDir, "custom-123.bin").absolutePath,
            ),
            getManagedInternalArtifactPaths(filesDir, sources),
        )
    }

    @Test
    fun reportsGeneratedMergedArtifactPathsOutsideFilesDir() {
        val testRoot = File("build/test-audiosrc-artifacts-import-root").absoluteFile
        val filesDir = File(testRoot, "files").also { it.mkdirs() }
        val cdAudioDir = File(testRoot, "external-import/cd_audio").also { it.mkdirs() }
        val cueFile = File(cdAudioDir, "disc.cue")
        val binFile = File(cdAudioDir, "disc.bin")
        cueFile.writeText("$GENERATED_MERGED_CUE_MARKER\nFILE \"disc.bin\" BINARY\n")
        binFile.writeText("test")
        val sources =
            listOf(
                AudioSourceManager.AudioSource(
                    id = "generated-absolute",
                    cuePath = cueFile.absolutePath,
                    binPaths = listOf(binFile.absolutePath),
                    discLabel = "Generated",
                    discId = "unknown",
                    trackCount = 2,
                    audioTrackCount = 1,
                    legacyDiscId = 0L,
                    binContentUri = binFile.absolutePath,
                ),
            )

        assertEquals(
            setOf(cueFile.absolutePath, binFile.absolutePath),
            getManagedInternalArtifactPaths(filesDir, sources),
        )
    }

    @Test
    fun excludesExternalAbsoluteBinPathFromManagedArtifacts() {
        val filesDir = File("build/test-audiosrc-artifacts-ext").absoluteFile
        val sources =
            listOf(
                AudioSourceManager.AudioSource(
                    id = "custom-456",
                    cuePath = "custom-456.cue",
                    binPaths = listOf("track.bin"),
                    discLabel = "External",
                    discId = "unknown",
                    trackCount = 2,
                    audioTrackCount = 1,
                    legacyDiscId = 0L,
                    binContentUri = File(filesDir.parentFile, "external.bin").absolutePath,
                ),
            )

        assertEquals(
            setOf(File(filesDir, "custom-456.cue").absolutePath),
            getManagedInternalArtifactPaths(filesDir, sources),
        )
    }

    @Test
    fun reportsOnlySafHelperArtifactsForSafBackedSources() {
        val filesDir = File("build/test-audiosrc-helper-artifacts").absoluteFile
        val sources =
            listOf(
                AudioSourceManager.AudioSource(
                    id = "saf",
                    cuePath = "saf_disc.cue",
                    binPaths = listOf("ignored.bin"),
                    discLabel = "Saf",
                    discId = "unknown",
                    trackCount = 2,
                    audioTrackCount = 1,
                    legacyDiscId = 0L,
                    binContentUri = "content://good-bin",
                    cueContentUri = "content://good-cue",
                ),
                AudioSourceManager.AudioSource(
                    id = "merged",
                    cuePath = "merged_disc.cue",
                    binPaths = listOf("merged_disc.bin"),
                    discLabel = "Merged",
                    discId = "unknown",
                    trackCount = 2,
                    audioTrackCount = 1,
                    legacyDiscId = 0L,
                    binContentUri = File(filesDir, "merged_disc.bin").absolutePath,
                ),
            )

        assertEquals(
            setOf(File(filesDir, "saf_disc.cue").absolutePath),
            getSafLinkedHelperArtifactPaths(filesDir, sources),
        )
    }
}