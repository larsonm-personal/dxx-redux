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

        assertEquals(8, migrated.version)
        assertTrue(migratedButton.longPressEnabled)
        assertEquals(TouchBindings.META_GYRO_TOGGLE, migratedButton.longPressBinding)
        assertEquals(TouchBindings.DEFAULT_LONG_PRESS_DURATION_MS, migratedButton.longPressDurationMs)
    }

    @Test
    fun repositoryMigrationRemovesLegacyCheatsMenuControls() {
        val legacyLayout =
            TouchLayout(
                version = 3,
                name = "Legacy cheats",
                buttons =
                    listOf(
                        ButtonControl(
                            id = "cheats",
                            xPct = 50f,
                            yPct = 90f,
                            binding = 100,
                            label = "Cheats",
                        ),
                        ButtonControl(
                            id = "fire",
                            xPct = 60f,
                            yPct = 90f,
                            binding = TouchBindings.BTN_FIRE_PRIMARY,
                            longPressEnabled = true,
                            longPressBinding = 100,
                        ),
                    ),
                radialMenus =
                    listOf(
                        RadialMenuControl(
                            id = "radial",
                            xPct = 50f,
                            yPct = 50f,
                            segments =
                                listOf(
                                    RadialSegment("Cheats", 100),
                                    RadialSegment("Fire", TouchBindings.BTN_FIRE_PRIMARY),
                                ),
                            centerBinding = 100,
                        ),
                    ),
            )

        val migrated = TouchLayoutRepository.migrateForCurrentVersion(legacyLayout)

        assertEquals(8, migrated.version)
        assertEquals(listOf("fire"), migrated.buttons.map { it.id })
        assertFalse(migrated.buttons.single().longPressEnabled)
        assertEquals(-1, migrated.buttons.single().longPressBinding)
        assertEquals(listOf("Fire"), migrated.radialMenus.single().segments.map { it.label })
        assertEquals(-1, migrated.radialMenus.single().centerBinding)
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
    fun gyroRuntimeToggleDoesNotChangeBaseConfiguredState() {
        val configured = gyroRuntimeStateFromConfig(GyroConfig(enabled = true))
        val disabled = toggledGyroRuntimeState(configured)
        val reenabled = toggledGyroRuntimeState(disabled)
        val unconfigured = gyroRuntimeStateFromConfig(GyroConfig(enabled = false))

        assertTrue(configured.configured)
        assertTrue(configured.activeInGame)
        assertTrue(configured.effectiveEnabled)

        assertTrue(disabled.configured)
        assertFalse(disabled.activeInGame)
        assertFalse(disabled.effectiveEnabled)

        assertTrue(reenabled.configured)
        assertTrue(reenabled.activeInGame)
        assertTrue(reenabled.effectiveEnabled)

        assertFalse(toggledGyroRuntimeState(unconfigured).activeInGame)
    }

    @Test
    fun activeIndicatorStateTracksGyroAndHeadlight() {
        val gyroButton =
            ButtonControl(
                id = "gyro_toggle",
                xPct = 50f,
                yPct = 50f,
                binding = TouchBindings.META_GYRO_TOGGLE,
            )
        val lightButton =
            ButtonControl(
                id = "headlight",
                xPct = 10f,
                yPct = 10f,
                binding = TouchBindings.BTN_HEADLIGHT,
            )
        val demoButton =
            ButtonControl(
                id = "demo_record",
                xPct = 20f,
                yPct = 20f,
                binding = TouchBindings.META_DEMO_RECORD_TOGGLE,
            )
        val longPressDemoButton =
            ButtonControl(
                id = "menu",
                xPct = 30f,
                yPct = 30f,
                binding = TouchBindings.META_MENU_CYCLE,
                longPressEnabled = true,
                longPressBinding = TouchBindings.META_DEMO_RECORD_TOGGLE,
            )

        assertTrue(
            buttonHasActiveIndicatorState(
                button = gyroButton,
                gameVariant = "d2",
                weaponState = null,
                gyroConfigured = true,
                gyroActiveInGame = true,
                demoRecordingActive = false,
            ),
        )
        assertFalse(
            buttonHasActiveIndicatorState(
                button = gyroButton,
                gameVariant = "d2",
                weaponState = null,
                gyroConfigured = false,
                gyroActiveInGame = true,
                demoRecordingActive = false,
            ),
        )
        assertTrue(
            buttonHasActiveIndicatorState(
                button = lightButton,
                gameVariant = "d2",
                weaponState = weaponState(playerFlags = WeaponState.PLAYER_FLAGS_HEADLIGHT_ON),
                gyroConfigured = false,
                gyroActiveInGame = false,
                demoRecordingActive = false,
            ),
        )
        assertFalse(
            buttonHasActiveIndicatorState(
                button = lightButton,
                gameVariant = "d2",
                weaponState = weaponState(),
                gyroConfigured = false,
                gyroActiveInGame = false,
                demoRecordingActive = false,
            ),
        )
        assertFalse(
            buttonHasActiveIndicatorState(
                button = demoButton,
                gameVariant = "d2",
                weaponState = null,
                gyroConfigured = false,
                gyroActiveInGame = false,
                demoRecordingActive = false,
            ),
        )
        assertTrue(
            buttonHasActiveIndicatorState(
                button = demoButton,
                gameVariant = "d2",
                weaponState = null,
                gyroConfigured = false,
                gyroActiveInGame = false,
                demoRecordingActive = true,
            ),
        )
        assertTrue(
            buttonHasActiveIndicatorState(
                button = longPressDemoButton,
                gameVariant = "d2",
                weaponState = null,
                gyroConfigured = false,
                gyroActiveInGame = false,
                demoRecordingActive = true,
            ),
        )
    }

    @Test
    fun weaponButtonsShowCurrentSelectionOnSecondLine() {
        val bombButton =
            ButtonControl(
                id = "bomb_drop",
                xPct = 50f,
                yPct = 50f,
                binding = TouchBindings.BTN_DROP_BOMB,
                label = "Bomb",
            )
        val bombCycleButton =
            ButtonControl(
                id = "bomb_cycle",
                xPct = 55f,
                yPct = 50f,
                binding = TouchBindings.BTN_TOGGLE_BOMB,
                label = "Cycle Bomb",
            )
        val primaryButton =
            ButtonControl(
                id = "primary_fire",
                xPct = 60f,
                yPct = 50f,
                binding = TouchBindings.BTN_FIRE_PRIMARY,
                label = "Primary",
            )
        val secondaryCycleButton =
            ButtonControl(
                id = "secondary_cycle",
                xPct = 65f,
                yPct = 50f,
                binding = TouchBindings.BTN_CYCLE_SECONDARY,
                label = "Secondary",
            )
        val weaponState = weaponState(currentPrimary = 6, currentSecondary = 6, currentBomb = 7)

        assertEquals("Drop\nSmart Mine", buttonDisplayLabel(bombButton, "d2", weaponState))
        assertEquals("Cycle\nSmart Mine", buttonDisplayLabel(bombCycleButton, "d2", weaponState))
        assertEquals("Fire\nGauss", buttonDisplayLabel(primaryButton, "d2", weaponState))
        assertEquals("Cycle\nGuided Missile", buttonDisplayLabel(secondaryCycleButton, "d2", weaponState))
    }

    @Test
    fun weaponButtonsFallBackToConfiguredLabelWithoutState() {
        val primaryButton =
            ButtonControl(
                id = "primary_fire",
                xPct = 60f,
                yPct = 50f,
                binding = TouchBindings.BTN_FIRE_PRIMARY,
                label = "Primary",
            )

        assertEquals("Primary", buttonDisplayLabel(primaryButton, "d2", null))
    }

    @Test
    fun longPressDurationClampsToSupportedRange() {
        assertEquals(TouchBindings.MIN_LONG_PRESS_DURATION_MS, normalizeButtonLongPressDurationMs(10))
        assertEquals(TouchBindings.MAX_LONG_PRESS_DURATION_MS, normalizeButtonLongPressDurationMs(5000))
        assertEquals(650, normalizeButtonLongPressDurationMs(650))
    }

    private fun weaponState(
        playerFlags: Int = 0,
        currentPrimary: Int = 0,
        currentSecondary: Int = 0,
        currentBomb: Int = -1,
    ) =
        WeaponState(
            primaryFlags = 0,
            secondaryFlags = 0,
            playerFlags = playerFlags,
            primaryAmmo = IntArray(10),
            secondaryAmmo = IntArray(10),
            primaryAmmoMax = IntArray(10),
            secondaryAmmoMax = IntArray(10),
            currentPrimary = currentPrimary,
            currentSecondary = currentSecondary,
            currentBomb = currentBomb,
        )
}
