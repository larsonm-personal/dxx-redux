package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.abs
import kotlin.math.pow
import kotlin.math.sign

// --- Enums ---

enum class ResponseCurve { LINEAR, EXPONENTIAL, S_CURVE }

enum class DPadMode { INDIVIDUAL, CONNECTED, SWIPE_STICK }

enum class ButtonShape { CIRCLE, ROUNDED_RECT }

enum class SliderOrientation { VERTICAL, HORIZONTAL }

enum class GyroActivation { ALWAYS, TOUCH_STICK, ADS_ONLY }

// --- Response curve math ---

/** Apply the selected response curve to a normalized -1..1 input value. */
fun applyResponseCurve(
    input: Float,
    curve: ResponseCurve,
    exponent: Float = 2f,
): Float {
    val clamped = input.coerceIn(-1f, 1f)
    return when (curve) {
        ResponseCurve.LINEAR -> clamped
        ResponseCurve.EXPONENTIAL ->
            sign(clamped) * abs(clamped).pow(exponent)
        ResponseCurve.S_CURVE -> {
            // Attempt a smooth S-curve: low response near center, fast near edges
            val t = abs(clamped)
            val s = t.pow(exponent) / (t.pow(exponent) + (1f - t).pow(exponent))
            sign(clamped) * s
        }
    }
}

// --- Control data classes ---

data class FloatingZone(
    val leftPct: Float = 0f,
    val topPct: Float = 0f,
    val rightPct: Float = 50f,
    val bottomPct: Float = 100f,
) {
    fun toJson() =
        JSONObject().apply {
            put("left", leftPct.toDouble())
            put("top", topPct.toDouble())
            put("right", rightPct.toDouble())
            put("bottom", bottomPct.toDouble())
        }

    companion object {
        fun fromJson(j: JSONObject) =
            FloatingZone(
                j.optDouble("left", 0.0).toFloat(),
                j.optDouble("top", 0.0).toFloat(),
                j.optDouble("right", 50.0).toFloat(),
                j.optDouble("bottom", 100.0).toFloat(),
            )
    }
}

data class AnalogStickControl(
    val id: String,
    val xPct: Float,
    val yPct: Float,
    val sizeMult: Float = 1f,
    val opacity: Float = 0.7f,
    val axisX: Int,
    val axisY: Int,
    val invertX: Boolean = false,
    val invertY: Boolean = false,
    val deadzone: Int = 15,
    val responseCurve: ResponseCurve = ResponseCurve.LINEAR,
    val exponent: Float = TouchBindings.DEFAULT_EXPONENT,
    val sensitivity: Float = 1f,
    val floating: Boolean = false,
    val floatingZone: FloatingZone = FloatingZone(),
    val hapticFeedback: Boolean = true,
    val buttonMode: Boolean = false,
    val negXBinding: Int = TouchBindings.BTN_FIRE_PRIMARY,
    val posXBinding: Int = TouchBindings.BTN_FIRE_PRIMARY,
    val negYBinding: Int = TouchBindings.BTN_FIRE_PRIMARY,
    val posYBinding: Int = TouchBindings.BTN_FIRE_PRIMARY,
) {
    fun toJson() =
        JSONObject().apply {
            put("type", "analog_stick")
            put("id", id)
            put("x", xPct.toDouble())
            put("y", yPct.toDouble())
            put("size", sizeMult.toDouble())
            put("opacity", opacity.toDouble())
            put("axisX", axisX)
            put("axisY", axisY)
            put("invertX", invertX)
            put("invertY", invertY)
            put("deadzone", deadzone)
            put("responseCurve", responseCurve.name)
            put("exponent", exponent.toDouble())
            put("sensitivity", sensitivity.toDouble())
            put("floating", floating)
            put("floatingZone", floatingZone.toJson())
            put("haptic", hapticFeedback)
            if (buttonMode) {
                put("buttonMode", true)
                put("negXBinding", negXBinding)
                put("posXBinding", posXBinding)
                put("negYBinding", negYBinding)
                put("posYBinding", posYBinding)
            }
        }

    companion object {
        fun fromJson(j: JSONObject) =
            AnalogStickControl(
                id = j.getString("id"),
                xPct = j.getDouble("x").toFloat(),
                yPct = j.getDouble("y").toFloat(),
                sizeMult = j.optDouble("size", 1.0).toFloat(),
                opacity = j.optDouble("opacity", 0.7).toFloat(),
                axisX = j.getInt("axisX"),
                axisY = j.getInt("axisY"),
                invertX = j.optBoolean("invertX"),
                invertY = j.optBoolean("invertY"),
                deadzone = j.optInt("deadzone", 15),
                responseCurve = ResponseCurve.valueOf(j.optString("responseCurve", "LINEAR")),
                exponent = j.optDouble("exponent", TouchBindings.DEFAULT_EXPONENT.toDouble()).toFloat(),
                sensitivity = j.optDouble("sensitivity", 1.0).toFloat(),
                floating = j.optBoolean("floating"),
                floatingZone =
                    if (j.has(
                            "floatingZone",
                        )
                    ) {
                        FloatingZone.fromJson(j.getJSONObject("floatingZone"))
                    } else {
                        FloatingZone()
                    },
                hapticFeedback = j.optBoolean("haptic", true),
                buttonMode = j.optBoolean("buttonMode"),
                negXBinding = j.optInt("negXBinding", TouchBindings.BTN_FIRE_PRIMARY),
                posXBinding = j.optInt("posXBinding", TouchBindings.BTN_FIRE_PRIMARY),
                negYBinding = j.optInt("negYBinding", TouchBindings.BTN_FIRE_PRIMARY),
                posYBinding = j.optInt("posYBinding", TouchBindings.BTN_FIRE_PRIMARY),
            )
    }
}

data class ButtonControl(
    val id: String,
    val xPct: Float,
    val yPct: Float,
    val sizeMult: Float = 1f,
    val opacity: Float = 0.7f,
    val binding: Int,
    val label: String = "",
    val shape: ButtonShape = ButtonShape.CIRCLE,
    val toggle: Boolean = false,
    val hapticFeedback: Boolean = true,
) {
    fun toJson() =
        JSONObject().apply {
            put("type", "button")
            put("id", id)
            put("x", xPct.toDouble())
            put("y", yPct.toDouble())
            put("size", sizeMult.toDouble())
            put("opacity", opacity.toDouble())
            put("binding", binding)
            put("label", label)
            put("shape", shape.name)
            put("toggle", toggle)
            put("haptic", hapticFeedback)
        }

    companion object {
        fun fromJson(j: JSONObject) =
            ButtonControl(
                id = j.getString("id"),
                xPct = j.getDouble("x").toFloat(),
                yPct = j.getDouble("y").toFloat(),
                sizeMult = j.optDouble("size", 1.0).toFloat(),
                opacity = j.optDouble("opacity", 0.7).toFloat(),
                binding = j.getInt("binding"),
                label = j.optString("label", ""),
                shape = ButtonShape.valueOf(j.optString("shape", "CIRCLE")),
                toggle = j.optBoolean("toggle"),
                hapticFeedback = j.optBoolean("haptic", true),
            )
    }
}

data class SliderControl(
    val id: String,
    val xPct: Float,
    val yPct: Float,
    val sizeMult: Float = 1f,
    val opacity: Float = 0.7f,
    val axis: Int,
    val orientation: SliderOrientation = SliderOrientation.VERTICAL,
    val responseCurve: ResponseCurve = ResponseCurve.LINEAR,
    val exponent: Float = TouchBindings.DEFAULT_EXPONENT,
    val sensitivity: Float = 1f,
    val springBack: Boolean = true,
) {
    fun toJson() =
        JSONObject().apply {
            put("type", "slider")
            put("id", id)
            put("x", xPct.toDouble())
            put("y", yPct.toDouble())
            put("size", sizeMult.toDouble())
            put("opacity", opacity.toDouble())
            put("axis", axis)
            put("orientation", orientation.name)
            put("responseCurve", responseCurve.name)
            put("exponent", exponent.toDouble())
            put("sensitivity", sensitivity.toDouble())
            put("springBack", springBack)
        }

    companion object {
        fun fromJson(j: JSONObject) =
            SliderControl(
                id = j.getString("id"),
                xPct = j.getDouble("x").toFloat(),
                yPct = j.getDouble("y").toFloat(),
                sizeMult = j.optDouble("size", 1.0).toFloat(),
                opacity = j.optDouble("opacity", 0.7).toFloat(),
                axis = j.getInt("axis"),
                orientation = SliderOrientation.valueOf(j.optString("orientation", "VERTICAL")),
                responseCurve = ResponseCurve.valueOf(j.optString("responseCurve", "LINEAR")),
                exponent = j.optDouble("exponent", TouchBindings.DEFAULT_EXPONENT.toDouble()).toFloat(),
                sensitivity = j.optDouble("sensitivity", 1.0).toFloat(),
                springBack = j.optBoolean("springBack", true),
            )
    }
}

data class RadialSegment(
    val label: String,
    val binding: Int,
    val iconRes: String = "",
    // weapon slot (0-4), -1 = not a weapon segment
    val weaponIndex: Int = -1,
    // "keycode" = binding is an Android KeyEvent keycode (legacy presets)
    // "action"  = binding is a TouchBindings button ID (0-54) or meta action (1000+)
    val bindingType: String = "keycode",
) {
    fun toJson() =
        JSONObject().apply {
            put("label", label)
            put("binding", binding)
            put("iconRes", iconRes)
            if (weaponIndex >= 0) put("wpnIdx", weaponIndex)
            if (bindingType != "keycode") put("bindingType", bindingType)
        }

    companion object {
        fun fromJson(j: JSONObject) =
            RadialSegment(
                j.getString("label"),
                j.getInt("binding"),
                j.optString("iconRes", ""),
                j.optInt("wpnIdx", -1),
                j.optString("bindingType", "keycode"),
            )
    }
}

data class RadialMenuControl(
    val id: String,
    val xPct: Float,
    val yPct: Float,
    val sizeMult: Float = 1f,
    val opacity: Float = 0.7f,
    val segments: List<RadialSegment>,
    val centerLabel: String = "",
    val centerBinding: Int = -1,
    val hapticFeedback: Boolean = true,
) {
    fun toJson() =
        JSONObject().apply {
            put("type", "radial_menu")
            put("id", id)
            put("x", xPct.toDouble())
            put("y", yPct.toDouble())
            put("size", sizeMult.toDouble())
            put("opacity", opacity.toDouble())
            put("segments", JSONArray(segments.map { it.toJson() }))
            if (centerLabel.isNotEmpty()) put("centerLabel", centerLabel)
            if (centerBinding >= 0) put("centerBinding", centerBinding)
            put("haptic", hapticFeedback)
        }

    companion object {
        fun fromJson(j: JSONObject) =
            RadialMenuControl(
                id = j.getString("id"),
                xPct = j.getDouble("x").toFloat(),
                yPct = j.getDouble("y").toFloat(),
                sizeMult = j.optDouble("size", 1.0).toFloat(),
                opacity = j.optDouble("opacity", 0.7).toFloat(),
                segments =
                    j.getJSONArray("segments").let { arr ->
                        (0 until arr.length()).map { RadialSegment.fromJson(arr.getJSONObject(it)) }
                    },
                centerLabel = j.optString("centerLabel", ""),
                centerBinding = j.optInt("centerBinding", -1),
                hapticFeedback = j.optBoolean("haptic", true),
            )
    }
}

data class DPadControl(
    val id: String,
    val xPct: Float,
    val yPct: Float,
    val sizeMult: Float = 1f,
    val opacity: Float = 0.7f,
    val mode: DPadMode = DPadMode.INDIVIDUAL,
    val upBinding: Int,
    val downBinding: Int,
    val leftBinding: Int,
    val rightBinding: Int,
    val swipeThreshold: Float = TouchBindings.DEFAULT_SWIPE_THRESHOLD,
    val hapticFeedback: Boolean = true,
) {
    fun toJson() =
        JSONObject().apply {
            put("type", "dpad")
            put("id", id)
            put("x", xPct.toDouble())
            put("y", yPct.toDouble())
            put("size", sizeMult.toDouble())
            put("opacity", opacity.toDouble())
            put("mode", mode.name)
            put("up", upBinding)
            put("down", downBinding)
            put("left", leftBinding)
            put("right", rightBinding)
            put("swipeThreshold", swipeThreshold.toDouble())
            put("haptic", hapticFeedback)
        }

    companion object {
        fun fromJson(j: JSONObject) =
            DPadControl(
                id = j.getString("id"),
                xPct = j.getDouble("x").toFloat(),
                yPct = j.getDouble("y").toFloat(),
                sizeMult = j.optDouble("size", 1.0).toFloat(),
                opacity = j.optDouble("opacity", 0.7).toFloat(),
                mode = DPadMode.valueOf(j.optString("mode", "INDIVIDUAL")),
                upBinding = j.getInt("up"),
                downBinding = j.getInt("down"),
                leftBinding = j.getInt("left"),
                rightBinding = j.getInt("right"),
                swipeThreshold =
                    j
                        .optDouble(
                            "swipeThreshold",
                            TouchBindings.DEFAULT_SWIPE_THRESHOLD.toDouble(),
                        ).toFloat(),
                hapticFeedback = j.optBoolean("haptic", true),
            )
    }
}

data class GyroConfig(
    val enabled: Boolean = false,
    val activation: GyroActivation = GyroActivation.ALWAYS,
    val sensitivityX: Float = 1f,
    val sensitivityY: Float = 1f,
    val invertX: Boolean = false,
    val invertY: Boolean = false,
    val axisX: Int = TouchBindings.AXIS_RIGHT_X,
    val axisY: Int = TouchBindings.AXIS_RIGHT_Y,
    val deadzone: Float = 0.02f,
) {
    fun toJson() =
        JSONObject().apply {
            put("enabled", enabled)
            put("activation", activation.name)
            put("sensitivityX", sensitivityX.toDouble())
            put("sensitivityY", sensitivityY.toDouble())
            put("invertX", invertX)
            put("invertY", invertY)
            put("axisX", axisX)
            put("axisY", axisY)
            put("deadzone", deadzone.toDouble())
        }

    companion object {
        fun fromJson(j: JSONObject) =
            GyroConfig(
                enabled = j.optBoolean("enabled"),
                activation = GyroActivation.valueOf(j.optString("activation", "ALWAYS")),
                sensitivityX = j.optDouble("sensitivityX", 1.0).toFloat(),
                sensitivityY = j.optDouble("sensitivityY", 1.0).toFloat(),
                invertX = j.optBoolean("invertX"),
                invertY = j.optBoolean("invertY"),
                axisX = j.optInt("axisX", TouchBindings.AXIS_RIGHT_X),
                axisY = j.optInt("axisY", TouchBindings.AXIS_RIGHT_Y),
                deadzone = j.optDouble("deadzone", 0.02).toFloat(),
            )
    }
}

data class TouchLayout(
    val version: Int = 1,
    val name: String = "Default",
    val globalOpacity: Float = TouchBindings.DEFAULT_GLOBAL_OPACITY,
    val sticks: List<AnalogStickControl> = emptyList(),
    val buttons: List<ButtonControl> = emptyList(),
    val sliders: List<SliderControl> = emptyList(),
    val radialMenus: List<RadialMenuControl> = emptyList(),
    val dpads: List<DPadControl> = emptyList(),
    val gyro: GyroConfig = GyroConfig(),
) {
    fun toJson() =
        JSONObject().apply {
            put("version", version)
            put("name", name)
            put("globalOpacity", globalOpacity.toDouble())
            put("sticks", JSONArray(sticks.map { it.toJson() }))
            put("buttons", JSONArray(buttons.map { it.toJson() }))
            put("sliders", JSONArray(sliders.map { it.toJson() }))
            put("radialMenus", JSONArray(radialMenus.map { it.toJson() }))
            put("dpads", JSONArray(dpads.map { it.toJson() }))
            put("gyro", gyro.toJson())
        }

    companion object {
        fun fromJson(j: JSONObject): TouchLayout {
            fun <T> parseArray(
                key: String,
                parse: (JSONObject) -> T,
            ): List<T> {
                val arr = j.optJSONArray(key) ?: return emptyList()
                return (0 until arr.length()).map { parse(arr.getJSONObject(it)) }
            }
            return TouchLayout(
                version = j.optInt("version", 1),
                name = j.optString("name", "Default"),
                globalOpacity = j.optDouble("globalOpacity", TouchBindings.DEFAULT_GLOBAL_OPACITY.toDouble()).toFloat(),
                sticks = parseArray("sticks") { AnalogStickControl.fromJson(it) },
                buttons = parseArray("buttons") { ButtonControl.fromJson(it) },
                sliders = parseArray("sliders") { SliderControl.fromJson(it) },
                radialMenus = parseArray("radialMenus") { RadialMenuControl.fromJson(it) },
                dpads = parseArray("dpads") { DPadControl.fromJson(it) },
                gyro = if (j.has("gyro")) GyroConfig.fromJson(j.getJSONObject("gyro")) else GyroConfig(),
            )
        }
    }
}
