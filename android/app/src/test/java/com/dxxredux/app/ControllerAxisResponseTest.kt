package com.dxxredux.app

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class ControllerAxisResponseTest {
    @Test
    fun mappingDefaultsCoverFullAndSingleDirectionAxes() {
        val fine = ControllerAxisResponse(0.25f, 0.5f)
        val linear = ControllerAxisResponse(1f, 0f)
        for (function in listOf("Pitch U/D", "Turn L/R", "Pitch Up", "Pitch Down", "Turn Left", "Turn Right")) {
            assertEquals(fine, defaultControllerAxisResponse(function))
        }
        for (function in listOf(
            "Slide L/R",
            "Slide U/D",
            "Throttle",
            "Bank L/R",
            "Slide Left",
            "Slide Right",
            "Slide Up",
            "Slide Down",
            "Accelerate",
            "Reverse",
            "Bank Left",
            "Bank Right",
        )) {
            assertEquals(linear, defaultControllerAxisResponse(function))
        }
    }

    @Test
    fun mappingChangesSetDefaultsWithoutResettingUnchangedCustomCurves() {
        val custom = ControllerAxisResponse(0.7f, 0.8f)
        for (function in listOf(
            "Pitch U/D",
            "Turn L/R",
            "Slide U/D",
            "Slide L/R",
            "Throttle",
            "Bank L/R",
            "Pitch Up",
            "Turn Left",
            "Accelerate",
            "Reverse",
        )) {
            assertEquals(
                defaultControllerAxisResponse(function),
                controllerResponseAfterMappingChange(null, function, custom),
            )
            assertEquals(custom, controllerResponseAfterMappingChange(function, function, custom))
            assertEquals(custom, controllerResponseAfterMappingChange(function, null, custom))
        }
        assertEquals(
            ControllerAxisResponse.LINEAR,
            controllerResponseAfterMappingChange("Turn L/R", "Slide L/R", custom),
        )
        assertEquals(ControllerAxisResponse.FINE, controllerResponseAfterMappingChange("Throttle", "Pitch U/D", custom))
        assertEquals(
            ControllerAxisResponse.LINEAR,
            controllerResponseAfterMappingChange("Fire Primary", "Accelerate", custom),
        )
        assertEquals(custom, controllerResponseAfterMappingChange("Pitch U/D", "Fire Primary", custom))
    }

    @Test
    fun defaultsFillMissingCurvesWhileStoredCustomCurvesSurviveReload() {
        val custom = ControllerAxisResponse(0.9f, 0.65f)
        val config =
            ControllerConfigState(
                bindings = mapOf("LS_X" to "Turn L/R", "RS_X" to "Slide L/R", "LT" to "Pitch Up", "RT" to "Reverse"),
                axisResponses = mapOf("LS_X" to custom, "RS_X" to ControllerAxisResponse.FINE),
            )
        val loaded = controllerConfigStateFromHumanJson(controllerConfigStateToHumanJson(config)).value!!
        assertEquals(custom, loaded.axisResponses["LS_X"])
        assertEquals(ControllerAxisResponse.FINE, loaded.axisResponses["RS_X"])
        assertEquals(ControllerAxisResponse.FINE, loaded.axisResponses["LT"])
        assertEquals(ControllerAxisResponse.LINEAR, loaded.axisResponses["RT"])
        val fresh = ControllerConfigState(bindings = config.bindings)
        assertEquals(ControllerAxisResponse.FINE, fresh.axisResponses["LS_X"])
        assertEquals(ControllerAxisResponse.LINEAR, fresh.axisResponses["RS_X"])
    }

    @Test
    fun shippedPresetExplicitlyUsesFineLookAndLinearMovement() {
        val json = JSONObject(File("src/main/assets/configs/controller/default.json").readText())
        val responses = json.getJSONObject("axis_responses")
        val config = controllerConfigStateFromHumanJson(json).value!!
        for (axis in AXIS_CONTROLS.keys) {
            val expected =
                if (axis == "RS_X" ||
                    axis == "RS_Y"
                ) {
                    ControllerAxisResponse.FINE
                } else {
                    ControllerAxisResponse.LINEAR
                }
            assertEquals(expected, ControllerAxisResponse.fromJson(responses.getJSONObject(axis)))
            assertEquals(expected, config.axisResponses[axis])
        }
    }

    @Test
    fun actualReferenceValuesAndPhysicalDeadzone() {
        val response = ControllerAxisResponse(0.25f, 0.5f)
        // After 10% deadzone, physical 55% is half input
        assertEquals(0.224609375f, applyControllerAxisResponse(0.55f, 10, response), 0.000001f)
        assertEquals(-0.224609375f, applyControllerAxisResponse(-0.55f, 10, response), 0.000001f)
        assertEquals(0f, applyControllerAxisResponse(0.1f, 10, response), 0f)
        assertTrue(applyControllerAxisResponse(0.11f, 10, response) > 0f)
        assertEquals(0.5f, applyControllerAxisResponse(0.55f, 10, ControllerAxisResponse(1f, 1f)), 0.000001f)
    }

    @Test
    fun curvesPreserveTravelAndNeverReverseOrSaturateEarly() {
        for (center in listOf(0f, 0.25f, 0.5f, 1f)) {
            for (expo in listOf(0f, 0.5f, 1f)) {
                for (deadzone in listOf(0, 10, 95)) {
                    val response = ControllerAxisResponse(center, expo)
                    var previous = 0f
                    for (i in 0..1000) {
                        val x = i / 1000f
                        val y = applyControllerAxisResponse(x, deadzone, response)
                        assertTrue(y >= previous)
                        assertTrue(y >= 0f && y <= 1f)
                        if (i < 1000) assertTrue(y < 1f)
                        assertEquals(-y, applyControllerAxisResponse(-x, deadzone, response), 0f)
                        previous = y
                    }
                    assertEquals(1f, previous, 0f)
                }
            }
        }
    }

    @Test
    fun zeroDeadzoneAndResponseSurviveConfigurationRoundTrip() {
        val config =
            ControllerConfigState(
                bindings = mapOf("LT" to "Accelerate"),
                thresholds = mapOf("LT" to 0),
                axisResponses = mapOf("LT" to ControllerAxisResponse(0.25f, 0.75f)),
            )
        val parsed = controllerConfigStateFromHumanJson(controllerConfigStateToHumanJson(config))
        assertTrue(parsed.warnings.isEmpty())
        assertEquals(0, parsed.value?.thresholds?.get("LT"))
        assertEquals(config.axisResponses["LT"], parsed.value?.axisResponses?.get("LT"))
        assertEquals(8, parsed.value?.axisResponses?.size)
    }

    @Test
    fun nonfiniteInputCannotReachNativeTransport() {
        assertEquals(0f, applyControllerAxisResponse(Float.NaN, 10, ControllerAxisResponse()), 0f)
        assertEquals(0f, applyControllerAxisResponse(Float.POSITIVE_INFINITY, 10, ControllerAxisResponse()), 0f)
        assertEquals(ControllerAxisResponse(), ControllerAxisResponse(Float.NaN, Float.NaN).clamped())
    }

    @Test
    fun digitalDirectionsUseRawThresholdAndReleaseIndependently() {
        val changes = mutableListOf<Pair<Int, Int>>()
        val mixer = InputMixer({ action, pressed -> changes += action to pressed }, { _, _, _ -> })
        val samples = FloatArray(CONTROLLER_SAMPLE_AXIS_COUNT)
        val buttons = mapOf(10 to listOf(5), 11 to listOf(6), 19 to listOf(7))
        samples[0] = -0.31f
        samples[4] = 0.31f
        mixControllerAxisButtons(mixer, samples, defaultThresholds().mapValues { 30 }, buttons)
        assertEquals(listOf(5 to 1, 7 to 1), changes)
        changes.clear()
        samples.fill(0f)
        mixControllerAxisButtons(mixer, samples, defaultThresholds().mapValues { 30 }, buttons)
        assertEquals(listOf(5 to 0, 7 to 0), changes)
    }
}
