package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class MissionMovieScanTest {
    @Test
    fun parsesNativeScanWithoutRequiringLevelAnalysis() {
        val result =
            LevelMetadataResult.fromJson(
                """{
            "status":"ok", "movies":{
                "status":"ok", "required":["rb3.mve","rb9.mve"],
                "missing":["rb3.mve","rb9.mve"],
                "unavailable_libraries":["d2x-h.mvl / d2x-l.mvl"], "problems":[]
            }
        }""",
            )
        val scan = checkNotNull(result.movies)
        assertTrue(result.levels.isEmpty())
        assertEquals("Missing movies (2): rb3.mve, rb9.mve", scan.summary)
        assertEquals(listOf("d2x-h.mvl / d2x-l.mvl"), scan.unavailableLibraries)
    }

    @Test
    fun incompleteScanDoesNotClaimAllMoviesAreAvailable() {
        val incomplete = MissionMovieScan("partial", listOf("rb3.mve"), emptyList(), emptyList(), listOf("Read failed"))
        assertEquals("Movie scan incomplete", incomplete.summary)
        assertEquals(
            "All 1 referenced movies are available",
            incomplete.copy(status = "ok", problems = emptyList()).summary,
        )
        assertFalse(incomplete.copy(status = "not_applicable").summary.contains("Missing"))
    }
}
