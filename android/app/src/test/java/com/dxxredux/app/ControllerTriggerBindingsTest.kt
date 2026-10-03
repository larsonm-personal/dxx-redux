package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Test
import java.io.File

class ControllerTriggerBindingsTest {
    @Test
    fun proportionalTriggersDoNotAlsoEmitDigitalActions() {
        for (game in listOf("d1", "d2")) {
            val bindings = mapOf("RT" to "Accelerate", "GAS" to "Reverse", "R2" to "Fire Primary")
            val buttons = buildMixerButtonMap(bindings, game)
            assertEquals(setOf("27"), buttons.keys().asSequence().toSet())
            assertEquals(1, buildJoyPairs(bindings, emptySet(), game).combiners.size)
        }
    }

    @Test
    fun defaultTriggerSourcesShareActionsAndReleaseOnlyWhenAllAreReleased() {
        val json = JSONObject(File("src/main/assets/configs/controller/default.json").readText())
        val config = HumanReadableConfig.humanJsonToControllerConfig(json).value!!
        for (game in listOf("d1", "d2")) {
            val settings = buildJoySettingsArray(buildJoyPairs(config.bindings, config.inverts, game), game)
            val buttonJson = buildMixerButtonMap(config.bindings, game)
            val buttonMap =
                buttonJson.keys().asSequence().associate { key ->
                    val actions = buttonJson.getJSONArray(key)
                    key.toInt() to (0 until actions.length()).map { actions.getInt(it) }
                }
            for ((action, controls) in listOf(0 to listOf("RT", "GAS", "R2"), 1 to listOf("LT", "BRAKE", "L2"))) {
                assertEquals(TouchBindings.MIXER_BTN_BASE + action, settings[action].toInt() and 0xFF)
                for (control in controls) assertEquals(listOf(action), buttonMap[BUTTON_CONTROLS.getValue(control)])
                val events = mutableListOf<Pair<Int, Int>>()
                val mixer = InputMixer({ button, pressed -> events += button to pressed }, { _, _, _ -> })
                val values = FloatArray(CONTROLLER_SAMPLE_AXIS_COUNT)
                val firstAxis = AXIS_CONTROLS.getValue(controls[0])
                val secondAxis = AXIS_CONTROLS.getValue(controls[1])
                values[firstAxis] = 0.29f
                mixControllerTriggerButtons(mixer, values, defaultThresholds(), buttonMap)
                assertEquals(emptyList<Pair<Int, Int>>(), events)
                values[firstAxis] = 1f
                mixControllerTriggerButtons(mixer, values, defaultThresholds(), buttonMap)
                values[secondAxis] = 1f
                mixControllerTriggerButtons(mixer, values, defaultThresholds(), buttonMap)
                mixer.setButton(action, "ctrl:key", true)
                values[firstAxis] = 0f
                mixControllerTriggerButtons(mixer, values, defaultThresholds(), buttonMap)
                mixer.setButton(action, "ctrl:key", false)
                assertEquals(listOf(action to 1), events)
                values[secondAxis] = 0f
                mixControllerTriggerButtons(mixer, values, defaultThresholds(), buttonMap)
                assertEquals(listOf(action to 1, action to 0), events)
            }
        }
    }
}
