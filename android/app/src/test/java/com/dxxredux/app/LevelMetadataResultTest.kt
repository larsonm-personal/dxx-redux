package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
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
                      "key_carrier_objnum": 42,
                      "label_pos": {"x": 12.5, "y": -4.0, "z": 88.25}
                    }]
                  }],
                  "problems": []
                }
                """.trimIndent(),
            )

        assertEquals(LevelMetadataPosition(12.5, -4.0, 88.25), result.levels.single().routeSteps.single().labelPosition)
        assertEquals(42, result.levels.single().routeSteps.single().keyCarrierObjnum)
    }

    @Test
    fun fromJsonReadsChangedReplacementsAndOmitsUnchangedRows() {
        val result =
            LevelMetadataResult.fromJson(
                """
                {
                  "status": "ok",
                  "levels": [{
                    "level_num": 4,
                    "replacements": [
                      {"kind":"player_ship_size","label":"Player ship size","base_game":310325,"mod":310313},
                      {"kind":"unchanged","label":"Unchanged","base_game":10,"mod":10}
                    ]
                  }]
                }
                """.trimIndent(),
            )

        val replacement = result.levels.single().replacements.single()
        assertEquals("Player ship size", replacement.label)
        assertEquals(310325, replacement.baseGame)
        assertEquals(310313, replacement.mod)
        assertEquals("4.735184", formatLevelMetadataReplacement(replacement, replacement.baseGame))
        assertEquals("4.735001", formatLevelMetadataReplacement(replacement, replacement.mod))
        assertTrue(result.problems.isEmpty())
    }
}
