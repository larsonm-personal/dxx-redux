package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class MissionZipMidiMetadataResolutionTest {
    @Test
    fun resolvesOnlyAnExactUniqueMidPeerInTheSameSource() {
        val hmp = track("hmp", "music/GAME05.HMP", "hmp")
        val mid = track("mid", "music/game05.mid", "mid")
        val crossSource = track("other", "music/game05.mid", "mid")
        val catalog =
            MissionZipMusicCatalog(
                archivePath = "obsidian.zip",
                sources =
                    listOf(
                        MissionZipMusicSource("hog", "HOG", "obsidian.hog", listOf(hmp, mid)),
                        MissionZipMusicSource("other", "Other", "other.hog", listOf(crossSource)),
                    ),
                sourceIdentity = "obsidian",
            )

        assertEquals(mid, MissionZipMusic.midiMetadataPeer(catalog, hmp))
        assertNull(MissionZipMusic.midiMetadataPeer(catalog, hmp.copy(extension = "hmq")))
        assertNull(MissionZipMusic.midiMetadataPeer(catalog, hmp.copy(id = "missing")))
    }

    @Test
    fun rejectsDifferentDirectoriesAndAmbiguousPeers() {
        val hmp = track("hmp", "a/game05.hmp", "hmp")
        val wrongDirectory = track("wrong", "b/game05.mid", "mid")
        val duplicateOne = track("mid-1", "a/game05.mid", "mid")
        val duplicateTwo = track("mid-2", "a/GAME05.MID", "mid")

        assertNull(MissionZipMusic.midiMetadataPeer(catalog(hmp, wrongDirectory), hmp))
        assertNull(MissionZipMusic.midiMetadataPeer(catalog(hmp, duplicateOne, duplicateTwo), hmp))
    }

    private fun catalog(vararg tracks: MissionZipMusicTrack) =
        MissionZipMusicCatalog(
            archivePath = "mission.zip",
            sources = listOf(MissionZipMusicSource("source", "Source", "mission.hog", tracks.toList())),
            sourceIdentity = "source",
        )

    private fun track(
        id: String,
        name: String,
        extension: String,
    ) = MissionZipMusicTrack(
        id = id,
        displayName = name,
        sourceRelativeName = name,
        archiveEntryPath = "mission.hog",
        hogEntryName = name.substringAfterLast('/'),
        kind = MissionZipMusic.KIND_MIDI,
        extension = extension,
        sizeBytes = 1,
        playable = true,
    )
}
