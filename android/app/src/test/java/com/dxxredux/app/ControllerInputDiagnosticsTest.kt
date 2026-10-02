package com.dxxredux.app

import org.junit.Assert.*
import org.junit.Test

class ControllerInputDiagnosticsTest {
    @Test
    fun bothModeKeepsRawButtonBrakeAndUndeclaredAxisThroughReleaseAndExport() {
        val state = ControllerInputDiagnosticsState()
        state.device(RawControllerDevice(7, "Handheld", 0x1000411,
            listOf(RawControllerRange(23, 0x1000010, "AXIS_BRAKE", 0f, 1f, 0.02f, 0f))))
        assertNull(state.snapshot().axes.single().value)
        state.button(7, 0x401, 104, "KEYCODE_BUTTON_L2", true, 100)
        state.axis(7, 0x1000010, 23, "AXIS_BRAKE", 0.45f, 101)
        state.axis(7, 0x1000010, 40, "AXIS_GENERIC_9", 0.7f, 102)
        state.axis(7, 0x1000010, 41, "AXIS_GENERIC_10", 0f, 102)
        val held = state.snapshot()
        assertEquals(2, held.axes.size)
        assertEquals(0.45f, held.axes.first().value)
        assertTrue(held.buttons.single().pressed)
        state.button(7, 0x401, 104, "KEYCODE_BUTTON_L2", false, 110)
        state.axis(7, 0x1000010, 23, "AXIS_BRAKE", 0f, 111)
        state.axis(7, 0x1000010, 40, "AXIS_GENERIC_9", 0f, 112)
        val json = state.snapshot().toJson()
        assertEquals(2, json.getJSONArray("axes").length())
        assertFalse(json.getJSONArray("buttons").getJSONObject(0).getBoolean("pressed"))
        assertFalse(json.getJSONArray("axes").getJSONObject(1).getBoolean("advertised"))
        assertEquals(6, json.getJSONArray("recent_events").length())
        assertTrue(held.buttons.single().pressed)
    }

    @Test
    fun sameCodesOnDifferentDevicesAndSourcesRemainIndependent() {
        val state = ControllerInputDiagnosticsState()
        state.button(1, 0x401, 188, "KEYCODE_BUTTON_1", true, 1)
        state.button(2, 0x401, 188, "KEYCODE_BUTTON_1", true, 2)
        state.button(1, 0x101, 188, "KEYCODE_BUTTON_1", true, 3)
        state.button(1, 0x401, 188, "KEYCODE_BUTTON_1", false, 4)
        assertEquals(2, state.snapshot().buttons.count { it.pressed })
        state.removeDevice(1)
        assertEquals(2, state.snapshot().buttons.single().deviceId)
    }

    @Test
    fun capabilityChangeAndFocusLossInvalidateLiveStateButKeepHistory() {
        val state = ControllerInputDiagnosticsState()
        val device = RawControllerDevice(1, "Handheld", 0x401)
        state.device(device)
        state.button(1, 0x401, 105, "KEYCODE_BUTTON_R2", true, 1)
        state.device(device.copy(ranges = listOf(RawControllerRange(22, 0x1000010, "AXIS_GAS", 0f, 1f, 0f, 0f))))
        assertTrue(state.snapshot().buttons.isEmpty())
        state.axis(1, 0x1000010, 22, "AXIS_GAS", 1f, 2)
        state.clearLiveState()
        assertNull(state.snapshot().axes.single().value)
        assertEquals(2, state.snapshot().recentEvents.size)
    }

    @Test
    fun repeatsDoNotFloodHistoryAndHistoryIsBounded() {
        val state = ControllerInputDiagnosticsState()
        repeat(100) { state.button(1, 0x401, 188, "KEYCODE_BUTTON_1", true, it.toLong()) }
        assertEquals(1, state.snapshot().recentEvents.size)
        repeat(40) { state.button(1, 0x401, 188, "KEYCODE_BUTTON_1", it % 2 == 1, 100L + it) }
        assertEquals(24, state.snapshot().recentEvents.size)
    }
}
