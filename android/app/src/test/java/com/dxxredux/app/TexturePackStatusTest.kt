package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class TexturePackStatusTest {
    @Test
    fun mountedArchiveDoesNotClaimActualUse() {
        val rows = texturePackStatusRows(JSONObject("""{"status":"none_loaded","mounted_mods":["pack.dxa"]}"""))
        assertEquals("No replacements loaded", rows[0].second)
        assertEquals("None in use", rows[1].second)
    }

    @Test
    fun successfulUploadsIdentifyPackAndMipUse() {
        val rows =
            texturePackStatusRows(
                JSONObject(
                    """{"status":"active","loaded":12,"mipmapped":12,"adapted":1,"packs":[{"name":"d2-hires-256-textures-ktx2.dxa","loaded":12}]}""",
                ),
            )
        assertEquals("Active: 12 replacements loaded", rows[0].second)
        assertEquals("D2 256px (12)", rows[1].second)
        assertTrue(rows[2].second.startsWith("12 textures; 1"))
    }

    @Test
    fun multiplayerPolicyOverridesCachedResidency() {
        val rows = texturePackStatusRows(JSONObject("""{"status":"blocked_multiplayer","loaded":12}"""))
        assertEquals("Blocked in competitive multiplayer", rows[0].second)
    }
}
