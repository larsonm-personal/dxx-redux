package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

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
}