package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class MissionZipMusicAnalysisSkipTest {
    @Test
    fun metadataAnalysisSkipsTracksAlreadyCachedAtImport() {
        val cached = track("game01.ogg")
        val missing = track("game02.ogg")

        val pending =
            missionZipMusicTracksNeedingLocalAnalysis(
                listOf(cached, missing),
                mapOf(cached.id to entry(cached)),
            )

        assertEquals(listOf(missing), pending)
    }

    @Test
    fun metadataAnalysisDoesNothingWhenAllTracksAreCached() {
        val tracks = listOf(track("game01.ogg"), track("game02.ogg"))
        val cached = tracks.associate { it.id to entry(it) }

        assertEquals(emptyList<MissionZipMusicTrack>(), missionZipMusicTracksNeedingLocalAnalysis(tracks, cached))
    }

    private fun track(name: String): MissionZipMusicTrack =
        MissionZipMusicTrack(
            id = "archive:$name",
            displayName = name,
            archiveEntryPath = name,
            kind = MissionZipMusic.KIND_COMPRESSED_AUDIO,
            extension = "ogg",
            sizeBytes = 10L,
            playable = true,
        )

    private fun entry(track: MissionZipMusicTrack): MissionZipAudioFingerprintCache.Entry =
        MissionZipAudioFingerprintCache.Entry(
            archiveName = "mission.zip",
            archiveSize = 1L,
            archiveMtime = 2L,
            trackId = track.id,
            entryPath = track.archiveEntryPath,
            nestedPath = track.nestedEntryPath.orEmpty(),
            hogEntryName = track.hogEntryName.orEmpty(),
            contentSha256 = "abc",
            durationMs = 1000,
            chromaprint = "fingerprint",
            localMatchName = "Known Track",
            localMatchConfidence = 0.9f,
            localMatchDiscId = "disc",
            localMatchTrack = 1,
            localMatchDbIdentity = "db",
            acoustIdName = null,
            acoustIdLookupStatus = null,
            acoustIdLookupAt = null,
            lookupAt = 3L,
        )
}
