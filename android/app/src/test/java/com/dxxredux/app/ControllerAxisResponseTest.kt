package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ControllerAxisResponseTest {
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
