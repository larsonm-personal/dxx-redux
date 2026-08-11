package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class MusicNameSidecarTest {
    @Test
    fun preservesTypedCollidingAliasesAndEscapedUnicodeNames() {
        val text =
            MusicNameSidecar.encode(
                listOf(
                    MusicNameSidecar.Record(
                        listOf("music/a/game01.ogg"),
                        listOf("game01.ogg"),
                        "First\nSmile \u263a \ud83d\ude80",
                    ),
                    MusicNameSidecar.Record(
                        listOf("music/b/game01.ogg"),
                        listOf("game01.ogg"),
                        "Second",
                    ),
                ),
                "source-a",
            )

        val root = JSONObject(text)
        assertEquals(MusicNameSidecar.VERSION, root.getInt("version"))
        assertEquals("source-a", root.getString("sourceIdentity"))
        assertEquals(2, root.getJSONArray("records").length())
        assertTrue(text.toByteArray().size <= MusicNameSidecar.MAX_FILE_BYTES)
    }

    @Test
    fun rejectsDuplicateExactPathsAndBoundaryOverruns() {
        val duplicate =
            listOf(
                MusicNameSidecar.Record(listOf("music/A.ogg"), emptyList(), "First"),
                MusicNameSidecar.Record(listOf("MUSIC/a.ogg"), emptyList(), "Second"),
            )
        assertTrue(runCatching { MusicNameSidecar.encode(duplicate) }.isFailure)

        val accepted = MusicNameSidecar.Record(listOf("p".repeat(1024)), emptyList(), "n".repeat(512))
        MusicNameSidecar.encode(listOf(accepted))
        assertTrue(
            runCatching {
                MusicNameSidecar.encode(listOf(accepted.copy(paths = listOf("p".repeat(1025)))))
            }.isFailure,
        )
        assertTrue(
            runCatching {
                MusicNameSidecar.encode(listOf(accepted.copy(name = "n".repeat(513))))
            }.isFailure,
        )
    }

    @Test
    fun rejectsRecordCountAboveSharedLimit() {
        val records =
            (0..MusicNameSidecar.MAX_RECORDS).map { index ->
                MusicNameSidecar.Record(listOf("track-$index.ogg"), emptyList(), "Track $index")
            }
        assertTrue(runCatching { MusicNameSidecar.encode(records) }.isFailure)
    }
}
