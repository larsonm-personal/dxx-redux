package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Test

class LevelMetadataResultTest {
    @Test
    fun fromJsonReadsCoopStartsHeader() {
        val result =
            LevelMetadataResult.fromJson(
                """
                {
                  "status": "ok",
                  "source": "Example mission",
                  "game": "d2",
                  "mission_name": "example",
                  "mission_filename": "example.mn2",
                  "coop_starts": "1-4",
                  "levels": [],
                  "problems": []
                }
                """.trimIndent(),
            )

        assertEquals("1-4", result.coopStarts)
    }
}
