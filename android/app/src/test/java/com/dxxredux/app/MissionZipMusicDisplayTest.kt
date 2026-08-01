package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class MissionZipMusicDisplayTest {
    @Test
    fun decodedNamePrefersBundledMatch() {
        val entry =
            entry(
                localName = "Bundled Track",
                acoustIdName = "Web Track",
            )

        assertEquals("Bundled Track", missionZipMusicDecodedName(entry))
    }

    @Test
    fun decodedNameFallsBackToAcoustIdMatch() {
        val entry =
            entry(
                localName = null,
                acoustIdName = "Web Track",
            )

        assertEquals("Web Track", missionZipMusicDecodedName(entry))
    }

    @Test
    fun decodedNameIgnoresPlaceholder() {
        assertNull(
            missionZipMusicDecodedName(
                entry(
                    localName = "[unknown] - [untitled]",
                    acoustIdName = "[unknown] - [untitled]",
                ),
            ),
        )
    }

    private fun entry(
        localName: String?,
        acoustIdName: String?,
    ): MissionZipAudioFingerprintCache.Entry =
        MissionZipAudioFingerprintCache.Entry(
            archiveName = "mission.zip",
            archiveSize = 1L,
            archiveMtime = 2L,
            sourceIdentity = "source-a",
            trackId = "track-a",
            entryPath = "game01.ogg",
            nestedPath = "",
            hogEntryName = "",
            contentSha256 = "abc",
            durationMs = 1000,
            chromaprint = "fingerprint",
            localMatchName = localName,
            localMatchConfidence = null,
            localMatchDiscId = null,
            localMatchTrack = null,
            localMatchDbIdentity = null,
            acoustIdName = acoustIdName,
            acoustIdLookupStatus = null,
            acoustIdLookupAt = null,
            lookupAt = 3L,
        )
}
