package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ControllerConfigSerializationTest {
    private fun nativeBytes(count: Int): JSONArray = JSONArray().apply { repeat(count) { put(255) } }

    private fun validNativeConfig(): JSONObject =
        JSONObject()
            .put("version", CONTROLLER_CONFIG_VERSION)
            .put("control_type", 1)
            .put("automap_free_flight", 1)
            .put("key_settings_joystick_d1", nativeBytes(50))
            .put("key_settings_joystick_d2", nativeBytes(56))
            .put("key_settings_keyboard", nativeBytes(60))
            .put(
                "thresholds",
                JSONObject()
                    .put(
                        "LS_X",
                        20,
                    ).put("LS_Y", 20)
                    .put("RS_X", 20)
                    .put("RS_Y", 20)
                    .put("LT", 20)
                    .put("RT", 20),
            )

    @Test
    fun nativeConfigValidationRequiresCompleteTypedDomains() {
        assertTrue(isNativeControllerConfigValid(validNativeConfig()))
        assertTrue(isNativeControllerConfigValid(validNativeConfig().put("key_settings_keyboard", nativeBytes(50))))
        assertFalse(isNativeControllerConfigValid(validNativeConfig().put("control_type", "1")))
        assertFalse(isNativeControllerConfigValid(validNativeConfig().put("automap_free_flight", 2)))
        for (size in listOf(49, 61)) {
            assertFalse(
                isNativeControllerConfigValid(validNativeConfig().put("key_settings_keyboard", nativeBytes(size))),
            )
        }
        assertFalse(isNativeControllerConfigValid(validNativeConfig().put("key_settings_joystick_d1", nativeBytes(49))))
        assertFalse(
            isNativeControllerConfigValid(validNativeConfig().apply { getJSONObject("thresholds").put("RT", 96) }),
        )
        assertFalse(
            isNativeControllerConfigValid(
                validNativeConfig().apply { getJSONArray("key_settings_joystick_d2").put(0, -1) },
            ),
        )
    }

    @Test
    fun buildJoySettingsArray_preservesD2HighButtonSlots() {
        val settings = buildJoySettingsArray(buildJoyPairs(emptyMap(), emptySet(), "d2"), "d2")

        assertEquals(56, settings.size)
        assertEquals(104, settings[4].toInt() and 0xFF)
        assertEquals(127, settings[27].toInt() and 0xFF)
        assertEquals(128, settings[28].toInt() and 0xFF)
        assertEquals(129, settings[29].toInt() and 0xFF)
        assertEquals(150, settings[50].toInt() and 0xFF)
        assertEquals(152, settings[52].toInt() and 0xFF)
        assertEquals(154, settings[54].toInt() and 0xFF)
    }

    @Test
    fun buildJoySettingsArray_keepsD1ButtonBounds() {
        val settings = buildJoySettingsArray(buildJoyPairs(emptyMap(), emptySet(), "d1"), "d1")

        assertEquals(50, settings.size)
        assertEquals(104, settings[4].toInt() and 0xFF)
        assertEquals(127, settings[27].toInt() and 0xFF)
        assertEquals(144, settings[44].toInt() and 0xFF)
        assertEquals(145, settings[45].toInt() and 0xFF)
    }
}
