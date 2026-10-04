package com.dxxredux.app

import org.json.JSONObject
import kotlin.math.abs
import kotlin.math.sign

data class ControllerAxisResponse(
    val center: Float = 0.5f,
    val expo: Float = 0f,
) {
    fun clamped(): ControllerAxisResponse =
        ControllerAxisResponse(
            if (center.isFinite()) center.coerceIn(0f, 1f) else 0.5f,
            if (expo.isFinite()) expo.coerceIn(0f, 1f) else 0f,
        )

    fun toJson(): JSONObject = JSONObject().put("center", center.toDouble()).put("expo", expo.toDouble())

    companion object {
        val LINEAR = ControllerAxisResponse(1f, 0f)
        val FINE = ControllerAxisResponse(0.25f, 0.5f)

        fun fromJson(json: JSONObject): ControllerAxisResponse =
            ControllerAxisResponse(
                json.optDouble("center", 0.5).toFloat(),
                json.optDouble("expo", 0.0).toFloat(),
            ).clamped()
    }
}

/** Normalized Actual rates with full command fixed at 100%, shared by gameplay and preview */
internal fun applyControllerAxisResponse(
    input: Float,
    deadzonePercent: Int,
    response: ControllerAxisResponse,
): Float {
    if (!input.isFinite()) return 0f
    val x = input.coerceIn(-1f, 1f)
    val deadzone = deadzonePercent.coerceIn(0, 95) / 100f
    val u = ((abs(x) - deadzone) / (1f - deadzone)).coerceIn(0f, 1f)
    val settings = response.clamped()
    // Betaflight applyActualRates: abs(u) * (expo*u^5 + (1-expo)*u)
    // https://github.com/betaflight/betaflight/blob/master/src/main/fc/rc.c
    val u2 = u * u
    val transition = (1f - settings.expo) * u2 + settings.expo * u2 * u2 * u2
    return sign(x) * (settings.center * u + (1f - settings.center) * transition)
}
