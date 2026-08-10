package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test
import java.io.File
import kotlin.io.path.createTempDirectory

class CdAudioSourceNormalizationTest {
    @Test
    fun normalizesTrackOffsetsAcrossMultipleBinFiles() {
        val tracks =
            listOf(
                DiscImportBridge.CueTrack(1, 0, 0, 0, 100, "Data"),
                DiscImportBridge.CueTrack(2, 1, 1, 0, 75, "Track 2"),
                DiscImportBridge.CueTrack(3, 1, 2, 0, 50, "Track 3"),
            )

        val normalized = normalizeCueTracksForMergedBin(tracks, listOf(100L * 2352L, 75L * 2352L, 50L * 2352L))

        assertEquals(listOf(0, 100, 175), normalized.map { it.startSector })
        assertEquals(listOf(0, 0, 0), normalized.map { it.fileIndex })
    }

    @Test
    fun buildsMergedCueTextWithSingleBinaryFile() {
        val tracks =
            listOf(
                DiscImportBridge.CueTrack(1, 0, 0, 0, 100, "Data"),
                DiscImportBridge.CueTrack(2, 1, 0, 100, 75, "A \"Song\""),
            )

        val cueText = buildMergedCueText("merged.bin", tracks)

        assertEquals(
            "REM DXX-REDUX GENERATED MERGED LOCAL SOURCE\n" +
                "FILE \"merged.bin\" BINARY\n" +
                "  TRACK 01 MODE1/2352\n" +
                "    TITLE \"Data\"\n" +
                "    INDEX 01 00:00:00\n" +
                "  TRACK 02 AUDIO\n" +
                "    TITLE \"A 'Song'\"\n" +
                "    INDEX 01 00:01:25\n",
            cueText,
        )
    }

    @Test
    fun detectsGeneratedMergedArtifactsOnlyFromCueMarker() {
        val tempDir = createTempDirectory("merged-cue-test").toFile()
        val markedCue = File(tempDir, "known-disc.cue")
        markedCue.writeText("$GENERATED_MERGED_CUE_MARKER\nFILE \"known-disc.bin\" BINARY\n")
        val markedBin = File(tempDir, "known-disc.bin")
        markedBin.writeText("test")

        val legacyCue = File(tempDir, "custom-123.cue")
        legacyCue.writeText("FILE \"custom-123.bin\" BINARY\n")
        val legacyBin = File(tempDir, "custom-123.bin")
        legacyBin.writeText("test")

        assertEquals(true, isGeneratedMergedCueFile(markedCue))
        assertEquals(true, isGeneratedMergedStorageArtifact(markedBin))
        assertEquals(false, isGeneratedMergedCueFile(legacyCue))
        assertEquals(false, isGeneratedMergedStorageArtifact(legacyBin))
        assertEquals(true, isLegacyGeneratedMergedStorageArtifact(legacyCue))
        assertEquals(true, isLegacyGeneratedMergedStorageArtifact(legacyBin))
    }

    @Test
    fun choosesCueBasedCdAudioImportStemAndAvoidsCollisions() {
        assertEquals("infinite_abyss_disc_1", sanitizeCdAudioImportStem("Infinite Abyss Disc 1"))
        assertEquals(
            "infinite_abyss_disc_1-2",
            chooseUniqueCdAudioImportStem(
                preferredStem = "Infinite Abyss Disc 1",
                existingFileNames = setOf("infinite_abyss_disc_1.bin"),
            ),
        )
        assertEquals(
            "cd_audio_source",
            chooseUniqueCdAudioImportStem(
                preferredStem = "   ???   ",
                existingFileNames = emptySet(),
            ),
        )
    }
}
