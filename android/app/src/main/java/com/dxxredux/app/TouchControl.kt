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

enum class SelectorPresentation { WHEEL, SCROLL_STRIP }

enum class GyroActivation { ALWAYS, TOUCH_STICK, ADS_ONLY }

enum class GyroMode { RATE, ABSOLUTE }

enum class DoubleTapMode {
    REPEAT_FIRE, // Each additional tap fires again (3 taps = 2 fires) -- current default
    SINGLE_FIRE, // One fire per double-tap only (4 taps = 2 fires)
    LATCH_DOUBLE, // Double-tap to latch on, double-tap to release
    LATCH_SINGLE, // Double-tap to latch on, single-tap to release
    HOLD_FIRE, // Double-tap starts, releasing second tap stops
}

enum class StickExtremeAxis { X, Y }

enum class StickExtremeDirection { NEGATIVE, POSITIVE }

enum class StickExtremeActionMode { HOLD, PULSE_ON_ENTER }

// --- Response curve math ---

/** Apply the selected response curve to a normalized -1..1 input value. */
fun applyResponseCurve(
    input: Float,
    curve: ResponseCurve,
    exponent: Float = 2f,
): Float {
    val clamped = input.coerceIn(-1f, 1f)
    return when (curve) {
        ResponseCurve.LINEAR -> {
            clamped
        }

        ResponseCurve.EXPONENTIAL -> {
            sign(clamped) * abs(clamped).pow(exponent)
        }

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

data class StickExtremeAction(
    val enabled: Boolean = false,
    val axis: StickExtremeAxis = StickExtremeAxis.Y,
    val direction: StickExtremeDirection = StickExtremeDirection.POSITIVE,
    val threshold: Float = TouchBindings.DEFAULT_STICK_EXTREME_THRESHOLD,
    val releaseThreshold: Float = TouchBindings.DEFAULT_STICK_EXTREME_RELEASE_THRESHOLD,
    val binding: Int = TouchBindings.BTN_AFTERBURNER,
    val mode: StickExtremeActionMode = StickExtremeActionMode.HOLD,
) {
    fun toJson() =
        JSONObject().apply {
            put("enabled", enabled)
            put("axis", axis.name)
            put("direction", direction.name)
            put("threshold", threshold.toDouble())
            put("releaseThreshold", releaseThreshold.toDouble())
            put("binding", binding)
            put("mode", mode.name)
        }

    companion object {
        fun fromJson(j: JSONObject) =
            normalizeStickExtremeAction(
                StickExtremeAction(
                    enabled = j.optBoolean("enabled"),
                    axis =
                        enumValueOrDefault(
                            j.optString("axis", StickExtremeAxis.Y.name),
                            StickExtremeAxis.Y,
                        ),
                    direction =
                        enumValueOrDefault(
                            j.optString("direction", StickExtremeDirection.POSITIVE.name),
                            StickExtremeDirection.POSITIVE,
                        ),
                    threshold =
                        j
                            .optDouble(
                                "threshold",
                                TouchBindings.DEFAULT_STICK_EXTREME_THRESHOLD.toDouble(),
                            ).toFloat(),
                    releaseThreshold =
                        j
                            .optDouble(
                                "releaseThreshold",
                                TouchBindings.DEFAULT_STICK_EXTREME_RELEASE_THRESHOLD.toDouble(),
                            ).toFloat(),
                    binding = j.optInt("binding", TouchBindings.BTN_AFTERBURNER),
                    mode =
                        enumValueOrDefault(
                            j.optString("mode", StickExtremeActionMode.HOLD.name),
                            StickExtremeActionMode.HOLD,
                        ),
                ),
            )
    }
}

private inline fun <reified T : Enum<T>> enumValueOrDefault(
    name: String,
    defaultValue: T,
): T =
    try {
        enumValueOf<T>(name)
    } catch (_: IllegalArgumentException) {
        defaultValue
    }

internal fun normalizeStickExtremeAction(action: StickExtremeAction): StickExtremeAction {
    val threshold =
        action.threshold.coerceIn(
            TouchBindings.MIN_STICK_EXTREME_THRESHOLD,
            TouchBindings.MAX_STICK_EXTREME_THRESHOLD,
        )
    val release =
        action.releaseThreshold
            .coerceIn(
                TouchBindings.MIN_STICK_EXTREME_RELEASE_THRESHOLD,
                threshold - TouchBindings.MIN_STICK_EXTREME_HYSTERESIS,
            )
    return action.copy(threshold = threshold, releaseThreshold = release)
}

internal fun stickExtremeActionPressed(
    action: StickExtremeAction,
    axisX: Float,
    axisY: Float,
    wasPressed: Boolean,
): Boolean {
    if (!action.enabled) return false
    val value =
        when (action.axis) {
            StickExtremeAxis.X -> axisX
            StickExtremeAxis.Y -> axisY
        }
    val directionalValue =
        when (action.direction) {
            StickExtremeDirection.NEGATIVE -> -value
            StickExtremeDirection.POSITIVE -> value
        }
    val threshold = if (wasPressed) action.releaseThreshold else action.threshold
    return directionalValue > threshold
}

internal fun stickAfterburnerChargeVisible(
    gameVariant: String,
    actions: List<StickExtremeAction>,
): Boolean =
    gameVariant == "d2" &&
        actions.any { it.enabled && it.binding == TouchBindings.BTN_AFTERBURNER }

internal fun afterburnerChargeDepletedFraction(chargePct: Int?): Float =
    (100 - (chargePct ?: 100)).coerceIn(0, 100) / 100f

internal fun guideWheelVisibleSegments(
    segments: List<RadialSegment>,
    secretAreaRevealed: Boolean,
): List<RadialSegment> =
    segments.filter {
        it.binding != TouchBindings.META_GUIDE_RELEASE_CONTROL &&
            (secretAreaRevealed || it.binding != TouchBindings.META_GUIDE_FIND_SECRET)
    }

internal fun stickExtremeTravelFromTouch(
    dxPx: Float,
    dyPx: Float,
    radiusPx: Float,
    invertX: Boolean,
    invertY: Boolean,
): Pair<Float, Float> {
    if (radiusPx <= 0f) return 0f to 0f
    var axisX = dxPx / radiusPx
    var axisY = -dyPx / radiusPx
    if (invertX) axisX = -axisX
    if (invertY) axisY = -axisY
    return axisX to axisY
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
    val sensitivityX: Float = sensitivity,
    val sensitivityY: Float = sensitivity,
    val floating: Boolean = false,
    val floatingZone: FloatingZone = FloatingZone(),
    val hapticFeedback: Boolean = true,
    val mouseMode: Boolean = false,
    val mouseExponential: Boolean = true,
    val mouseExponentialMax: Float = 3f,
    val buttonMode: Boolean = false,
    val negXBinding: Int = TouchBindings.BTN_FIRE_PRIMARY,
    val posXBinding: Int = TouchBindings.BTN_FIRE_PRIMARY,
    val negYBinding: Int = TouchBindings.BTN_FIRE_PRIMARY,
    val posYBinding: Int = TouchBindings.BTN_FIRE_PRIMARY,
    val doubleTapBinding: Int = -1,
    val doubleTapMode: DoubleTapMode = DoubleTapMode.REPEAT_FIRE,
    val extremeActions: List<StickExtremeAction> = emptyList(),
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
            put("sensitivityX", sensitivityX.toDouble())
            put("sensitivityY", sensitivityY.toDouble())
            put("floating", floating)
            put("floatingZone", floatingZone.toJson())
            put("haptic", hapticFeedback)
            if (mouseMode) {
                put("mouseMode", true)
                put("mouseExponential", mouseExponential)
                put("mouseExponentialMax", mouseExponentialMax.toDouble())
            }
            if (buttonMode) {
                put("buttonMode", true)
                put("negXBinding", negXBinding)
                put("posXBinding", posXBinding)
                put("negYBinding", negYBinding)
                put("posYBinding", posYBinding)
            }
            if (doubleTapBinding >= 0) put("doubleTapBinding", doubleTapBinding)
            if (doubleTapMode != DoubleTapMode.REPEAT_FIRE) put("doubleTapMode", doubleTapMode.name)
            if (extremeActions.isNotEmpty()) {
                put("extremeActions", JSONArray(extremeActions.map { it.toJson() }))
            }
        }

    companion object {
        fun fromJson(j: JSONObject): AnalogStickControl {
            // Migration: old configs have a single "sensitivity" field.
            // New configs have sensitivityX / sensitivityY.
            val baseSens = j.optDouble("sensitivity", 1.0).toFloat()
            val sensX = j.optDouble("sensitivityX", baseSens.toDouble()).toFloat()
            val sensY = j.optDouble("sensitivityY", baseSens.toDouble()).toFloat()
            return AnalogStickControl(
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
                sensitivityX = sensX,
                sensitivityY = sensY,
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
                mouseMode = j.optBoolean("mouseMode"),
                mouseExponential = j.optBoolean("mouseExponential", true),
                mouseExponentialMax = j.optDouble("mouseExponentialMax", 3.0).toFloat(),
                buttonMode = j.optBoolean("buttonMode"),
                negXBinding = j.optInt("negXBinding", TouchBindings.BTN_FIRE_PRIMARY),
                posXBinding = j.optInt("posXBinding", TouchBindings.BTN_FIRE_PRIMARY),
                negYBinding = j.optInt("negYBinding", TouchBindings.BTN_FIRE_PRIMARY),
                posYBinding = j.optInt("posYBinding", TouchBindings.BTN_FIRE_PRIMARY),
                doubleTapBinding = j.optInt("doubleTapBinding", -1),
                doubleTapMode =
                    try {
                        DoubleTapMode.valueOf(j.optString("doubleTapMode", "REPEAT_FIRE"))
                    } catch (
                        _: IllegalArgumentException,
                    ) {
                        DoubleTapMode.REPEAT_FIRE
                    },
                extremeActions =
                    j.optJSONArray("extremeActions")?.let { arr ->
                        (0 until arr.length()).map {
                            StickExtremeAction.fromJson(arr.getJSONObject(it))
                        }
                    } ?: emptyList(),
            )
        }
    }
}

internal fun normalizeButtonLongPressDurationMs(durationMs: Int): Int =
    durationMs.coerceIn(
        TouchBindings.MIN_LONG_PRESS_DURATION_MS,
        TouchBindings.MAX_LONG_PRESS_DURATION_MS,
    )

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
    val longPressEnabled: Boolean = false,
    val longPressBinding: Int = -1,
    val longPressDurationMs: Int = TouchBindings.DEFAULT_LONG_PRESS_DURATION_MS,
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
            if (longPressEnabled ||
                longPressBinding >= 0 ||
                longPressDurationMs != TouchBindings.DEFAULT_LONG_PRESS_DURATION_MS
            ) {
                put("longPressEnabled", longPressEnabled)
                put("longPressBinding", longPressBinding)
                put("longPressDurationMs", longPressDurationMs)
            }
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
                longPressEnabled = j.optBoolean("longPressEnabled"),
                longPressBinding = j.optInt("longPressBinding", -1),
                longPressDurationMs =
                    normalizeButtonLongPressDurationMs(
                        j.optInt(
                            "longPressDurationMs",
                            TouchBindings.DEFAULT_LONG_PRESS_DURATION_MS,
                        ),
                    ),
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
    val ringSizeMult: Float = 1f,
    val opacity: Float = 0.7f,
    val segments: List<RadialSegment>,
    val centerLabel: String = "",
    val centerBinding: Int = -1,
    val hapticFeedback: Boolean = true,
    val presentation: SelectorPresentation = SelectorPresentation.WHEEL,
    val stripOrientation: SliderOrientation = SliderOrientation.HORIZONTAL,
    val stripDragSpanWidthPct: Float = 20f,
    val stripLabelAngleDeg: Float = 0f,
    val stripSelectedScale: Float = 2f,
) {
    fun toJson() =
        JSONObject().apply {
            put("type", "radial_menu")
            put("id", id)
            put("x", xPct.toDouble())
            put("y", yPct.toDouble())
            put("size", sizeMult.toDouble())
            put("ringSize", ringSizeMult.toDouble())
            put("opacity", opacity.toDouble())
            put("segments", JSONArray(segments.map { it.toJson() }))
            if (centerLabel.isNotEmpty()) put("centerLabel", centerLabel)
            if (centerBinding >= 0) put("centerBinding", centerBinding)
            put("haptic", hapticFeedback)
            put("presentation", presentation.name)
            put("stripOrientation", stripOrientation.name)
            put("stripDragSpanWidthPct", stripDragSpanWidthPct.toDouble())
            put("stripLabelAngleDeg", stripLabelAngleDeg.toDouble())
            put("stripSelectedScale", stripSelectedScale.toDouble())
        }

    companion object {
        fun fromJson(j: JSONObject) =
            RadialMenuControl(
                id = j.getString("id"),
                xPct = j.getDouble("x").toFloat(),
                yPct = j.getDouble("y").toFloat(),
                sizeMult = j.optDouble("size", 1.0).toFloat(),
                ringSizeMult = j.optDouble("ringSize", 1.0).toFloat(),
                opacity = j.optDouble("opacity", 0.7).toFloat(),
                segments =
                    j.getJSONArray("segments").let { arr ->
                        (0 until arr.length()).map { RadialSegment.fromJson(arr.getJSONObject(it)) }
                    },
                centerLabel = j.optString("centerLabel", ""),
                centerBinding = j.optInt("centerBinding", -1),
                hapticFeedback = j.optBoolean("haptic", true),
                presentation =
                    runCatching {
                        SelectorPresentation.valueOf(j.optString("presentation", SelectorPresentation.WHEEL.name))
                    }.getOrDefault(SelectorPresentation.WHEEL),
                stripOrientation =
                    runCatching {
                        SliderOrientation.valueOf(j.optString("stripOrientation", SliderOrientation.HORIZONTAL.name))
                    }.getOrDefault(SliderOrientation.HORIZONTAL),
                stripDragSpanWidthPct = j.optDouble("stripDragSpanWidthPct", 20.0).toFloat(),
                stripLabelAngleDeg = j.optDouble("stripLabelAngleDeg", 0.0).toFloat(),
                stripSelectedScale = j.optDouble("stripSelectedScale", 2.0).toFloat(),
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
    val mode: GyroMode = GyroMode.ABSOLUTE,
    val invertX: Boolean = false,
    val invertY: Boolean = true,
    val invertZ: Boolean = true,
    val axisX: Int = TouchBindings.AXIS_RIGHT_X,
    val axisY: Int = TouchBindings.AXIS_RIGHT_Y,
    val axisZ: Int = -1, // -1 = disabled (roll not mapped by default)
    val deadzone: Float = 0.1f, // legacy single deadzone, kept for migration
    val deadzoneX: Float = 0.1f, // per-axis: yaw
    val deadzoneY: Float = 0.1f, // per-axis: roll
    val deadzoneZ: Float = 0.3f, // per-axis: pitch (typically needs larger deadzone)
    val maxAngleX: Float = 0.436f, // ~25 degrees
    val maxAngleY: Float = 0.436f,
    val maxAngleZ: Float = 0.436f,
    // Persisted reference orientation from last recenter (null = not yet calibrated)
    val refAzimuth: Float? = null,
    val refPitch: Float? = null,
    val refRoll: Float? = null,
) {
    fun toJson() =
        JSONObject().apply {
            put("enabled", enabled)
            put("activation", activation.name)
            put("mode", mode.name)
            put("invertX", invertX)
            put("invertY", invertY)
            put("invertZ", invertZ)
            put("axisX", axisX)
            put("axisY", axisY)
            put("axisZ", axisZ)
            put("deadzone", deadzoneX.toDouble()) // write deadzoneX as legacy "deadzone" for compat
            put("deadzoneX", deadzoneX.toDouble())
            put("deadzoneY", deadzoneY.toDouble())
            put("deadzoneZ", deadzoneZ.toDouble())
            put("maxAngleX", maxAngleX.toDouble())
            put("maxAngleY", maxAngleY.toDouble())
            put("maxAngleZ", maxAngleZ.toDouble())
            if (refAzimuth != null) put("refAzimuth", refAzimuth.toDouble())
            if (refPitch != null) put("refPitch", refPitch.toDouble())
            if (refRoll != null) put("refRoll", refRoll.toDouble())
        }

    companion object {
        /** Migrate old raw-radian deadzone (<=0.1) to fraction-of-maxAngle. */
        private fun migrateDeadzone(raw: Float): Float {
            // Old format: radians (0.0-0.1). New format: fraction (0.0-0.6).
            // Old default was 0.02 rad with maxAngle 0.436 -> 0.02/0.436 ~= 0.046.
            // Values <= 0.1 are clearly old-format radians; convert to fraction.
            if (raw <= 0.1f && raw > 0f) return (raw / 0.436f).coerceIn(0f, 0.6f)
            return raw
        }

        fun fromJson(j: JSONObject): GyroConfig {
            // Migration: old configs have single maxAngle; new have maxAngleX/Y/Z
            val legacyAngle = j.optDouble("maxAngle", 0.436).toFloat()
            val rawDz = j.optDouble("deadzone", 0.1).toFloat()
            val migratedDz = migrateDeadzone(rawDz)
            // Per-axis deadzones: fall back to migrated single value if absent
            val dzX = if (j.has("deadzoneX")) j.optDouble("deadzoneX").toFloat() else migratedDz
            val dzY = if (j.has("deadzoneY")) j.optDouble("deadzoneY").toFloat() else migratedDz
            val dzZ =
                if (j.has(
                        "deadzoneZ",
                    )
                ) {
                    j.optDouble("deadzoneZ").toFloat()
                } else {
                    (migratedDz * 3f).coerceAtMost(0.6f)
                }
            return GyroConfig(
                enabled = j.optBoolean("enabled"),
                activation = GyroActivation.valueOf(j.optString("activation", "ALWAYS")),
                mode = GyroMode.valueOf(j.optString("mode", "ABSOLUTE")),
                invertX = j.optBoolean("invertX"),
                invertY = j.optBoolean("invertY"),
                invertZ = j.optBoolean("invertZ"),
                axisX = j.optInt("axisX", TouchBindings.AXIS_RIGHT_X),
                axisY = j.optInt("axisY", TouchBindings.AXIS_RIGHT_Y),
                axisZ = j.optInt("axisZ", -1),
                deadzone = migratedDz,
                deadzoneX = dzX,
                deadzoneY = dzY,
                deadzoneZ = dzZ,
                maxAngleX = j.optDouble("maxAngleX", legacyAngle.toDouble()).toFloat(),
                maxAngleY = j.optDouble("maxAngleY", legacyAngle.toDouble()).toFloat(),
                maxAngleZ = j.optDouble("maxAngleZ", legacyAngle.toDouble()).toFloat(),
                refAzimuth = if (j.has("refAzimuth")) j.getDouble("refAzimuth").toFloat() else null,
                refPitch = if (j.has("refPitch")) j.getDouble("refPitch").toFloat() else null,
                refRoll = if (j.has("refRoll")) j.getDouble("refRoll").toFloat() else null,
            )
        }
    }
}

enum class DiagnosticType {
    GYRO,
    MUSIC,
    SETTINGS,
    ;

    /** Human-readable label for the editor UI. */
    val label: String
        get() =
            when (this) {
                GYRO -> "Gyro Display"
                MUSIC -> "Music Controls"
                SETTINGS -> "Settings"
            }
}

data class DiagnosticControl(
    val id: String,
    val xPct: Float,
    val yPct: Float,
    val sizeMult: Float = 1f,
    val opacity: Float = 0.7f,
    val type: DiagnosticType = DiagnosticType.GYRO,
) {
    fun toJson() =
        JSONObject().apply {
            put("type", "diagnostic")
            put("id", id)
            put("x", xPct.toDouble())
            put("y", yPct.toDouble())
            put("size", sizeMult.toDouble())
            put("opacity", opacity.toDouble())
            put("diagType", type.name)
        }

    companion object {
        fun fromJson(j: JSONObject) =
            DiagnosticControl(
                id = j.getString("id"),
                xPct = j.getDouble("x").toFloat(),
                yPct = j.getDouble("y").toFloat(),
                sizeMult = j.optDouble("size", 1.0).toFloat(),
                opacity = j.optDouble("opacity", 0.7).toFloat(),
                type = DiagnosticType.valueOf(j.optString("diagType", "GYRO")),
            )
    }
}

data class AxisRegionControl(
    val id: String,
    val axis: Int = TouchBindings.AXIS_SLIDE_UD,
    val orientation: SliderOrientation = SliderOrientation.VERTICAL,
    val zone: FloatingZone = FloatingZone(leftPct = 0f, topPct = 50f, rightPct = 8f, bottomPct = 100f),
    val sensitivity: Float = 1f,
    val invert: Boolean = false,
    val responseCurve: ResponseCurve = ResponseCurve.LINEAR,
    val exponent: Float = TouchBindings.DEFAULT_EXPONENT,
    val opacity: Float = 0.3f,
) {
    fun toJson() =
        JSONObject().apply {
            put("type", "axis_region")
            put("id", id)
            put("axis", axis)
            put("orientation", orientation.name)
            put("zone", zone.toJson())
            put("sensitivity", sensitivity.toDouble())
            put("invert", invert)
            put("responseCurve", responseCurve.name)
            put("exponent", exponent.toDouble())
            put("opacity", opacity.toDouble())
        }

    companion object {
        fun fromJson(j: JSONObject) =
            AxisRegionControl(
                id = j.getString("id"),
                axis = j.optInt("axis", TouchBindings.AXIS_SLIDE_UD),
                orientation = SliderOrientation.valueOf(j.optString("orientation", "VERTICAL")),
                zone = if (j.has("zone")) FloatingZone.fromJson(j.getJSONObject("zone")) else FloatingZone(),
                sensitivity = j.optDouble("sensitivity", 1.0).toFloat(),
                invert = j.optBoolean("invert"),
                responseCurve = ResponseCurve.valueOf(j.optString("responseCurve", "LINEAR")),
                exponent = j.optDouble("exponent", TouchBindings.DEFAULT_EXPONENT.toDouble()).toFloat(),
                opacity = j.optDouble("opacity", 0.3).toFloat(),
            )
    }
}

data class MoreActionsControl(
    val xPct: Float = 11f,
    val yPct: Float = 94.5f,
    val sizeMult: Float = 1f,
    val opacity: Float = 0.7f,
) {
    fun toJson() =
        JSONObject().apply {
            put("type", "more_actions")
            put("x", xPct.toDouble())
            put("y", yPct.toDouble())
            put("size", sizeMult.toDouble())
            put("opacity", opacity.toDouble())
        }

    companion object {
        fun fromJson(j: JSONObject) =
            MoreActionsControl(
                xPct = j.optDouble("x", 11.0).toFloat(),
                yPct = j.optDouble("y", 94.5).toFloat(),
                sizeMult = j.optDouble("size", 1.0).toFloat(),
                opacity = j.optDouble("opacity", 0.7).toFloat(),
            )
    }
}

data class TouchLayout(
    val version: Int = 2,
    val name: String = "Default",
    val globalOpacity: Float = TouchBindings.DEFAULT_GLOBAL_OPACITY,
    val sticks: List<AnalogStickControl> = emptyList(),
    val buttons: List<ButtonControl> = emptyList(),
    val sliders: List<SliderControl> = emptyList(),
    val radialMenus: List<RadialMenuControl> = emptyList(),
    val dpads: List<DPadControl> = emptyList(),
    val diagnostics: List<DiagnosticControl> = emptyList(),
    val axisRegions: List<AxisRegionControl> = emptyList(),
    val moreActions: MoreActionsControl = MoreActionsControl(),
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
            put("diagnostics", JSONArray(diagnostics.map { it.toJson() }))
            put("axisRegions", JSONArray(axisRegions.map { it.toJson() }))
            put("moreActions", moreActions.toJson())
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
                diagnostics = parseArray("diagnostics") { DiagnosticControl.fromJson(it) },
                axisRegions = parseArray("axisRegions") { AxisRegionControl.fromJson(it) },
                moreActions =
                    if (j.has(
                            "moreActions",
                        )
                    ) {
                        MoreActionsControl.fromJson(j.getJSONObject("moreActions"))
                    } else {
                        MoreActionsControl()
                    },
                gyro = if (j.has("gyro")) GyroConfig.fromJson(j.getJSONObject("gyro")) else GyroConfig(),
            )
        }
    }
}
