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

    @Test
    fun fromJsonReadsObjectiveLabelPosition() {
        val result =
            LevelMetadataResult.fromJson(
                """
                {
                  "status": "ok",
                  "source": "Example mission",
                  "game": "d2",
                  "mission_name": "example",
                  "mission_filename": "example.mn2",
                  "coop_starts": "1",
                  "levels": [{
                    "level_num": 1,
                    "route_steps": [{
                      "index": 1,
                      "kind": "key",
                      "label": "blue key",
                      "label_pos": {"x": 12.5, "y": -4.0, "z": 88.25}
                    }]
                  }],
                  "problems": []
                }
                """.trimIndent(),
            )

        assertEquals(LevelMetadataPosition(12.5, -4.0, 88.25), result.levels.single().routeSteps.single().labelPosition)
    }
}
