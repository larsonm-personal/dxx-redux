package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import java.io.File

class MusicEqResponseTest {
    @Test fun responseMatchesIndependentScipyCascadeIncludingPreamp() {
        val asset = JSONObject(File("src/main/assets/music_eq_sc55.json").readText())
        // scipy.signal.sosfreqz at 20 / 200 / 2000 / 20000 Hz, 48 kHz
        val expected = listOf(
            doubleArrayOf(-9.177937493, -1.464179145, -1.147486243, -3.413917942),
            doubleArrayOf(-8.419525738, -1.651199974, -1.308460002, -2.884814504),
            doubleArrayOf(-7.157580560, -2.160436353, -1.345509337, -1.651036556),
        )
        listOf(MusicEq.DETAIL, MusicEq.BALANCED, MusicEq.BROAD).forEachIndexed { profile, id ->
            val curve = MusicEqResponse.curve(id, asset)
            listOf(0, 80, 160, 240).forEachIndexed { index, sample ->
                assertEquals(expected[profile][index], curve[sample].second.toDouble(), 0.00001)
            }
        }
        assertTrue(MusicEqResponse.curve(MusicEq.FLAT, asset).all { it.second == 0f })
    }

    @Test fun onlyBundledProfileDefaultsToMeasuredAndInvalidOverridesAreRejected() {
        assertEquals(MusicEq.BALANCED, MusicEq.defaultPreset(MusicEq.BUNDLED))
        assertEquals(MusicEq.FLAT, MusicEq.defaultPreset(MusicEq.OPL3))
        assertEquals(MusicEq.FLAT, MusicEq.defaultPreset(MusicEq.SOUNDFONT_SHA256))
        for (value in listOf("invalid", "{\"opl3\":\"measured-sc55-balanced\"}", "{\"../bad\":\"flat\"}", "{\"bundled\":\"unknown\"}")) {
            assertTrue(runCatching { MusicEq.decodeProfiles(value) }.isFailure)
        }
    }
}
