package com.dxxredux.app

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ControllerAxisExponentTest {
    @Test
    fun defaultExponentIsPassThrough() {
        assertEquals(0.5f, applyControllerAxisExponent(0.5f, DEFAULT_CONTROLLER_AXIS_EXPONENT), 0.0001f)
        assertEquals(-0.5f, applyControllerAxisExponent(-0.5f, DEFAULT_CONTROLLER_AXIS_EXPONENT), 0.0001f)
    }

    @Test
    fun exponentialTransformPreservesSign() {
        assertEquals(0.25f, applyControllerAxisExponent(0.5f, 2.0f), 0.0001f)
        assertEquals(-0.25f, applyControllerAxisExponent(-0.5f, 2.0f), 0.0001f)
    }

    @Test
    fun exponentsClampToTouchRange() {
        assertEquals(TouchBindings.MIN_EXPONENT, clampControllerAxisExponent(0.25f), 0.0001f)
        assertEquals(TouchBindings.MAX_EXPONENT, clampControllerAxisExponent(9.0f), 0.0001f)
        assertEquals(DEFAULT_CONTROLLER_AXIS_EXPONENT, clampControllerAxisExponent(Float.NaN), 0.0001f)
    }

    @Test
    fun clampedMapKeepsKnownAxisDefaults() {
        val exponents = clampedControllerAxisExponents(mapOf("RS_X" to 2.5f, "unknown" to 4.0f))

        assertEquals(6, exponents.size)
        assertEquals(2.5f, exponents["RS_X"] ?: 0f, 0.0001f)
        assertEquals(DEFAULT_CONTROLLER_AXIS_EXPONENT, exponents["LS_X"] ?: 0f, 0.0001f)
        assertTrue("unknown" !in exponents)
    }

    @Test
    fun humanReadableControllerConfigRoundTripsAxisExponents() {
        val json =
            HumanReadableConfig.controllerConfigToHumanJson(
                bindings = mapOf("LT" to "Accelerate"),
                inverts = setOf("RS_Y"),
                thresholds = mapOf("LT" to 35),
                axisExponents = mapOf("LT" to 2.5f),
            )

        val parsed = HumanReadableConfig.humanJsonToControllerConfig(json)

        assertTrue(parsed.warnings.isEmpty())
        assertEquals(2.5f, parsed.value?.axisExponents?.get("LT") ?: 0f, 0.0001f)
        assertEquals(35, parsed.value?.thresholds?.get("LT"))
        assertEquals("Accelerate", parsed.value?.bindings?.get("LT"))
    }
}