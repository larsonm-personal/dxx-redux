package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.security.MessageDigest

class MissionZipAudioFingerprintCacheTest {
    @Test
    fun cachedResultSurvivesArchivePathChange() {
        val filesDir = testDir("path-change/files")
        val archiveA = testArchive("path-change/a/mission.zip", byteArrayOf(1, 2, 3))
        val archiveB = testArchive("path-change/b/mission.zip", byteArrayOf(1, 2, 3))
        val fixedMtime = 1_781_012_345_000L
        archiveA.setLastModified(fixedMtime)
        archiveB.setLastModified(fixedMtime)
        val track = testTrack("game01.ogg")
        val staged = testArchive("path-change/staged/game01.ogg", byteArrayOf(9, 8, 7))

        val cache = MissionZipAudioFingerprintCache(filesDir)
        cache.record(
            testCatalog(archiveA, track),
            track,
            staged,
            FingerprintBridge.FingerprintResult("fp-a", 1234),
            FingerprintBridge.MatchResult(0.82f, "Known Track", "disc-a", 7),
        )

        val cached = cache.cachedEntries(testCatalog(archiveB, track))[track.id]

        assertNotNull(cached)
        assertEquals("Known Track", cached!!.localMatchName)
        assertEquals(7, cached.localMatchTrack)
    }

    @Test
    fun cacheInvalidatesWhenArchiveIdentityChanges() {
        val filesDir = testDir("archive-change/files")
        val archive = testArchive("archive-change/mission.zip", byteArrayOf(1, 2, 3))
        val track = testTrack("game01.ogg")
        val staged = testArchive("archive-change/staged/game01.ogg", byteArrayOf(9, 8, 7))
        val cache = MissionZipAudioFingerprintCache(filesDir)
        cache.record(
            testCatalog(archive, track),
            track,
            staged,
            FingerprintBridge.FingerprintResult("fp-a", 1234),
            null,
        )

        archive.appendBytes(byteArrayOf(4))

        assertTrue(cache.cachedEntries(testCatalog(archive, track)).isEmpty())
    }

    @Test
    fun exactLookupRequiresMatchingContentHash() {
        val filesDir = testDir("content-hash/files")
        val archive = testArchive("content-hash/mission.zip", byteArrayOf(1, 2, 3))
        val track = testTrack("game01.ogg")
        val staged = testArchive("content-hash/staged/game01.ogg", byteArrayOf(9, 8, 7))
        val cache = MissionZipAudioFingerprintCache(filesDir)
        cache.record(
            testCatalog(archive, track),
            track,
            staged,
            FingerprintBridge.FingerprintResult("fp-a", 1234),
            null,
        )

        assertNotNull(cache.get(testCatalog(archive, track), track, sha256(staged)))
        assertNull(cache.get(testCatalog(archive, track), track, "bad-hash"))
    }

    @Test
    fun writesPrettySortedJson() {
        val filesDir = testDir("pretty/files")
        val archive = testArchive("pretty/mission.zip", byteArrayOf(1, 2, 3))
        val cache = MissionZipAudioFingerprintCache(filesDir)
        val trackB = testTrack("game02.ogg")
        val trackA = testTrack("game01.ogg")
        val staged = testArchive("pretty/staged/game.ogg", byteArrayOf(9, 8, 7))
        cache.record(testCatalog(archive, trackB), trackB, staged, FingerprintBridge.FingerprintResult("fp-b", 2), null)
        cache.record(testCatalog(archive, trackA), trackA, staged, FingerprintBridge.FingerprintResult("fp-a", 1), null)

        val text = File(filesDir, "mission_zip_audio_fingerprints.json").readText()

        assertTrue(text.contains("\n  \"schema\""))
        assertTrue(text.indexOf("game01.ogg") < text.indexOf("game02.ogg"))
    }

    @Test
    fun recordsAcoustIdResultOnExistingFingerprintEntry() {
        val filesDir = testDir("acoustid/files")
        val archive = testArchive("acoustid/mission.zip", byteArrayOf(1, 2, 3))
        val track = testTrack("game01.ogg")
        val staged = testArchive("acoustid/staged/game01.ogg", byteArrayOf(9, 8, 7))
        val cache = MissionZipAudioFingerprintCache(filesDir)
        val local =
            cache.record(
                testCatalog(archive, track),
                track,
                staged,
                FingerprintBridge.FingerprintResult("fp-a", 1234),
                FingerprintBridge.MatchResult(0.82f, "Known Track", "disc-a", 7),
            )

        val updated =
            cache.recordAcoustIdResult(
                local,
                "Web Track",
                MissionZipAudioFingerprintCache.ACOUSTID_STATUS_OK,
                0.93,
                "recording-123",
            )
        val reloaded = cache.cachedEntries(testCatalog(archive, track))[track.id]

        assertEquals("Known Track", updated.localMatchName)
        assertEquals("Web Track", reloaded!!.acoustIdName)
        assertEquals(0.93, reloaded.acoustIdScore)
        assertEquals("recording-123", reloaded.acoustIdRecordingId)
        assertEquals(MissionZipAudioFingerprintCache.ACOUSTID_STATUS_OK, reloaded.acoustIdLookupStatus)
        assertTrue(File(filesDir, "mission_zip_audio_fingerprints.json").readText().contains("\"acoustid_name\""))
    }

    @Test
    fun onlyAuthoritativeAcoustIdResultsSuppressExplicitRetry() {
        val filesDir = testDir("acoustid-retry/files")
        val archive = testArchive("acoustid-retry/mission.zip", byteArrayOf(1, 2, 3))
        val track = testTrack("game01.ogg")
        val staged = testArchive("acoustid-retry/staged/game01.ogg", byteArrayOf(9, 8, 7))
        val cache = MissionZipAudioFingerprintCache(filesDir)
        val local =
            cache.record(
                testCatalog(archive, track),
                track,
                staged,
                FingerprintBridge.FingerprintResult("fp-a", 1234),
                null,
            )

        val retryable = cache.recordAcoustIdResult(local, AcoustIdLookupResult.RetryableFailure("offline"))
        assertEquals(MissionZipAudioFingerprintCache.ACOUSTID_STATUS_RETRYABLE_FAILURE, retryable.acoustIdLookupStatus)
        assertTrue(!retryable.hasAuthoritativeAcoustIdLookup)

        val configuration = cache.recordAcoustIdResult(local, AcoustIdLookupResult.ConfigurationFailure("bad key"))
        assertEquals(
            MissionZipAudioFingerprintCache.ACOUSTID_STATUS_CONFIGURATION_FAILURE,
            configuration.acoustIdLookupStatus,
        )
        assertTrue(!configuration.hasAuthoritativeAcoustIdLookup)

        val legacyFailure =
            cache.recordAcoustIdResult(local, null, MissionZipAudioFingerprintCache.ACOUSTID_STATUS_FAILED)
        assertTrue(!legacyFailure.hasAuthoritativeAcoustIdLookup)

        val noMatch = cache.recordAcoustIdResult(local, AcoustIdLookupResult.NoMatch)
        assertTrue(noMatch.hasAuthoritativeAcoustIdLookup)
        assertNotNull(noMatch.acoustIdLookupAt)

        val match =
            cache.recordAcoustIdResult(
                local,
                AcoustIdLookupResult.Match(
                    AcoustIdLabelMatch("Artist - game01", null, 0.95, "recording-1"),
                ),
            )
        assertTrue(match.hasAuthoritativeAcoustIdLookup)
        assertEquals("Artist - game01", match.acoustIdName)
    }

    @Test
    fun localMatchRefreshPreservesWebLookupResult() {
        val filesDir = testDir("local-refresh/files")
        val archive = testArchive("local-refresh/mission.zip", byteArrayOf(1, 2, 3))
        val track = testTrack("game01.ogg")
        val staged = testArchive("local-refresh/staged/game01.ogg", byteArrayOf(9, 8, 7))
        val cache = MissionZipAudioFingerprintCache(filesDir)
        val local =
            cache.record(
                testCatalog(archive, track),
                track,
                staged,
                FingerprintBridge.FingerprintResult("fp-a", 1234),
                null,
                "old-db",
            )
        val web =
            cache.recordAcoustIdResult(
                local,
                "Web Track",
                MissionZipAudioFingerprintCache.ACOUSTID_STATUS_OK,
            )

        val updated =
            cache.recordLocalMatchResult(
                web,
                FingerprintBridge.MatchResult(0.91f, "Bundled Track", "disc-b", 2),
                "new-db",
            )
        val reloaded = cache.cachedEntries(testCatalog(archive, track))[track.id]

        assertEquals("Bundled Track", updated.localMatchName)
        assertEquals("new-db", reloaded!!.localMatchDbIdentity)
        assertEquals("Web Track", reloaded.acoustIdName)
        assertEquals(MissionZipAudioFingerprintCache.ACOUSTID_STATUS_OK, reloaded.acoustIdLookupStatus)
        assertEquals(2, reloaded.localMatchTrack)
    }

    private fun testCatalog(
        archive: File,
        track: MissionZipMusicTrack,
    ): MissionZipMusicCatalog =
        MissionZipMusicCatalog(
            archive.absolutePath,
            listOf(MissionZipMusicSource("hog:${archive.name}", "${archive.name} music", archive.name, listOf(track))),
        )

    private fun testTrack(name: String): MissionZipMusicTrack =
        MissionZipMusicTrack(
            id = "hog:mission.zip:mission.hog:$name",
            displayName = name,
            archiveEntryPath = "mission.hog",
            hogEntryName = name,
            kind = MissionZipMusic.KIND_COMPRESSED_AUDIO,
            extension = "ogg",
            sizeBytes = 3,
            playable = true,
        )

    private fun testDir(path: String): File =
        File("build/test-mission-zip-fingerprint-cache/$path")
            .absoluteFile
            .also {
                it.deleteRecursively()
                it.mkdirs()
            }

    private fun testArchive(
        path: String,
        bytes: ByteArray,
    ): File =
        File("build/test-mission-zip-fingerprint-cache/$path")
            .absoluteFile
            .also {
                it.parentFile?.mkdirs()
                it.writeBytes(bytes)
            }

    private fun sha256(file: File): String =
        MessageDigest
            .getInstance("SHA-256")
            .digest(file.readBytes())
            .joinToString("") { "%02x".format(it) }
}
