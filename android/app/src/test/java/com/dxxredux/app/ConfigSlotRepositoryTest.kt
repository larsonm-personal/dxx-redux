package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File
import kotlin.io.path.createTempDirectory

class ConfigSlotRepositoryTest {
    @Test
    fun pairedPilotPreferenceWriteRollsBackAtomically() {
        val root = createTempDirectory("pilot-pref-transaction").toFile()
        try {
            val d1 =
                File(root, "d1x-redux/test.plx").apply {
                    requireNotNull(parentFile).mkdirs()
                    writeText("d1-old")
                }
            val d2 =
                File(root, "d2x-redux/Players/test.plr").apply {
                    requireNotNull(parentFile).mkdirs()
                    writeText("d2-old")
                }
            assertEquals(
                -1,
                writePilotPreferencesToAll(root, {
                    d1.writeText("d1-new")
                    1
                }, {
                    d2.writeText("d2-new")
                    -1
                }),
            )
            assertEquals("d1-old", d1.readText())
            assertEquals("d2-old", d2.readText())

            var d2Called = false
            assertEquals(
                -1,
                writePilotPreferencesToAll(root, {
                    d1.writeText("partial")
                    -1
                }, {
                    d2Called = true
                    1
                }),
            )
            assertEquals("d1-old", d1.readText())
            assertFalse(d2Called)
        } finally {
            root.deleteRecursively()
        }
    }

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

        assertNotNull("touch slot export should parse", parsed)
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

        assertNotNull("touch slot export should parse", parsed)
        val roundTripped = parsed!!
        assertEquals(1, roundTripped.safeActiveIndex)
        assertEquals(DEFAULT_CONFIG_SLOT_NAME, roundTripped.slots[0].name)
        assertEquals("touch custom", roundTripped.activeSlot.name)
        assertEquals("Custom Layout", roundTripped.activeSlot.value.name)
        assertEquals(
            TouchBindings.BTN_FIRE_PRIMARY,
            roundTripped.activeSlot.value.buttons
                .single()
                .binding,
        )
    }

    @Test
    fun touchSlotsRejectMalformedEntriesWithoutCompactingActiveIdentity() {
        val layout = HumanReadableConfig.touchLayoutToHumanJson(TouchLayout(name = "valid"))
        val slots =
            JSONArray()
                .put(JSONObject().put("name", "first").put("layout", layout))
                .put(JSONObject().put("name", "broken"))
                .put(JSONObject().put("name", "active").put("layout", layout))

        assertNull(TouchLayoutSlotRepository.fromExportJsonArray(slots, activeIndex = 2))
        assertNull(TouchLayoutSlotRepository.fromExportJsonArray(JSONArray().put(slots.get(0)), activeIndex = 1))
    }

    @Test
    fun controllerSlotsRejectMalformedEntriesWithoutCompactingActiveIdentity() {
        val config = controllerConfigStateToHumanJson(ControllerConfigState())
        val slots =
            JSONArray()
                .put(JSONObject().put("name", "first").put("config", config))
                .put(JSONObject().put("name", "broken"))
                .put(JSONObject().put("name", "active").put("config", config))

        assertNull(ControllerConfigSlotRepository.fromExportJsonArray(slots, activeIndex = 2))
        assertNull(ControllerConfigSlotRepository.fromExportJsonArray(JSONArray().put(slots.get(0)), activeIndex = 1))
    }
}
