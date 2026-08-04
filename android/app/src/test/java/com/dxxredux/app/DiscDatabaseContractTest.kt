package com.dxxredux.app

import org.json.JSONArray
import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class DiscDatabaseContractTest {
    private val physicalDisc =
        """
        {
          "discs": [{
            "id": "physical",
            "label": "Physical disc",
            "game": "d1d2",
            "tracks": [
              {"track": 1, "type": "data", "sha1": "0123456789abcdef0123456789abcdef01234567"},
              {
                "track": 2,
                "type": "audio",
                "sha1": "89abcdef0123456789abcdef0123456789abcdef",
                "name": "Title",
                "chromaprint": "physical-fingerprint",
                "duration_ms": 1000
              }
            ]
          }]
        }
        """.trimIndent()

    @Test
    fun physicalParserAcceptsOnlyCompletePhysicalRecords() {
        val discs = DiscIdentifier.parseDatabase(physicalDisc)

        assertEquals(listOf("physical"), discs.map { it.id })
        assertEquals("d1d2", discs.single().game)
        assertEquals(
            listOf(
                "0123456789abcdef0123456789abcdef01234567",
                "89abcdef0123456789abcdef0123456789abcdef",
            ),
            discs.single().tracks.map { it.sha1 },
        )
    }

    @Test
    fun physicalParserRejectsAlbumAndMalformedPhysicalRecordsExplicitly() {
        val albumInDiscArray =
            """{"discs":[{"id":"album","label":"Album","type":"album","tracks":[]}]}"""
        val malformedPhysical =
            """{"discs":[{"id":"bad","label":"Bad","game":"d1","tracks":[{"track":1,"type":"data"}]}]}"""

        val albumError =
            assertThrows(IllegalArgumentException::class.java) {
                DiscIdentifier.parseDatabase(albumInDiscArray)
            }
        assertEquals(true, albumError.message?.contains("known_albums.json5"))
        assertThrows(IllegalArgumentException::class.java) {
            DiscIdentifier.parseDatabase(malformedPhysical)
        }
    }

    @Test
    fun fingerprintProjectionCombinesPhysicalAndAlbumArrays() {
        val albums =
            """
            {
              "albums": [
                {
                  "id": "album",
                  "label": "Album",
                  "tracks": [{
                    "track": 7,
                    "type": "audio",
                    "name": "Album track",
                    "chromaprint": "album-fingerprint",
                    "duration_ms": 2000
                  }]
                },
                {"id": "empty-album", "label": "Empty album", "tracks": []}
              ]
            }
            """.trimIndent()

        val flattened = JSONArray(flattenFingerprintDatabase(physicalDisc, albums))

        assertEquals(2, flattened.length())
        assertEquals("physical", flattened.getJSONObject(0).getString("disc_id"))
        assertEquals("album", flattened.getJSONObject(1).getString("disc_id"))
        assertEquals("album-fingerprint", flattened.getJSONObject(1).getString("chromaprint"))
    }

    @Test
    fun fingerprintProjectionAcceptsAlbumsWithoutPhysicalDiscs() {
        val flattened =
            JSONArray(
                flattenFingerprintDatabase(
                    """{"discs":[]}""",
                    """{"albums":[{"id":"empty","label":"Empty","tracks":[]}]}""",
                ),
            )

        assertEquals(0, flattened.length())
    }
}
