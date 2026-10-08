package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class TouchLayoutFormatTest {
    @Test
    fun shippedMouseAccelerationDefaultsAndEditedSettingsRoundTrip() {
        val json = JSONObject(File("src/main/assets/configs/touch/touch_default.json").readText())
        val sticks = json.getJSONArray("sticks")
        val mouseSticks =
            (0 until sticks.length()).map { sticks.getJSONObject(it) }.filter {
                it.optBoolean("mouseMode", false)
            }
        assertTrue(mouseSticks.isNotEmpty())
        mouseSticks.forEach {
            assertTrue(it.getBoolean("mouseExponential"))
            assertEquals(3.0, it.getDouble("mouseExponentialMax"), 0.0)
        }
        // Exercise the shipped sticks without menu key-name parsing, which requires Android
        val stickLayout =
            HumanReadableConfig
                .touchLayoutToHumanJson(TouchLayout(name = json.getString("name")))
                .put("version", json.getInt("version"))
                .put("sticks", sticks)
        val parsed = HumanReadableConfig.humanJsonToTouchLayout(stickLayout)
        assertEquals(emptyList<String>(), parsed.warnings)
        val starter = requireNotNull(parsed.value)
        assertRoundTrips(starter)
        for (enabled in listOf(false, true)) {
            assertRoundTrips(
                starter.copy(
                    sticks =
                        starter.sticks.map {
                            if (it.mouseMode) it.copy(mouseExponential = enabled, mouseExponentialMax = 4.25f) else it
                        },
                ),
            )
        }
    }

    @Test
    fun controllerVisibilitySettingRoundTripsInLayoutsAndSlots() {
        assertRoundTrips(TouchLayout(hideControllerBoundControls = true))
        assertRoundTrips(TouchLayout(hideControllerBoundControls = false))
    }

    private fun assertRoundTrips(layout: TouchLayout) {
        assertEquals(CURRENT_TOUCH_LAYOUT_VERSION, layout.version)
        assertEquals(layout, TouchLayout.fromJson(layout.toJson()))
        val human = HumanReadableConfig.humanJsonToTouchLayout(HumanReadableConfig.touchLayoutToHumanJson(layout))
        assertEquals(emptyList<String>(), human.warnings)
        assertEquals(layout, human.value)
        val slots = ConfigSlotSet(0, listOf(ConfigSlot(DEFAULT_CONFIG_SLOT_NAME, layout)))
        assertEquals(
            slots,
            TouchLayoutSlotRepository.fromExportJsonArray(TouchLayoutSlotRepository.toExportJsonArray(slots), 0),
        )
    }

    @Test
    fun customPerAxisGyroAndLongPressBindingsRoundTripWithoutRewriting() {
        val layout =
            TouchLayout(
                buttons =
                    listOf(
                        ButtonControl(
                            id = "gyro",
                            xPct = 50f,
                            yPct = 90f,
                            binding = TouchBindings.BTN_GYRO_RECENTER,
                            longPressEnabled = true,
                            longPressBinding = TouchBindings.META_GYRO_TOGGLE,
                        ),
                    ),
                gyro = GyroConfig(enabled = true, deadzoneX = 0.03f, deadzoneY = 0.17f, deadzoneZ = 0.4f),
            )
        assertRoundTrips(layout)
    }

    @Test
    fun internalStoragePreservesRecenterCalibration() {
        val layout = TouchLayout(gyro = GyroConfig(refAzimuth = 1.2f, refPitch = -0.4f, refRoll = 0.8f))
        val restored = TouchLayout.fromJson(layout.toJson())
        assertNotNull(restored.gyro.refAzimuth)
        assertEquals(layout, restored)
    }
}
