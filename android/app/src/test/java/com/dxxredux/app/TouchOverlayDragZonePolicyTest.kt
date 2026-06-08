package com.dxxredux.app

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TouchOverlayDragZonePolicyTest {
    @Test
    fun allowsMomentaryButtonInD2() {
        assertTrue(
            dragZoneButtonLatchAllowed(
                gameVariant = "d2",
                binding = TouchBindings.BTN_FIRE_PRIMARY,
                pointerId = -1,
                toggle = false,
            ),
        )
    }

    @Test
    fun rejectsToggleButtons() {
        assertFalse(
            dragZoneButtonLatchAllowed(
                gameVariant = "d2",
                binding = TouchBindings.BTN_FIRE_PRIMARY,
                pointerId = -1,
                toggle = true,
            ),
        )
    }

    @Test
    fun rejectsAlreadyOwnedButtons() {
        assertFalse(
            dragZoneButtonLatchAllowed(
                gameVariant = "d2",
                binding = TouchBindings.BTN_FIRE_PRIMARY,
                pointerId = 7,
                toggle = false,
            ),
        )
    }

    @Test
    fun rejectsSpecialButtons() {
        assertFalse(
            dragZoneButtonLatchAllowed(
                gameVariant = "d2",
                binding = TouchBindings.BTN_AUTOMAP,
                pointerId = -1,
                toggle = false,
            ),
        )
        assertFalse(
            dragZoneButtonLatchAllowed(
                gameVariant = "d2",
                binding = TouchBindings.BTN_GYRO_RECENTER,
                pointerId = -1,
                toggle = false,
            ),
        )
    }

    @Test
    fun rejectsD2OnlyButtonsInD1() {
        val d2OnlyBinding = TouchBindings.D2_ONLY_BUTTONS.first()
        assertFalse(
            dragZoneButtonLatchAllowed(
                gameVariant = "d1",
                binding = d2OnlyBinding,
                pointerId = -1,
                toggle = false,
            ),
        )
    }

    @Test
    fun centeredButtonExtendsDragZoneStart() {
        assertTrue(
            buttonExtendsDragZoneStart(
                zoneLeft = 0f,
                zoneTop = 0f,
                zoneRight = 100f,
                zoneBottom = 100f,
                buttonCenterX = 80f,
                buttonCenterY = 50f,
                buttonRadius = 20f,
                touchX = 105f,
                touchY = 50f,
            ),
        )
    }

    @Test
    fun buttonOutsideZoneCenterDoesNotExtendStart() {
        assertFalse(
            buttonExtendsDragZoneStart(
                zoneLeft = 0f,
                zoneTop = 0f,
                zoneRight = 100f,
                zoneBottom = 100f,
                buttonCenterX = 110f,
                buttonCenterY = 50f,
                buttonRadius = 20f,
                touchX = 105f,
                touchY = 50f,
            ),
        )
    }

    @Test
    fun touchOutsideButtonDoesNotExtendStart() {
        assertFalse(
            buttonExtendsDragZoneStart(
                zoneLeft = 0f,
                zoneTop = 0f,
                zoneRight = 100f,
                zoneBottom = 100f,
                buttonCenterX = 80f,
                buttonCenterY = 50f,
                buttonRadius = 20f,
                touchX = 130f,
                touchY = 50f,
            ),
        )
    }
}
