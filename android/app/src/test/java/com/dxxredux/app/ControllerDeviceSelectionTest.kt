package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class ControllerDeviceSelectionTest {
    @Test
    fun prefersNamedPhysicalPadOverVirtualDevice() {
        val selected =
            selectDisplayedController(
                listOf(
                    ControllerDisplayDevice(2, "Virtual", 0, 0, true),
                    ControllerDisplayDevice(7, "8BitDo Pro 2", 11720, 12545, false),
                ),
            )

        assertEquals("8BitDo Pro 2", selected?.name)
    }

    @Test
    fun prefersHardwareIdsWhenNamesAreOtherwiseComparable() {
        val selected =
            selectDisplayedController(
                listOf(
                    ControllerDisplayDevice(3, "Wireless Controller", 0, 0, false),
                    ControllerDisplayDevice(4, "Wireless Controller", 1356, 3302, false),
                ),
            )

        assertEquals(4, selected?.id)
    }

    @Test
    fun returnsNullWhenNoDevicesArePresent() {
        assertNull(selectDisplayedController(emptyList()))
    }
}