package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LevelMetadataResultTest {
    @Test
    fun fromJsonReadsAndFormatsLevelStatistics() {
        val result =
            LevelMetadataResult.fromJson(
                """
                {
                  "status": "ok",
                  "levels": [{
                    "level_num": 2,
                    "segment_count": 518,
                    "wall_count": 115,
                    "trigger_count": 16,
                    "object_count": 229,
                    "texture_count": 53
                  }]
                }
                """.trimIndent(),
            )

        val level = result.levels.single()
        assertEquals(518, level.segmentCount)
        assertEquals(115, level.wallCount)
        assertEquals(16, level.triggerCount)
        assertEquals(229, level.objectCount)
        assertEquals(53, level.textureCount)
        assertEquals(
            "Statistics: 518 cubes, 115 walls, 16 triggers, 229 objects, 53 textures",
            formatLevelMetadataStatistics(
                level.segmentCount,
                level.wallCount,
                level.triggerCount,
                level.objectCount,
                level.textureCount,
            ),
        )
    }

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
    fun fromJsonReadsInheritedMidiMetadata() {
        val result =
            LevelMetadataResult.fromJson(
                """
                {
                  "status": "ok",
                  "music_tracks": [{
                    "slot_index": 9,
                    "slot_kind": "level",
                    "filename": "game05.hmp",
                    "format": "hmp",
                    "resolved_name": "Created for Final Insertion Levels... (Verran Eventide)",
					"duration_ms": 123456,
                    "metadata_source_filename": "game05.mid",
                    "inherited_from_midi": true,
                    "parse_status": "ok",
                    "smf_format": 1,
                    "track_count": 12,
                    "time_division": 480,
                    "title": "Created for Final Insertion Levels; Descent 2",
                    "composer": "Verran Eventide",
                    "display_name": "Created for Final Insertion Levels... (Verran Eventide)",
                    "metadata_truncated": false,
                    "text_events": [{"track_index":0,"type":"Copyright","text":"Copyright by Verran Eventide"}]
                  }],
                  "levels": []
                }
                """.trimIndent(),
            )

        val track = result.musicTracks.single()
        assertEquals("game05.hmp", track.filename)
        assertEquals("game05.mid", track.metadata.metadata_source_filename)
        assertTrue(track.metadata.inherited_from_midi)
        assertEquals("Verran Eventide", track.metadata.composer)
        assertEquals("Created for Final Insertion Levels... (Verran Eventide)", track.resolvedName)
		assertEquals(123456, track.durationMs)
        assertEquals("Copyright", track.metadata.text_events.single().type)
    }

    @Test
    fun fromJsonRetainsCompressedAudioMetadata() {
        val result =
            LevelMetadataResult.fromJson(
                """
                {
                  "status": "ok",
                  "music_tracks": [{
                    "slot_index": 4,
                    "slot_kind": "credits",
                    "filename": "credits.flac",
                    "format": "flac",
                    "resolved_name": "Reactor Core (Jane Doe)",
					"duration_ms": 65432,
                    "parse_status": "ok",
                    "title": "Reactor Core",
                    "composer": "Jane Doe",
                    "artist": "Test Performer",
                    "album": "DXX Test Album",
                    "genre": "Game",
                    "display_name": "Reactor Core (Jane Doe)",
                    "properties": [{"key":"CUSTOM","values":["First","Second"]}]
                  }],
                  "levels": []
                }
                """.trimIndent(),
            )

        val metadata = result.musicTracks.single().audioMetadata!!
        assertEquals("Reactor Core (Jane Doe)", result.musicTracks.single().resolvedName)
        assertEquals("Jane Doe", metadata.composer)
        assertEquals("Test Performer", metadata.artist)
        assertEquals("DXX Test Album", metadata.album)
        assertEquals(listOf("First", "Second"), metadata.properties.single().values)
    }

    @Test
    fun regressionProjectionContainsCompactTrackDetails() {
        val result =
            LevelMetadataResult.fromJson(
                """
                {
                  "status": "ok",
                  "music_tracks": [{
                    "slot_index": 5,
                    "slot_kind": "level",
                    "filename": "level01.ogg",
                    "format": "ogg",
                    "resolved_name": "Reactor Core (Jane Doe)",
					"duration_ms": 65432,
                    "parse_status": "ok",
                    "title": "Reactor Core",
                    "composer": "Jane Doe",
                    "display_name": "Reactor Core (Jane Doe)",
                    "properties": [{"key":"ARTIST","values":["Test Performer"]}]
                  }],
                  "levels": []
                }
                """.trimIndent(),
            )

        val projection = levelMetadataTrackNamesJson(result.musicTracks)

        val row = projection.getJSONObject(0)
		assertEquals(setOf("track", "name", "filename", "format", "length_s"), row.keys().asSequence().toSet())
        assertEquals(5, row.getInt("track"))
        assertEquals("Reactor Core (Jane Doe)", row.getString("name"))
		assertEquals("level01.ogg", row.getString("filename"))
		assertEquals("ogg", row.getString("format"))
		assertEquals(65, row.getInt("length_s"))
		assertFalse(row.has("parse_status"))
        assertFalse(row.has("title"))
        assertFalse(row.has("properties"))
    }

    @Test
    fun regressionProjectionIncludesOnlyFailedParseStatus() {
        val result =
            LevelMetadataResult.fromJson(
                """
                {
                  "status": "ok",
                  "music_tracks": [{
                    "slot_index": 0,
                    "filename": "broken.mp3",
                    "format": "mp3",
                    "resolved_name": "broken",
                    "duration_ms": 0,
                    "parse_status": "invalid"
                  }],
                  "levels": []
                }
                """.trimIndent(),
            )

        val row = levelMetadataTrackNamesJson(result.musicTracks).getJSONObject(0)
        assertEquals(0, row.getInt("length_s"))
        assertEquals("invalid", row.getString("parse_status"))
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

    @Test
    fun fromJsonReadsNestedReplacementGroups() {
        val result =
            LevelMetadataResult.fromJson(
                """
                {
                  "status": "ok",
                  "levels": [{
                    "level_num": 4,
                    "replacement_groups": [
                      {"kind":"ship_stats","label":"Ship stats","summary":"1 change","items":[
                        {"kind":"player_ship","label":"Player ship","fields":[
                          {"kind":"player_ship_size","label":"Size","base_game":310325,"mod":310313,"format":"fixed"}
                        ]}
                      ]},
                      {"kind":"weapon_balance","label":"Weapon balance","summary":"1 change","items":[
                        {"kind":"weapon","label":"Weapon 30","fields":[
                          {"kind":"added","label":"Definition","base_game_text":"Not present","mod_text":"Added"}
                        ]}
                      ]},
                      {"kind":"robot_changes","label":"Robot changes","summary":"1 change","items":[
                        {"kind":"robot","number":60,"label":"Robot 60","fields":[
                          {"kind":"shields","label":"Shields","base_game":655360,"mod":1310720,"format":"fixed"}
                        ]}
                      ]},
                      {"kind":"asset_replacements","label":"Texture/model/sound replacements","summary":"1 change","items":[
                        {"kind":"textures","label":"Textures","summary":"12 replaced","fields":[]}
                      ]}
                    ]
                  }]
                }
                """.trimIndent(),
            )

        val groups = result.levels.single().replacementGroups
        assertEquals(4, groups.size)
        assertEquals("fixed", groups[0].items.single().fields.single().format)
        val added = groups[1].items.single().fields.single()
        assertEquals("Not present", added.baseGameText)
        assertEquals("Added", added.modText)
        assertEquals(60, groups[2].items.single().number)
        assertEquals("12 replaced", groups[3].items.single().summary)
    }
}
