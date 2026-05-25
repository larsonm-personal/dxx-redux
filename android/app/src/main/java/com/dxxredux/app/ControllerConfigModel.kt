package com.dxxredux.app

import android.content.Context
import org.json.JSONObject

// Virtual button indices for each physical button control.
internal val BUTTON_CONTROLS =
    linkedMapOf(
        "A" to 0,
        "B" to 1,
        "X" to 2,
        "Y" to 3,
        "L1" to 4,
        "R1" to 5,
        "Select" to 6,
        "Start" to 7,
        "L3" to 8,
        "R3" to 9,
        "LT" to 19,
        "RT" to 21,
    )

// Virtual axis indices for each physical stick/trigger axis.
internal val AXIS_CONTROLS =
    linkedMapOf(
        "LS_X" to 0,
        "LS_Y" to 1,
        "RS_X" to 2,
        "RS_Y" to 3,
        "LT" to 4,
        "RT" to 5,
    )

// Axis-button SDL indices: axis -> (negative button, positive button).
// These match the layout from joy.c axis_button_map registration.
internal val AXIS_BUTTON_SDL =
    mapOf(
        "LS_X" to Pair(10, 11),
        "LS_Y" to Pair(12, 13),
        "RS_X" to Pair(14, 15),
        "RS_Y" to Pair(16, 17),
        "LT" to Pair(18, 19),
        "RT" to Pair(20, 21),
    )

/*
 * Joystick function -> kc_joystick[] index maps.
 * IMPORTANT: These indices mirror the kc_joystick[] array layout in
 * d2/main/kconfig.c and d1/main/kconfig.c (primary BT_JOY_BUTTON / BT_JOY_AXIS slots).
 * D1 has 48 entries, D2 has 56. Key differences: Automap is at index 27 in D1
 * but index 50 in D2; D2 has Afterburner/Headlight/Energy-Shield/Toggle Bomb
 * which don't exist in D1; Cycle Primary/Secondary are at 44/45 in D1 but 28/29 in D2.
 * Update both locations together. Search for "BUTTON_KC_INDEX" in kconfig.h.
 *
 * Axis entries: the axis value is at the listed index; the invert flag is at index+1.
 * Invert indices (14, 16, 18, 20, 22, 24) default to 0 in the C fill function.
 */
private val D2_BUTTON_KC_INDEX =
    linkedMapOf(
        "Fire Primary" to 0,
        "Fire Secondary" to 1,
        "Accelerate" to 2,
        "Reverse" to 3,
        "Fire Flare" to 4,
        "Slide On" to 5,
        "Slide Left" to 6,
        "Slide Right" to 7,
        "Slide Up" to 8,
        "Slide Down" to 9,
        "Bank On" to 10,
        "Bank Left" to 11,
        "Bank Right" to 12,
        "Rear View" to 25,
        "Drop Bomb" to 26,
        "Afterburner" to 27,
        "Cycle Primary" to 28,
        "Cycle Secondary" to 29,
        "Headlight" to 30,
        "Automap" to 50,
        "Energy\u2192Shield" to 52,
        "Toggle Bomb" to 54,
    )

private val D1_BUTTON_KC_INDEX =
    linkedMapOf(
        "Fire Primary" to 0,
        "Fire Secondary" to 1,
        "Accelerate" to 2,
        "Reverse" to 3,
        "Fire Flare" to 4,
        "Slide On" to 5,
        "Slide Left" to 6,
        "Slide Right" to 7,
        "Slide Up" to 8,
        "Slide Down" to 9,
        "Bank On" to 10,
        "Bank Left" to 11,
        "Bank Right" to 12,
        "Rear View" to 25,
        "Drop Bomb" to 26,
        "Automap" to 27,
        "Cycle Primary" to 44,
        "Cycle Secondary" to 45,
    )

internal fun buttonKcIndex(gameVariant: String): LinkedHashMap<String, Int> =
    if (gameVariant == "d1") D1_BUTTON_KC_INDEX else D2_BUTTON_KC_INDEX

// Default to D2 for contexts where gameVariant is unknown.
private val BUTTON_KC_INDEX = D2_BUTTON_KC_INDEX

internal val AXIS_KC_INDEX =
    linkedMapOf(
        "Pitch U/D" to 13,
        "Turn L/R" to 15,
        "Slide L/R" to 17,
        "Slide U/D" to 19,
        "Bank L/R" to 21,
        "Throttle" to 23,
    )

/*
 * Keyboard function -> kc_keyboard[] secondary-slot index.
 * IMPORTANT: These mirror the odd-indexed entries of kc_keyboard[] in
 * d2/main/kconfig.c. Update both locations together.
 */
private val KB_KC_INDEX =
    linkedMapOf(
        "Pitch Forward" to 1,
        "Pitch Backward" to 3,
        "Turn Left" to 5,
        "Turn Right" to 7,
        "Slide Left" to 11,
        "Slide Right" to 13,
        "Slide Up" to 15,
        "Slide Down" to 17,
        "Bank Left" to 21,
        "Bank Right" to 23,
        "Fire Primary" to 25,
        "Fire Secondary" to 27,
        "Fire Flare" to 29,
        "Accelerate" to 31,
        "Reverse" to 33,
        "Drop Bomb" to 35,
        "Rear View" to 37,
        "Automap" to 45,
        "Afterburner" to 47,
        "Cycle Primary" to 49,
        "Cycle Secondary" to 51,
        "Headlight" to 53,
        "Energy\u2192Shield" to 55,
        "Toggle Bomb" to 56,
    )

internal val BUTTON_FUNCTIONS = BUTTON_KC_INDEX.keys.toList()
internal val AXIS_FUNCTIONS = AXIS_KC_INDEX.keys.toList()

// D-pad direction -> virtual joystick button index (must match DPAD_BUTTON_BASE in joy.c).
internal val DPAD_CONTROLS =
    linkedMapOf(
        "DUp" to 22,
        "DDown" to 23,
        "DLeft" to 24,
        "DRight" to 25,
    )

internal val KB_FUNCTIONS = KB_KC_INDEX.keys.toList()

internal fun loadDefaultBindings(context: Context): Map<String, String> =
    try {
        val json =
            context.assets.open("configs/controller/default.json").bufferedReader().use {
                JSONObject(it.readText())
            }
        val result = HumanReadableConfig.humanJsonToControllerConfig(json)
        result.warnings.forEach { android.util.Log.w("ControllerConfig", it) }
        result.value?.bindings ?: emptyMap()
    } catch (e: Exception) {
        android.util.Log.e("ControllerConfig", "Failed to load default bindings", e)
        emptyMap()
    }

internal fun hasControllerMenuBinding(bindings: Map<String, String>): Boolean =
    bindings.values.any { TouchBindings.metaActionIdForLabel(it) == TouchBindings.META_MENU_CYCLE }

// Default axis-to-button activation threshold (percentage, 5-95).
const val DEFAULT_AXIS_THRESHOLD = 30
const val DEFAULT_STICK_DEAD_ZONE = 10
const val DEFAULT_CONTROLLER_AXIS_EXPONENT = 1.0f

// Axis IDs that support per-axis thresholds.
private val THRESHOLD_AXES = listOf("LS_X", "LS_Y", "RS_X", "RS_Y", "LT", "RT")
private val STICK_THRESHOLD_AXES = setOf("LS_X", "LS_Y", "RS_X", "RS_Y")

private fun defaultThresholdForAxis(axis: String): Int =
    if (axis in STICK_THRESHOLD_AXES) {
        DEFAULT_STICK_DEAD_ZONE
    } else {
        DEFAULT_AXIS_THRESHOLD
    }

internal fun thresholdForDialog(
    axis: String,
    buttonMode: Boolean,
    thresholds: Map<String, Int>,
): Int = thresholds[axis] ?: if (buttonMode) DEFAULT_AXIS_THRESHOLD else defaultThresholdForAxis(axis)

// Build a default thresholds map with per-axis defaults.
internal fun defaultThresholds(): Map<String, Int> = THRESHOLD_AXES.associateWith(::defaultThresholdForAxis)

internal fun clampControllerAxisExponent(value: Float): Float =
    if (value.isFinite()) {
        value.coerceIn(TouchBindings.MIN_EXPONENT, TouchBindings.MAX_EXPONENT)
    } else {
        DEFAULT_CONTROLLER_AXIS_EXPONENT
    }

internal fun defaultControllerAxisExponents(): Map<String, Float> =
    THRESHOLD_AXES.associateWith { DEFAULT_CONTROLLER_AXIS_EXPONENT }

internal fun clampedControllerAxisExponents(values: Map<String, Float>): Map<String, Float> {
    val result = defaultControllerAxisExponents().toMutableMap()
    for ((axis, value) in values) {
        if (axis in result) result[axis] = clampControllerAxisExponent(value)
    }
    return result
}

internal fun applyControllerAxisExponent(
    value: Float,
    exponent: Float,
): Float = applyResponseCurve(value, ResponseCurve.EXPONENTIAL, clampControllerAxisExponent(exponent))

// Axis functions that implicitly cover discrete button functions.
internal val AXIS_COVERS_BUTTONS =
    mapOf(
        "Pitch U/D" to listOf("Pitch Up", "Pitch Down"),
        "Turn L/R" to listOf("Turn Left", "Turn Right"),
        "Slide L/R" to listOf("Slide Left", "Slide Right"),
        "Slide U/D" to listOf("Slide Up", "Slide Down"),
        "Bank L/R" to listOf("Bank Left", "Bank Right"),
        "Throttle" to listOf("Accelerate", "Reverse"),
    )

// Half-axis map: button function -> (axis function, isPositive).
// isPositive means this button corresponds to the positive half of the
// axis under the non-inverted sign convention in kconfig_read_controls().
// Shared constant: sign conventions must match kconfig.c axis handling
// (see pitch_time, heading_time, vertical_thrust_time, sideways_thrust_time,
// bank_time, forward_thrust_time).
internal val HALF_AXIS_MAP =
    mapOf(
        "Pitch Up" to Pair("Pitch U/D", false),
        "Pitch Down" to Pair("Pitch U/D", true),
        "Turn Left" to Pair("Turn L/R", false),
        "Turn Right" to Pair("Turn L/R", true),
        "Slide Up" to Pair("Slide U/D", true),
        "Slide Down" to Pair("Slide U/D", false),
        "Slide Right" to Pair("Slide L/R", true),
        "Slide Left" to Pair("Slide L/R", false),
        "Bank Left" to Pair("Bank L/R", false),
        "Bank Right" to Pair("Bank L/R", true),
        "Accelerate" to Pair("Throttle", false),
        "Reverse" to Pair("Throttle", true),
    )

// Ordered list of half-axis options shown in the trigger picker dialog.
// These are direction-specific names for single-direction trigger assignment.
internal val TRIGGER_HALF_AXIS_OPTIONS = HALF_AXIS_MAP.keys.toList()

internal fun assignButtonFunction(
    bindings: MutableMap<String, String>,
    controlId: String,
    funcLabel: String?,
) {
    if (funcLabel == null) {
        bindings.remove(controlId)
        return
    }
    val existing =
        bindings.entries.find {
            it.value == funcLabel && it.key != controlId && it.key in BUTTON_CONTROLS
        }
    if (existing != null) bindings.remove(existing.key)
    bindings[controlId] = funcLabel
}

internal fun assignAxisFunction(
    bindings: MutableMap<String, String>,
    axisControlId: String,
    funcLabel: String?,
) {
    if (funcLabel == null) {
        bindings.remove(axisControlId)
        return
    }
    val existing =
        bindings.entries.find {
            it.value == funcLabel && it.key != axisControlId && it.key in AXIS_CONTROLS
        }
    if (existing != null) bindings.remove(existing.key)
    bindings[axisControlId] = funcLabel
}

internal fun assignDpadFunction(
    bindings: MutableMap<String, String>,
    controlId: String,
    funcLabel: String?,
) {
    if (funcLabel == null) {
        bindings.remove(controlId)
        return
    }
    val existing =
        bindings.entries.find {
            it.value == funcLabel && it.key != controlId && it.key in DPAD_CONTROLS
        }
    if (existing != null) bindings.remove(existing.key)
    bindings[controlId] = funcLabel
}
