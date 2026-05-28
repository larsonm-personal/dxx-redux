package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ConfigSlotRepositoryTest {
    @Test
    fun slotNamesAreTrimmedAndLimited() {
        val normalized = normalizeConfigSlotName("  abc\n123456789012345678901234567890  ")

        assertEquals(CONFIG_SLOT_NAME_MAX_LENGTH, normalized.length)
        assertTrue(normalized.startsWith("abc123"))
    }

    @Test
    fun controllerSlotExportRoundTripsActiveIndexAndDefaultProtection() {
        val defaultConfig = ControllerConfigState(bindings = mapOf("A" to "Fire Primary"))
        val customConfig =
            ControllerConfigState(
                bindings = mapOf("B" to "Fire Secondary"),
                inverts = setOf("RS_Y"),
                thresholds = mapOf("RS_Y" to 42),
                axisExponents = mapOf("RS_Y" to 2.5f),
            )
        val slotSet =
            ConfigSlotSet(
                activeIndex = 1,
                slots =
                    listOf(
                        ConfigSlot("renamed default", defaultConfig),
                        ConfigSlot("custom controller slot name over limit", customConfig),
                    ),
            )

        val exported = ControllerConfigSlotRepository.toExportJsonArray(slotSet)
        val parsed = ControllerConfigSlotRepository.fromExportJsonArray(exported, activeIndex = 1)

        assertNotNull(parsed)
        val roundTripped = parsed!!
        assertEquals(1, roundTripped.safeActiveIndex)
        assertEquals(DEFAULT_CONFIG_SLOT_NAME, roundTripped.slots[0].name)
        assertEquals(CONFIG_SLOT_NAME_MAX_LENGTH, roundTripped.slots[1].name.length)
        assertEquals("Fire Secondary", roundTripped.activeSlot.value.bindings["B"])
        assertEquals(42, roundTripped.activeSlot.value.thresholds["RS_Y"])
        assertEquals(2.5f, roundTripped.activeSlot.value.axisExponents["RS_Y"] ?: 0f, 0.001f)
    }

    @Test
    fun touchSlotExportRoundTripsActiveIndexAndDefaultProtection() {
        val defaultLayout = TouchLayout(name = "Default Layout")
        val customLayout =
            TouchLayout(
                name = "Custom Layout",
                buttons =
                    listOf(
                        ButtonControl(
                            id = "fire",
                            xPct = 50f,
                            yPct = 60f,
                            binding = TouchBindings.BTN_FIRE_PRIMARY,
                        ),
                    ),
            )
        val slotSet =
            ConfigSlotSet(
                activeIndex = 1,
                slots =
                    listOf(
                        ConfigSlot("anything", defaultLayout),
                        ConfigSlot("touch custom", customLayout),
                    ),
            )

        val exported = TouchLayoutSlotRepository.toExportJsonArray(slotSet)
        val parsed = TouchLayoutSlotRepository.fromExportJsonArray(exported, activeIndex = 1)

        assertNotNull(parsed)
        val roundTripped = parsed!!
        assertEquals(1, roundTripped.safeActiveIndex)
        assertEquals(DEFAULT_CONFIG_SLOT_NAME, roundTripped.slots[0].name)
        assertEquals("touch custom", roundTripped.activeSlot.name)
        assertEquals("Custom Layout", roundTripped.activeSlot.value.name)
        assertEquals(TouchBindings.BTN_FIRE_PRIMARY, roundTripped.activeSlot.value.buttons.single().binding)
    }
}