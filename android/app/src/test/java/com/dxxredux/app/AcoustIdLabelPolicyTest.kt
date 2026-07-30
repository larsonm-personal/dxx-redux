package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class AcoustIdLabelPolicyTest {
    private fun response(vararg results: String): JSONObject =
        JSONObject("""{"results":[${results.joinToString(",")}]}""")

    private fun result(
        score: Double,
        id: String,
        artist: String,
        title: String,
        album: String = "Album",
    ): String =
        """{"score":$score,"recordings":[{"id":"$id","title":"$title","artists":[{"name":"$artist"}],"releases":[{"title":"$album"}]}]}"""

    @Test
    fun acceptsCorroboratedHighConfidenceMatch() {
        val selected =
            AcoustIdLabelPolicy.select(
                response(result(0.95, "recording-1", "Dan Wentz", "Vampyro Briefing (Remastered)")),
                "01 Vampyro Briefing (Remastered).mp3",
            )

        assertEquals("Dan Wentz - Vampyro Briefing (Remastered)", selected?.name)
        assertEquals("recording-1", selected?.recordingId)
        assertEquals(0.95, selected?.score ?: 0.0, 0.0)
    }

    @Test
    fun rejectsCurrentVampyroFalsePositiveAndLowScores() {
        assertNull(
            AcoustIdLabelPolicy.select(
                response(result(0.99, "torche", "Torche", "Vampyro")),
                "01 Vampyro Briefing (Remastered).mp3",
            ),
        )
        assertNull(
            AcoustIdLabelPolicy.select(
                response(result(0.79, "low", "Dan Wentz", "Vampyro Briefing (Remastered)")),
                "01 Vampyro Briefing (Remastered).mp3",
            ),
        )
    }

    @Test
    fun selectionIsOrderIndependentAndRejectsAmbiguousArtists() {
        val first = result(0.95, "b", "Artist B", "Haunted")
        val second = result(0.95, "a", "Artist A", "Haunted")

        assertNull(AcoustIdLabelPolicy.select(response(first, second), "02 Haunted.ogg"))
        assertNull(AcoustIdLabelPolicy.select(response(second, first), "02 Haunted.ogg"))
    }

    @Test
    fun incompleteResultsCannotBecomeLabels() {
        val selected =
            AcoustIdLabelPolicy.select(
                response(
                    """{"score":0.99,"recordings":[{"title":"Haunted"}]}""",
                    """{"recordings":[{"id":"id","title":"Haunted"}]}""",
                    """{"score":0.99,"recordings":[{"id":"id"}]}""",
                ),
                "Haunted.flac",
            )

        assertNull(selected)
        assertTrue(AcoustIdLabelPolicy.labelsAgree("01 Haunted.flac", "Artist - Haunted"))
        assertTrue(AcoustIdLabelPolicy.labelsAgree("03 Haunted - Remix.flac", "Artist - Haunted - Remix"))
        assertNull(
            AcoustIdLabelPolicy.select(
                response(result(0.99, "wrong-remix", "Artist", "Other - Remix")),
                "03 Haunted - Remix.flac",
            ),
        )
    }
}
