package com.dxxredux.app

import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.abs
import kotlin.math.pow
import kotlin.math.sign

private val TOUCH_FLOAT_RANGES =
    mapOf(
        "x" to 0.0..100.0,
        "y" to 0.0..100.0,
        "left" to 0.0..100.0,
        "top" to 0.0..100.0,
        "right" to 0.0..100.0,
        "bottom" to 0.0..100.0,
        "size" to TouchBindings.MIN_SIZE.toDouble()..TouchBindings.MAX_SIZE.toDouble(),
        "ringSize" to TouchBindings.MIN_SIZE.toDouble()..TouchBindings.MAX_SIZE.toDouble(),
        "opacity" to TouchBindings.MIN_OPACITY.toDouble()..TouchBindings.MAX_OPACITY.toDouble(),
        "globalOpacity" to TouchBindings.MIN_GLOBAL_OPACITY.toDouble()..TouchBindings.MAX_GLOBAL_OPACITY.toDouble(),
        "sensitivity" to TouchBindings.MIN_SENSITIVITY.toDouble()..TouchBindings.MAX_SENSITIVITY.toDouble(),
        "sensitivityX" to TouchBindings.MIN_SENSITIVITY.toDouble()..TouchBindings.MAX_SENSITIVITY.toDouble(),
        "sensitivityY" to TouchBindings.MIN_SENSITIVITY.toDouble()..TouchBindings.MAX_SENSITIVITY.toDouble(),
        "exponent" to TouchBindings.MIN_EXPONENT.toDouble()..TouchBindings.MAX_EXPONENT.toDouble(),
        "mouseExponentialMax" to 1.0..10.0,
        "threshold" to
            TouchBindings.MIN_STICK_EXTREME_THRESHOLD.toDouble()..TouchBindings.MAX_STICK_EXTREME_THRESHOLD.toDouble(),
        "releaseThreshold" to 0.5f.toDouble()..2.45f.toDouble(),
        "swipeThreshold" to
            TouchBindings.MIN_SWIPE_THRESHOLD.toDouble()..TouchBindings.MAX_SWIPE_THRESHOLD.toDouble(),
        "stripDragSpanWidthPct" to 5.0..80.0,
        "stripLabelAngleDeg" to -90.0..90.0,
        "stripSelectedScale" to 1.0..3.0,
        "stripCardScale" to MIN_SCROLL_STRIP_CARD_SCALE.toDouble()..MAX_SCROLL_STRIP_CARD_SCALE.toDouble(),
        "maxAngle" to 0.1f.toDouble()..1.57f.toDouble(),
        "maxAngleX" to 0.1f.toDouble()..1.57f.toDouble(),
        "maxAngleY" to 0.1f.toDouble()..1.57f.toDouble(),
        "maxAngleZ" to 0.1f.toDouble()..1.57f.toDouble(),
    )
private val TOUCH_UNBOUNDED_FLOAT_KEYS =
    setOf("deadzone", "deadzoneX", "deadzoneY", "deadzoneZ", "refAzimuth", "refPitch", "refRoll")

internal fun validateTouchLayoutJsonNumbers(root: JSONObject): String? {
    val pending = ArrayDeque<Pair<String, Any>>()
    pending.add("touch_layout" to root)
    while (pending.isNotEmpty()) {
        val (path, value) = pending.removeLast()
        when (value) {
            is JSONObject -> {
                val keys = value.keys()
                while (keys.hasNext()) {
                    val key = keys.next()
                    val child = value.get(key)
                    val childPath = "$path.$key"
                    val range = TOUCH_FLOAT_RANGES[key]
                    if (range != null || key in TOUCH_UNBOUNDED_FLOAT_KEYS) {
                        val number = child as? Number ?: return "$childPath must be a JSON number"
                        val decoded = number.toDouble()
                        if (!decoded.isFinite() || !decoded.toFloat().isFinite()) return "$childPath must be finite"
                        if (range != null &&
                            decoded !in range
                        ) {
                            return "$childPath is outside ${range.start}..${range.endInclusive}"
                        }
                    }
                    if (child is JSONObject || child is JSONArray) pending.add(childPath to child)
                }
            }

            is JSONArray -> {
                for (index in 0 until value.length()) {
                    val child = value.get(index)
                    if (child is JSONObject || child is JSONArray) pending.add("$path[$index]" to child)
                }
            }
        }
    }
    return null
}

// --- Enums ---

enum class ResponseCurve { LINEAR, EXPONENTIAL, S_CURVE }

enum class DPadMode { INDIVIDUAL, CONNECTED, SWIPE_STICK }

enum class ButtonShape { CIRCLE, ROUNDED_RECT }

enum class SliderOrientation { VERTICAL, HORIZONTAL }

enum class SelectorPresentation { WHEEL, SCROLL_STRIP }

enum class ScrollStripRowOffset(
    val crossDirection: Float,
    val symbol: String,
) {
    ABOVE_LEFT(-1f, "+"),
    CENTERED(0f, "0"),
    BELOW_RIGHT(1f, "-"),
}

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
    if (!input.isFinite() || !exponent.isFinite() ||
        exponent !in TouchBindings.MIN_EXPONENT..TouchBindings.MAX_EXPONENT
    ) {
        return 0f
    }
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

internal const val MIN_TOUCH_ZONE_SIZE_PCT = 2f

internal fun normalizeFloatingZone(zone: FloatingZone): FloatingZone {
    fun orderedSpan(
        first: Float,
        second: Float,
    ): Pair<Float, Float> {
        val low = minOf(first, second).coerceIn(0f, 100f)
        val high = maxOf(first, second).coerceIn(0f, 100f)
        if (high - low >= MIN_TOUCH_ZONE_SIZE_PCT) return low to high
        val start = ((low + high - MIN_TOUCH_ZONE_SIZE_PCT) / 2f).coerceIn(0f, 100f - MIN_TOUCH_ZONE_SIZE_PCT)
        return start to start + MIN_TOUCH_ZONE_SIZE_PCT
    }

    val (left, right) = orderedSpan(zone.leftPct, zone.rightPct)
    val (top, bottom) = orderedSpan(zone.topPct, zone.bottomPct)
    return FloatingZone(left, top, right, bottom)
}

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
            normalizeFloatingZone(
                FloatingZone(
                    j.optDouble("left", 0.0).toFloat(),
                    j.optDouble("top", 0.0).toFloat(),
                    j.optDouble("right", 50.0).toFloat(),
                    j.optDouble("bottom", 100.0).toFloat(),
                ),
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

private fun validateFloatingZone(
    path: String,
    zone: FloatingZone,
): String? {
    val values = listOf(zone.leftPct, zone.topPct, zone.rightPct, zone.bottomPct)
    if (values.any { !it.isFinite() || it !in 0f..100f }) return "$path must stay within 0..100"
    if (zone.leftPct >= zone.rightPct || zone.topPct >= zone.bottomPct) return "$path must have positive area"
    return null
}

internal fun validateTouchLayoutDomains(layout: TouchLayout): String? {
    if (layout.version !in MIN_SUPPORTED_TOUCH_LAYOUT_VERSION..CURRENT_TOUCH_LAYOUT_VERSION) {
        return "touch_layout.version is unsupported"
    }

    fun binding(
        path: String,
        value: Int,
        optional: Boolean = false,
    ): String? {
        if (optional && value == -1) return null
        return if (TouchBindings.isSupportedBinding(value)) null else "$path is unsupported"
    }

    fun axis(
        path: String,
        value: Int,
        optional: Boolean = false,
    ): String? {
        if (optional && value == -1) return null
        return if (TouchBindings.isSupportedAxis(value)) null else "$path is unsupported"
    }

    fun base(
        path: String,
        x: Float,
        y: Float,
        size: Float,
        opacity: Float,
    ): String? {
        if (!x.isFinite() || x !in 0f..100f || !y.isFinite() || y !in 0f..100f) return "$path position is invalid"
        if (!size.isFinite() || size !in TouchBindings.MIN_SIZE..TouchBindings.MAX_SIZE) return "$path size is invalid"
        if (!opacity.isFinite() ||
            opacity !in TouchBindings.MIN_OPACITY..TouchBindings.MAX_OPACITY
        ) {
            return "$path opacity is invalid"
        }
        return null
    }

    fun response(
        path: String,
        exponent: Float,
        sensitivity: Float,
    ): String? {
        if (!exponent.isFinite() ||
            exponent !in TouchBindings.MIN_EXPONENT..TouchBindings.MAX_EXPONENT
        ) {
            return "$path exponent is invalid"
        }
        if (!sensitivity.isFinite() ||
            sensitivity !in TouchBindings.MIN_SENSITIVITY..TouchBindings.MAX_SENSITIVITY
        ) {
            return "$path sensitivity is invalid"
        }
        return null
    }

    if (!layout.globalOpacity.isFinite() ||
        layout.globalOpacity !in TouchBindings.MIN_GLOBAL_OPACITY..TouchBindings.MAX_GLOBAL_OPACITY
    ) {
        return "touch_layout.globalOpacity is invalid"
    }
    layout.sticks.forEachIndexed { index, stick ->
        val path = "touch_layout.sticks[$index]"
        axis("$path.axisX", stick.axisX)?.let { return it }
        axis("$path.axisY", stick.axisY)?.let { return it }
        if (stick.buttonMode) {
            binding("$path.negXBinding", stick.negXBinding)?.let { return it }
            binding("$path.posXBinding", stick.posXBinding)?.let { return it }
            binding("$path.negYBinding", stick.negYBinding)?.let { return it }
            binding("$path.posYBinding", stick.posYBinding)?.let { return it }
        }
        binding("$path.doubleTapBinding", stick.doubleTapBinding, optional = true)?.let { return it }
        base(path, stick.xPct, stick.yPct, stick.sizeMult, stick.opacity)?.let { return it }
        response(path, stick.exponent, stick.sensitivityX)?.let { return it }
        if (!stick.sensitivityY.isFinite() ||
            stick.sensitivityY !in TouchBindings.MIN_SENSITIVITY..TouchBindings.MAX_SENSITIVITY
        ) {
            return "$path sensitivityY is invalid"
        }
        if (stick.deadzone !in
            TouchBindings.MIN_DEADZONE..TouchBindings.MAX_DEADZONE
        ) {
            return "$path deadzone is invalid"
        }
        if (!stick.mouseExponentialMax.isFinite() ||
            stick.mouseExponentialMax !in 1f..10f
        ) {
            return "$path mouseExponentialMax is invalid"
        }
        validateFloatingZone("$path.floatingZone", stick.floatingZone)?.let { return it }
        stick.extremeActions.forEachIndexed { actionIndex, action ->
            binding("$path.extremeActions[$actionIndex].binding", action.binding)?.let { return it }
            if (!action.threshold.isFinite() ||
                action.threshold !in
                TouchBindings.MIN_STICK_EXTREME_THRESHOLD..TouchBindings.MAX_STICK_EXTREME_THRESHOLD
            ) {
                return "$path.extremeActions[$actionIndex].threshold is invalid"
            }
            val releaseMaximum = action.threshold - TouchBindings.MIN_STICK_EXTREME_HYSTERESIS
            val releaseRange = TouchBindings.MIN_STICK_EXTREME_RELEASE_THRESHOLD..releaseMaximum
            val releaseThresholdValid =
                action.releaseThreshold.isFinite() && action.releaseThreshold in releaseRange
            if (!releaseThresholdValid) {
                return "$path.extremeActions[$actionIndex].releaseThreshold is invalid"
            }
        }
    }
    layout.buttons.forEachIndexed { index, control ->
        binding("touch_layout.buttons[$index].binding", control.binding)?.let { return it }
        binding("touch_layout.buttons[$index].longPressBinding", control.longPressBinding, optional = true)?.let {
            return it
        }
        base(
            "touch_layout.buttons[$index]",
            control.xPct,
            control.yPct,
            control.sizeMult,
            control.opacity,
        )?.let { return it }
    }
    layout.sliders.forEachIndexed { index, control ->
        val path = "touch_layout.sliders[$index]"
        axis("$path.axis", control.axis)?.let { return it }
        base(path, control.xPct, control.yPct, control.sizeMult, control.opacity)?.let { return it }
        response(path, control.exponent, control.sensitivity)?.let { return it }
    }
    layout.radialMenus.forEachIndexed { index, control ->
        val path = "touch_layout.radialMenus[$index]"
        control.segments.forEachIndexed { segmentIndex, segment ->
            binding("$path.segments[$segmentIndex].binding", segment.binding)?.let { return it }
        }
        binding("$path.centerBinding", control.centerBinding, optional = true)?.let { return it }
        base(path, control.xPct, control.yPct, control.sizeMult, control.opacity)?.let { return it }
        if (!control.ringSizeMult.isFinite() ||
            control.ringSizeMult !in TouchBindings.MIN_SIZE..TouchBindings.MAX_SIZE
        ) {
            return "$path ringSize is invalid"
        }
        if (!control.stripDragSpanWidthPct.isFinite() ||
            control.stripDragSpanWidthPct !in 5f..80f
        ) {
            return "$path stripDragSpanWidthPct is invalid"
        }
        if (!control.stripLabelAngleDeg.isFinite() ||
            control.stripLabelAngleDeg !in -90f..90f
        ) {
            return "$path stripLabelAngleDeg is invalid"
        }
        if (!control.stripSelectedScale.isFinite() ||
            control.stripSelectedScale !in 1f..3f
        ) {
            return "$path stripSelectedScale is invalid"
        }
        if (!control.stripCardScale.isFinite() ||
            control.stripCardScale !in MIN_SCROLL_STRIP_CARD_SCALE..MAX_SCROLL_STRIP_CARD_SCALE
        ) {
            return "$path stripCardScale is invalid"
        }
    }
    layout.dpads.forEachIndexed { index, control ->
        val path = "touch_layout.dpads[$index]"
        binding("$path.up", control.upBinding)?.let { return it }
        binding("$path.down", control.downBinding)?.let { return it }
        binding("$path.left", control.leftBinding)?.let { return it }
        binding("$path.right", control.rightBinding)?.let { return it }
        base(path, control.xPct, control.yPct, control.sizeMult, control.opacity)?.let { return it }
        if (!control.swipeThreshold.isFinite() ||
            control.swipeThreshold !in TouchBindings.MIN_SWIPE_THRESHOLD..TouchBindings.MAX_SWIPE_THRESHOLD
        ) {
            return "$path swipeThreshold is invalid"
        }
    }
    layout.diagnostics.forEachIndexed { index, control ->
        base("touch_layout.diagnostics[$index]", control.xPct, control.yPct, control.sizeMult, control.opacity)?.let {
            return it
        }
    }
    layout.axisRegions.forEachIndexed { index, control ->
        val path = "touch_layout.axisRegions[$index]"
        axis("$path.axis", control.axis)?.let { return it }
        validateFloatingZone("$path.zone", control.zone)?.let { return it }
        response(path, control.exponent, control.sensitivity)?.let { return it }
        if (!control.opacity.isFinite() ||
            control.opacity !in TouchBindings.MIN_OPACITY..TouchBindings.MAX_OPACITY
        ) {
            return "$path opacity is invalid"
        }
    }
    base(
        "touch_layout.moreActions",
        layout.moreActions.xPct,
        layout.moreActions.yPct,
        layout.moreActions.sizeMult,
        layout.moreActions.opacity,
    )?.let {
        return it
    }
    val gyro = layout.gyro
    axis("touch_layout.gyro.axisX", gyro.axisX)?.let { return it }
    axis("touch_layout.gyro.axisY", gyro.axisY)?.let { return it }
    axis("touch_layout.gyro.axisZ", gyro.axisZ, optional = true)?.let { return it }
    for ((name, value) in listOf(
        "deadzone" to gyro.deadzone,
        "deadzoneX" to gyro.deadzoneX,
        "deadzoneY" to gyro.deadzoneY,
        "deadzoneZ" to gyro.deadzoneZ,
    )) {
        if (!value.isFinite() || value !in 0f..0.6f) return "touch_layout.gyro.$name is invalid"
    }
    for ((name, value) in listOf(
        "maxAngleX" to gyro.maxAngleX,
        "maxAngleY" to gyro.maxAngleY,
        "maxAngleZ" to gyro.maxAngleZ,
    )) {
        if (!value.isFinite() || value !in 0.1f..1.57f) return "touch_layout.gyro.$name is invalid"
    }
    for ((name, value) in listOf(
        "refAzimuth" to gyro.refAzimuth,
        "refPitch" to gyro.refPitch,
        "refRoll" to gyro.refRoll,
    )) {
        if (value != null && !value.isFinite()) return "touch_layout.gyro.$name must be finite"
    }
    return null
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
    val stripSelectedScale: Float = DEFAULT_SCROLL_STRIP_SELECTED_SCALE,
    val stripCardScale: Float = DEFAULT_SCROLL_STRIP_CARD_SCALE,
    val stripRowOffset: ScrollStripRowOffset = ScrollStripRowOffset.ABOVE_LEFT,
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
            put("stripCardScale", stripCardScale.toDouble())
            put("stripRowOffset", stripRowOffset.name)
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
                stripSelectedScale =
                    j.optDouble("stripSelectedScale", DEFAULT_SCROLL_STRIP_SELECTED_SCALE.toDouble()).toFloat(),
                stripCardScale =
                    j
                        .optDouble("stripCardScale", DEFAULT_SCROLL_STRIP_CARD_SCALE.toDouble())
                        .toFloat()
                        .coerceIn(MIN_SCROLL_STRIP_CARD_SCALE, MAX_SCROLL_STRIP_CARD_SCALE),
                stripRowOffset =
                    runCatching {
                        ScrollStripRowOffset.valueOf(
                            j.optString("stripRowOffset", ScrollStripRowOffset.ABOVE_LEFT.name),
                        )
                    }.getOrDefault(ScrollStripRowOffset.ABOVE_LEFT),
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
            validateTouchLayoutJsonNumbers(j)?.let { throw IllegalArgumentException(it) }

            fun <T> parseArray(
                key: String,
                parse: (JSONObject) -> T,
            ): List<T> {
                val arr = j.optJSONArray(key) ?: return emptyList()
                return (0 until arr.length()).map { parse(arr.getJSONObject(it)) }
            }
            val layout =
                TouchLayout(
                    version = j.optInt("version", 1),
                    name = j.optString("name", "Default"),
                    globalOpacity =
                        j
                            .optDouble(
                                "globalOpacity",
                                TouchBindings.DEFAULT_GLOBAL_OPACITY.toDouble(),
                            ).toFloat(),
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
            validateTouchLayoutDomains(layout)?.let { throw IllegalArgumentException(it) }
            return layout
        }
    }
}
