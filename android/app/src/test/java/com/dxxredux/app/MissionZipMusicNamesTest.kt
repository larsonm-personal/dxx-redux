package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import java.io.IOException

class MissionZipMusicNamesTest {
    @Test
    fun writesDecodedNamesForEngineLookupKeys() {
        val output = testFile("names/mission_music_names.json")
        val track =
            MissionZipMusicTrack(
                id = "track-a",
                displayName = "music/game01.ogg",
                archiveEntryPath = "music/game01.ogg",
                kind = MissionZipMusic.KIND_COMPRESSED_AUDIO,
                extension = "ogg",
                sizeBytes = 12,
                playable = true,
            )
        val ignored =
            track.copy(
                id = "track-b",
                displayName = "game02.ogg",
                archiveEntryPath = "game02.ogg",
            )
        val catalog =
            MissionZipMusicCatalog(
                archivePath = "mission.zip",
                sources = listOf(MissionZipMusicSource("archive", "Archive music", "", listOf(track, ignored))),
                sourceIdentity = "source-a",
            )

        val count =
            MissionZipMusicNames.writeSidecar(
                output,
                catalog,
                mapOf(
                    track.id to entry(track, localName = "Decoded Mission Track"),
                    ignored.id to entry(ignored, localName = "[unknown] - [untitled]"),
                ),
            )

        val root = JSONObject(output.readText())
        assertEquals(1, count)
        assertEquals(MusicNameSidecar.VERSION, root.getInt("version"))
        val record = root.getJSONArray("records").getJSONObject(0)
        assertEquals("Decoded Mission Track", record.getString("name"))
        assertEquals(listOf("music/game01.ogg"), record.getJSONArray("paths").strings())
        assertEquals(listOf("game01.ogg"), record.getJSONArray("aliases").strings())
        assertTrue(MissionZipMusicNames.isCurrent(output, catalog))
        assertFalse(MissionZipMusicNames.isCurrent(output, catalog.copy(sourceIdentity = "source-b")))
    }

    @Test
    fun deletesEmptySidecarWhenNoNamesAreKnown() {
        val output =
            testFile("empty/mission_music_names.json").also {
                it.parentFile?.mkdirs()
                it.writeText("""{"game01.ogg":"Old"}""")
            }
        val catalog =
            MissionZipMusicCatalog(
                archivePath = "mission.zip",
                sources =
                    listOf(
                        MissionZipMusicSource(
                            "archive",
                            "Archive music",
                            "",
                            listOf(
                                MissionZipMusicTrack(
                                    id = "track-a",
                                    displayName = "game01.ogg",
                                    archiveEntryPath = "game01.ogg",
                                    kind = MissionZipMusic.KIND_COMPRESSED_AUDIO,
                                    extension = "ogg",
                                    sizeBytes = 12,
                                    playable = true,
                                ),
                            ),
                        ),
                    ),
                sourceIdentity = "source-a",
            )

        val count = MissionZipMusicNames.writeSidecar(output, catalog, emptyMap())

        assertEquals(0, count)
        assertFalse(output.exists())
    }

    @Test
    fun publicationFailurePreservesThePreviousCompleteSidecar() {
        val output = testFile("failure/mission_music_names.json")
        val track =
            MissionZipMusicTrack(
                id = "track-a",
                displayName = "game01.ogg",
                archiveEntryPath = "game01.ogg",
                kind = MissionZipMusic.KIND_COMPRESSED_AUDIO,
                extension = "ogg",
                sizeBytes = 12,
                playable = true,
            )
        val catalog =
            MissionZipMusicCatalog(
                "mission.zip",
                listOf(MissionZipMusicSource("archive", "Archive music", "", listOf(track))),
                "source-a",
            )
        MissionZipMusicNames.writeSidecar(output, catalog, mapOf(track.id to entry(track, "First Name")))
        val original = output.readText()

        val failure =
            runCatching {
                MissionZipMusicNames.writeSidecar(
                    output,
                    catalog,
                    mapOf(track.id to entry(track, "Replacement Name")),
                ) { _, _ -> throw IOException("injected publication failure") }
            }.exceptionOrNull()

        assertTrue(failure is IOException)
        assertEquals(original, output.readText())
        assertTrue(MissionZipMusicNames.isCurrent(output, catalog))
        assertEquals(
            "First Name",
            JSONObject(output.readText()).getJSONArray("records").getJSONObject(0).getString("name"),
        )
        assertTrue(
            output.parentFile!!
                .listFiles()
                .orEmpty()
                .none { it.name.endsWith(".tmp") },
        )
    }

    private fun entry(
        track: MissionZipMusicTrack,
        localName: String?,
    ): MissionZipAudioFingerprintCache.Entry =
        MissionZipAudioFingerprintCache.Entry(
            archiveName = "mission.zip",
            archiveSize = 1L,
            archiveMtime = 2L,
            sourceIdentity = "source-a",
            trackId = track.id,
            entryPath = track.archiveEntryPath,
            nestedPath = track.nestedEntryPath.orEmpty(),
            hogEntryName = track.hogEntryName.orEmpty(),
            contentSha256 = "abc",
            durationMs = 1000,
            chromaprint = "fingerprint",
            localMatchName = localName,
            localMatchConfidence = 0.9f,
            localMatchDiscId = "disc",
            localMatchTrack = 1,
            localMatchDbIdentity = "db",
            acoustIdName = null,
            acoustIdLookupStatus = null,
            acoustIdLookupAt = null,
            lookupAt = 3L,
        )

    private fun testFile(path: String): File =
        File("build/test-mission-zip-music-names/$path")
            .absoluteFile
            .also {
                it.parentFile?.deleteRecursively()
                it.parentFile?.mkdirs()
            }

    private fun org.json.JSONArray.strings(): List<String> = (0 until length()).map(::getString)
}
