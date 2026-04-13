package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class GyroToggleConfigTest {
    @Test
    fun gyroToggleBindingNameRoundTrips() {
        assertEquals("Meta: Gyro On/Off", TouchBindings.bindingToName(TouchBindings.META_GYRO_TOGGLE))
        assertEquals(TouchBindings.META_GYRO_TOGGLE, TouchBindings.nameToBinding("Meta: Gyro On/Off"))
        assertEquals(TouchBindings.META_GYRO_TOGGLE, TouchBindings.nameToBinding("Gyro On/Off"))
    }

    @Test
    fun repositoryMigrationUpgradesLegacyGyroRecenterButtons() {
        val legacyLayout =
            TouchLayout(
                version = 1,
                name = "Legacy",
                buttons =
                    listOf(
                        ButtonControl(
                            id = "gyro_recenter",
                            xPct = 50f,
                            yPct = 90f,
                            binding = TouchBindings.BTN_GYRO_RECENTER,
                            label = "GR",
                        ),
                    ),
            )

        val migrated = TouchLayoutRepository.migrateForCurrentVersion(legacyLayout)
        val migratedButton = migrated.buttons.single()

        assertEquals(2, migrated.version)
        assertTrue(migratedButton.longPressEnabled)
        assertEquals(TouchBindings.META_GYRO_TOGGLE, migratedButton.longPressBinding)
        assertEquals(TouchBindings.DEFAULT_LONG_PRESS_DURATION_MS, migratedButton.longPressDurationMs)
    }

    @Test
    fun gyroToggleIndicatorMatchesPrimaryAndLongPressBindings() {
        val primaryToggle =
            ButtonControl(
                id = "gyro_toggle",
                xPct = 50f,
                yPct = 50f,
                binding = TouchBindings.META_GYRO_TOGGLE,
            )
        val secondaryToggle =
            ButtonControl(
                id = "gyro_recenter",
                xPct = 50f,
                yPct = 90f,
                binding = TouchBindings.BTN_GYRO_RECENTER,
                longPressEnabled = true,
                longPressBinding = TouchBindings.META_GYRO_TOGGLE,
            )
        val plainButton =
            ButtonControl(
                id = "fire",
                xPct = 85f,
                yPct = 85f,
                binding = TouchBindings.BTN_FIRE_PRIMARY,
                longPressEnabled = true,
                longPressBinding = TouchBindings.BTN_FIRE_SECONDARY,
            )

        assertTrue(buttonUsesGyroToggleIndicator(primaryToggle))
        assertTrue(buttonUsesGyroToggleIndicator(secondaryToggle))
        assertFalse(buttonUsesGyroToggleIndicator(plainButton))
    }

    @Test
    fun longPressDurationClampsToSupportedRange() {
        assertEquals(TouchBindings.MIN_LONG_PRESS_DURATION_MS, normalizeButtonLongPressDurationMs(10))
        assertEquals(TouchBindings.MAX_LONG_PRESS_DURATION_MS, normalizeButtonLongPressDurationMs(5000))
        assertEquals(650, normalizeButtonLongPressDurationMs(650))
    }
}