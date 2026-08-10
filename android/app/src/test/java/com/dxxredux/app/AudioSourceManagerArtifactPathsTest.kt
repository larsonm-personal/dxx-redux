package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import kotlin.io.path.createTempDirectory

class AudioSourceManagerArtifactPathsTest {
    @Test
    fun reportsManagedInternalArtifactPathsForMergedLocalSource() {
        val filesDir = createTempDirectory("test-audiosrc-artifacts").toFile()
        File(filesDir, "custom-123.cue").writeText("FILE \"custom-123.bin\" BINARY\n")
        File(filesDir, "custom-123.bin").writeText("test")
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
        val testRoot = createTempDirectory("test-audiosrc-artifacts-import-root").toFile()
        val filesDir = File(testRoot, "files").also { it.mkdirs() }
        val importRoot = File(testRoot, "external-import")
        ImportLocationManager(filesDir).setOverride(importRoot)
        val cdAudioDir = File(importRoot, GENERATED_CD_AUDIO_ARTIFACT_DIR).also { it.mkdirs() }
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
        val testRoot = createTempDirectory("test-audiosrc-artifacts-ext").toFile()
        val filesDir = File(testRoot, "files").also { it.mkdirs() }
        File(filesDir, "custom-456.cue").writeText("FILE \"track.bin\" BINARY\n")
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
    fun preservesExternalCustomNamedPairOnRemove() {
        val testRoot = createTempDirectory("test-audiosrc-remove-ext").toFile()
        val filesDir = File(testRoot, "files").also { it.mkdirs() }
        val externalDir = File(testRoot, "external").also { it.mkdirs() }
        val internalCue = File(filesDir, "custom-disc.cue").apply { writeText("copied cue") }
        val externalCue = File(externalDir, "custom-disc.cue").apply { writeText("external cue") }
        val externalBin = File(externalDir, "custom-disc.bin").apply { writeText("external bin") }
        val manager = AudioSourceManager(filesDir)
        manager.installTestSource(externalCustomSource(internalCue.name, externalBin))

        manager.removeSource("external-custom") { _, _ -> }

        assertFalse(internalCue.exists())
        assertEquals("external cue", externalCue.readText())
        assertEquals("external bin", externalBin.readText())
        assertTrue(manager.getSources().isEmpty())
    }

    @Test
    fun preservesExternalMarkedPairOnClearAll() {
        val testRoot = createTempDirectory("test-audiosrc-clear-ext").toFile()
        val filesDir = File(testRoot, "files").also { it.mkdirs() }
        val externalDir = File(testRoot, "external").also { it.mkdirs() }
        val externalCue =
            File(externalDir, "forged.cue").apply {
                writeText("$GENERATED_MERGED_CUE_MARKER\nFILE \"forged.bin\" BINARY\n")
            }
        val externalBin = File(externalDir, "forged.bin").apply { writeText("external bin") }
        val manager = AudioSourceManager(filesDir)
        manager.installTestSource(externalCustomSource(externalCue.absolutePath, externalBin))

        manager.clearAll()

        assertTrue(externalCue.exists())
        assertEquals("external bin", externalBin.readText())
        assertTrue(manager.getSources().isEmpty())
    }

    @Test
    fun removesArtifactsFromDedicatedManagedRoot() {
        val testRoot = createTempDirectory("test-audiosrc-remove-owned").toFile()
        val filesDir = File(testRoot, "files").also { it.mkdirs() }
        val importRoot = File(testRoot, "external-import")
        ImportLocationManager(filesDir).setOverride(importRoot)
        val artifactDir = File(importRoot, GENERATED_CD_AUDIO_ARTIFACT_DIR).also { it.mkdirs() }
        val cueFile =
            File(artifactDir, "owned.cue").apply {
                writeText("$GENERATED_MERGED_CUE_MARKER\nFILE \"owned.bin\" BINARY\n")
            }
        val binFile = File(artifactDir, "owned.bin").apply { writeText("owned bin") }
        val manager = AudioSourceManager(filesDir)
        manager.installTestSource(externalCustomSource(cueFile.absolutePath, binFile))

        manager.removeSource("external-custom") { _, _ -> }

        assertFalse(cueFile.exists())
        assertFalse(binFile.exists())
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

    private fun externalCustomSource(
        cuePath: String,
        binFile: File,
    ) =
        AudioSourceManager.AudioSource(
            id = "external-custom",
            cuePath = cuePath,
            binPaths = listOf(binFile.name),
            discLabel = "External custom",
            discId = "unknown",
            trackCount = 2,
            audioTrackCount = 1,
            legacyDiscId = 0L,
            binContentUri = binFile.absolutePath,
        )

    private fun AudioSourceManager.installTestSource(source: AudioSourceManager.AudioSource) {
        AudioSourceManager::class.java.getDeclaredField("sources").apply {
            isAccessible = true
            set(this@installTestSource, mutableListOf(source))
        }
    }
}
