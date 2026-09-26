package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class TouchLayoutFormatTest {
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
