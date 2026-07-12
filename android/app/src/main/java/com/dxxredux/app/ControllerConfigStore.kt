package com.dxxredux.app

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

internal data class JoyPairsResult(
    val indices: IntArray,
    val values: IntArray,
    val combiners: List<Triple<Int, Int, Int>>,
)

internal data class ControllerConfigState(
    val bindings: Map<String, String> = emptyMap(),
    val inverts: Set<String> = emptySet(),
    val thresholds: Map<String, Int> = defaultThresholds(),
    val axisExponents: Map<String, Float> = defaultControllerAxisExponents(),
)

internal fun controllerConfigStateFromHumanData(data: HumanReadableConfig.ControllerConfigData): ControllerConfigState {
    val thresholds = defaultThresholds().toMutableMap()
    for ((controlId, threshold) in data.thresholds) {
        if (controlId in thresholds) thresholds[controlId] = threshold.coerceIn(5, 95)
    }
    val axisExponents = clampedControllerAxisExponents(data.axisExponents)
    return ControllerConfigState(data.bindings, data.inverts, thresholds, axisExponents)
}

internal fun controllerConfigStateFromHumanJson(
    json: JSONObject,
): HumanReadableConfig.ParseResult<ControllerConfigState> {
    val parsed = HumanReadableConfig.humanJsonToControllerConfig(json)
    return HumanReadableConfig.ParseResult(parsed.value?.let(::controllerConfigStateFromHumanData), parsed.warnings)
}

internal fun controllerConfigStateToHumanJson(config: ControllerConfigState): JSONObject =
    HumanReadableConfig.controllerConfigToHumanJson(
        config.bindings,
        config.inverts,
        config.thresholds,
        config.axisExponents,
    )

internal fun readActiveControllerConfig(context: Context): ControllerConfigState? {
    val file = File(context.filesDir, CONFIG_FILENAME)
    if (!file.exists()) return null
    return try {
        controllerConfigStateFromHumanJson(JSONObject(file.readText())).value
    } catch (_: Exception) {
        null
    }
}

internal fun loadDefaultControllerConfig(context: Context): ControllerConfigState =
    try {
        val json =
            JSONObject(
                context.assets.open("configs/controller/default.json").bufferedReader().use { reader ->
                    reader.readText()
                },
            )
        val parsed = controllerConfigStateFromHumanJson(json)
        parsed.warnings.forEach { android.util.Log.w("ControllerConfig", it) }
        parsed.value ?: ControllerConfigState()
    } catch (exception: Exception) {
        android.util.Log.e("ControllerConfig", "Failed to load default controller config", exception)
        ControllerConfigState()
    }

private const val D1_JOY_SETTINGS_SIZE = 50
private const val D2_JOY_SETTINGS_SIZE = 56

internal fun buildJoySettingsArray(
    result: JoyPairsResult,
    variant: String,
): ByteArray {
    val size = if (variant == "d1") D1_JOY_SETTINGS_SIZE else D2_JOY_SETTINGS_SIZE
    val settings = ByteArray(size) { 0xFF.toByte() }

    // Invert slots default to 0; all other slots default to 0xFF.
    for (invertIndex in AXIS_KC_INDEX.values.map { it + 1 }) {
        if (invertIndex < size) settings[invertIndex] = 0
    }

    for (i in result.indices.indices) {
        val index = result.indices[i]
        if (index in 0 until size) settings[index] = (result.values[i] and 0xFF).toByte()
    }

    return settings
}

private const val CONFIG_FILENAME = "controller_config.json"

// Bump when the config format changes to force regeneration from defaults.
// SetupActivity.writeDefaultControllerConfig checks this on startup.
internal const val CONTROLLER_CONFIG_VERSION = 4

private const val VIRTUAL_AXIS_BASE = 8

// Col1 -> Col2 kc_joystick index map for BT_JOY_BUTTON entries.
// Must match kc_joystick[] layout in d2/main/kconfig.c / d1/main/kconfig.c.
// Used to place secondary button bindings (e.g., d-pad) when col1 is already taken.
private val D2_COL2_MAP =
    mapOf(
        0 to 31,
        1 to 32,
        2 to 33,
        3 to 34,
        4 to 35,
        5 to 36,
        6 to 37,
        7 to 38,
        8 to 39,
        9 to 40,
        10 to 41,
        11 to 42,
        12 to 43,
        25 to 44,
        26 to 45,
        27 to 46,
        28 to 47,
        29 to 48,
        30 to 49,
        50 to 51,
        52 to 53,
        54 to 55,
    )

private val D1_COL2_MAP =
    mapOf(
        0 to 29,
        1 to 30,
        2 to 31,
        3 to 32,
        4 to 33,
        5 to 34,
        6 to 35,
        7 to 36,
        8 to 37,
        9 to 38,
        10 to 39,
        11 to 40,
        12 to 41,
        25 to 42,
        26 to 43,
        27 to 28,
        44 to 46,
        45 to 47,
    )

/**
 * Collect (kc_index, value) pairs for joystick settings from human-readable
 * bindings using the given game variant's kconfig index mapping.
 */
internal fun buildJoyPairs(
    bindings: Map<String, String>,
    inverts: Set<String>,
    variant: String,
): JoyPairsResult {
    val btnKcMap = buttonKcIndex(variant)
    val indices = mutableListOf<Int>()
    val values = mutableListOf<Int>()

    // Detect trigger axes bound to half-axis-eligible button functions.
    // These get combined into virtual axes for proportional control.
    // Skip if the same axis function already has a direct full-axis binding
    // (e.g. LS_Y -> Throttle takes priority over RT/LT -> Accel/Reverse).
    data class HalfAxisEntry(
        val sourceAxis: Int,
        val isPositive: Boolean,
    )

    // Pre-scan: which axis functions have a direct full-axis binding?
    val directAxisFuncs = mutableSetOf<String>()
    for ((controlId, funcLabel) in bindings) {
        if (AXIS_CONTROLS.containsKey(controlId) && AXIS_KC_INDEX.containsKey(funcLabel)) {
            directAxisFuncs.add(funcLabel)
        }
    }

    val halfAxisGroups = mutableMapOf<String, MutableList<HalfAxisEntry>>()
    val handledAsHalfAxis = mutableSetOf<String>()

    for ((controlId, funcLabel) in bindings) {
        val ha = HALF_AXIS_MAP[funcLabel] ?: continue
        if (ha.first in directAxisFuncs) continue
        val axisSdlId = AXIS_CONTROLS[controlId] ?: continue
        halfAxisGroups
            .getOrPut(ha.first) { mutableListOf() }
            .add(HalfAxisEntry(axisSdlId, ha.second))
        handledAsHalfAxis.add(controlId)
    }

    var nextVirtual = VIRTUAL_AXIS_BASE
    val combiners = mutableListOf<Triple<Int, Int, Int>>()
    for ((axisFunc, entries) in halfAxisGroups) {
        val kcIdx = AXIS_KC_INDEX[axisFunc] ?: continue
        val virtualAxis = nextVirtual++
        indices.add(kcIdx)
        values.add(virtualAxis)
        indices.add(kcIdx + 1)
        values.add(0) // not inverted
        val posSource = entries.firstOrNull { it.isPositive }?.sourceAxis ?: -1
        val negSource = entries.firstOrNull { !it.isPositive }?.sourceAxis ?: -1
        combiners.add(Triple(virtualAxis, posSource, negSource))
    }

    // Set identity values for all col1 button slots so the mixer can
    // activate any action via MIXER_BTN_BASE + kc_index. The InputMixer
    // handles the physical-button-to-action translation in Kotlin.
    for (kcIdx in btnKcMap.values) {
        indices.add(kcIdx)
        values.add(TouchBindings.MIXER_BTN_BASE + kcIdx)
    }

    // Normal button and axis bindings (skip those handled as half-axis).
    for ((controlId, funcLabel) in bindings) {
        if (controlId in handledAsHalfAxis) continue
        val btnKcIdx = btnKcMap[funcLabel]
        // Face buttons: skip, identity mapping + mixer_button_map handles them.
        // Exclude RT/LT (axis controls that also appear in BUTTON_CONTROLS) --
        // their primary input comes via axis events, not key events.
        if (btnKcIdx != null && BUTTON_CONTROLS[controlId] != null && !AXIS_CONTROLS.containsKey(controlId)) {
            continue
        }
        val axisKcIdx = AXIS_KC_INDEX[funcLabel]
        val axisSdlId = AXIS_CONTROLS[controlId]
        if (axisKcIdx != null && axisSdlId != null) {
            indices.add(axisKcIdx)
            values.add(axisSdlId)
            indices.add(axisKcIdx + 1)
            values.add(if (controlId in inverts) 1 else 0)
            continue
        }
        // Axis control bound to a button function (e.g., RT -> Fire Primary).
        // Use the positive axis-button SDL index so the C engine's
        // axis-button handler can match it via col2 after deduplication.
        if (btnKcIdx != null && axisSdlId != null) {
            val sdlPair = AXIS_BUTTON_SDL[controlId]
            if (sdlPair != null) {
                indices.add(btnKcIdx)
                values.add(sdlPair.second) // positive direction
            }
            continue
        }
        val isNeg = controlId.endsWith("_neg")
        val isPos = controlId.endsWith("_pos")
        if ((isNeg || isPos) && btnKcIdx != null) {
            val axisId = controlId.removeSuffix("_neg").removeSuffix("_pos")
            val sdlPair = AXIS_BUTTON_SDL[axisId]
            if (sdlPair != null) {
                val sdlBtn = if (isNeg) sdlPair.first else sdlPair.second
                indices.add(btnKcIdx)
                values.add(sdlBtn)
            }
        }
    }

    // Default gyro axis bindings (matches C fallback in android_apply_gamepad_defaults).
    // When no explicit binding targets Slide U/D or Bank L/R axes, use gyro.
    for ((kcIdx, axisVal) in arrayOf(19 to 7, 21 to 6)) {
        if (kcIdx !in indices) {
            indices.add(kcIdx)
            values.add(axisVal)
        }
    }

    // Safety net: mutual exclusivity between axis and button bindings.
    // When a physical axis is used as an axis (half or full), remove any
    // button bindings that reference its synthetic axis-button indices.
    // This prevents stale pilot files from retaining button-fire actions
    // for triggers that were remapped to axis functions.
    val poisonedButtons = mutableSetOf<Int>()
    for (controlId in handledAsHalfAxis) {
        AXIS_BUTTON_SDL[controlId]?.let { (neg, pos) ->
            poisonedButtons.add(neg)
            poisonedButtons.add(pos)
        }
    }
    for ((controlId, funcLabel) in bindings) {
        if (controlId in handledAsHalfAxis) continue
        if (AXIS_CONTROLS.containsKey(controlId) && AXIS_KC_INDEX.containsKey(funcLabel)) {
            AXIS_BUTTON_SDL[controlId]?.let { (neg, pos) ->
                poisonedButtons.add(neg)
                poisonedButtons.add(pos)
            }
        }
    }
    if (poisonedButtons.isNotEmpty()) {
        val btnKcSlots = btnKcMap.values.toSet()
        var i = indices.size - 1
        while (i >= 0) {
            if (indices[i] in btnKcSlots && values[i] in poisonedButtons) {
                indices.removeAt(i)
                values.removeAt(i)
            }
            i--
        }
    }

    // Deduplicate: when two bindings target the same kc index (e.g., a face
    // button and a d-pad both bound to Slide Up), redirect the second to the
    // col2 (secondary) kc index. Touch overlay no longer uses col2 -- it
    // uses implicit index-based matching in kconfig.c instead.
    val col2Map = if (variant == "d1") D1_COL2_MAP else D2_COL2_MAP
    val assigned = mutableSetOf<Int>()
    var i2 = 0
    while (i2 < indices.size) {
        val idx = indices[i2]
        if (idx in assigned) {
            val col2 = col2Map[idx]
            if (col2 != null) {
                indices[i2] = col2
                assigned.add(col2)
            } else {
                indices.removeAt(i2)
                values.removeAt(i2)
                continue
            }
        } else {
            assigned.add(idx)
        }
        i2++
    }

    return JoyPairsResult(indices.toIntArray(), values.toIntArray(), combiners)
}

internal fun saveConfig(
    context: Context,
    bindings: Map<String, String>,
    inverts: Set<String>,
    gameVariant: String = "d2",
    thresholds: Map<String, Int> = defaultThresholds(),
    axisExponents: Map<String, Float> = defaultControllerAxisExponents(),
) {
    val d1Result = buildJoyPairs(bindings, inverts, "d1")
    val d2Result = buildJoyPairs(bindings, inverts, "d2")
    val d1JoySettings = NativePilotPatcher.nativeBuildJoySettings(d1Result.indices, d1Result.values, "d1")
    val d2JoySettings = NativePilotPatcher.nativeBuildJoySettings(d2Result.indices, d2Result.values, "d2")
    val joySettings = if (gameVariant == "d1") d1JoySettings else d2JoySettings

    val activeSettings = joySettings
    val btnSummary = (0..12).joinToString { i -> "$i=${activeSettings[i].toInt() and 0xFF}" }
    android.util.Log.d("ControllerConfig", "saveConfig[$gameVariant] btn: $btnSummary")
    val axisSummary =
        listOf(13, 15, 17, 19, 21, 23).joinToString { i ->
            "$i=${activeSettings[i].toInt() and 0xFF}"
        }
    android.util.Log.d("ControllerConfig", "saveConfig[$gameVariant] axis: $axisSummary")

    val combiners = d2Result.combiners
    val kbIndices = mutableListOf<Int>()
    val kbValues = mutableListOf<Int>()
    val kbSettings =
        NativePilotPatcher.nativeBuildKbSettings(
            kbIndices.toIntArray(),
            kbValues.toIntArray(),
            gameVariant,
        )
    val controlType = 1 // CONTROL_USING_JOYSTICK

    val json = JSONObject()
    json.put("version", CONTROLLER_CONFIG_VERSION)
    json.put("control_type", controlType)
    json.put("automap_free_flight", 1)

    val bindingsObj = JSONObject()
    for ((k, v) in bindings) bindingsObj.put(k, v)
    json.put("bindings", bindingsObj)

    val invertsArr = JSONArray()
    for (inv in inverts) invertsArr.put(inv)
    json.put("inverts", invertsArr)

    val metaObj = JSONObject()
    for ((controlId, funcLabel) in bindings) {
        val metaId = TouchBindings.metaActionIdForLabel(funcLabel)
        if (metaId < 0) continue
        val sdlBtn = BUTTON_CONTROLS[controlId]
        if (sdlBtn != null) {
            metaObj.put(sdlBtn.toString(), metaId)
            continue
        }
        val dpadCode = DPAD_CONTROLS[controlId]
        if (dpadCode != null) {
            metaObj.put("dpad_$controlId", metaId)
            continue
        }
        val isNeg = controlId.endsWith("_neg")
        val isPos = controlId.endsWith("_pos")
        if (isNeg || isPos) {
            val axisId = controlId.removeSuffix("_neg").removeSuffix("_pos")
            val sdlPair = AXIS_BUTTON_SDL[axisId]
            if (sdlPair != null) {
                val abSdlBtn = if (isNeg) sdlPair.first else sdlPair.second
                metaObj.put(abSdlBtn.toString(), metaId)
            }
        }
    }
    json.put("meta_bindings", metaObj)

    val d1JoyArr = JSONArray()
    for (b in d1JoySettings) d1JoyArr.put(b.toInt() and 0xFF)
    val d2JoyArr = JSONArray()
    for (b in d2JoySettings) d2JoyArr.put(b.toInt() and 0xFF)
    json.put("key_settings_joystick_d1", d1JoyArr)
    json.put("key_settings_joystick_d2", d2JoyArr)

    val kbArr = JSONArray()
    for (b in kbSettings) kbArr.put(b.toInt() and 0xFF)
    json.put("key_settings_keyboard", kbArr)

    val thresholdsObj = JSONObject()
    for ((axis, pct) in thresholds) thresholdsObj.put(axis, pct)
    json.put("thresholds", thresholdsObj)

    val exponentsObj = JSONObject()
    for ((axis, exponent) in clampedControllerAxisExponents(axisExponents)) {
        exponentsObj.put(axis, exponent.toDouble())
    }
    json.put("axis_exponents", exponentsObj)

    if (combiners.isNotEmpty()) {
        val combArr = JSONArray()
        for ((virt, pos, neg) in combiners) {
            val entry = JSONArray()
            entry.put(virt)
            entry.put(pos)
            entry.put(neg)
            combArr.put(entry)
        }
        json.put("half_axis_combiners", combArr)
    }

    fun buildMixerButtonMap(variant: String): JSONObject {
        val kcMap = buttonKcIndex(variant)
        val result = JSONObject()
        for ((controlId, funcLabel) in bindings) {
            val kcIdx = kcMap[funcLabel] ?: continue
            val sdlBtn = BUTTON_CONTROLS[controlId]
            if (sdlBtn != null) {
                val arr =
                    result.optJSONArray(sdlBtn.toString())
                        ?: JSONArray().also { result.put(sdlBtn.toString(), it) }
                arr.put(kcIdx)
                continue
            }
            val dpadBtn = DPAD_CONTROLS[controlId]
            if (dpadBtn != null) {
                val arr =
                    result.optJSONArray(dpadBtn.toString())
                        ?: JSONArray().also { result.put(dpadBtn.toString(), it) }
                arr.put(kcIdx)
                continue
            }
        }
        return result
    }
    json.put("mixer_button_map_d1", buildMixerButtonMap("d1"))
    json.put("mixer_button_map_d2", buildMixerButtonMap("d2"))

    File(context.filesDir, CONFIG_FILENAME).writeText(json.toString(2))

    NativePilotPatcher.nativePatchPilotFiles(
        context.filesDir.absolutePath,
        joySettings,
        kbSettings,
        controlType,
        gameVariant,
    )
}

internal fun saveConfig(
    context: Context,
    config: ControllerConfigState,
    gameVariant: String = "d2",
) {
    saveConfig(
        context,
        config.bindings,
        config.inverts,
        gameVariant,
        config.thresholds,
        config.axisExponents,
    )
}

internal data class LoadedConfig(
    val bindings: Map<String, String>,
    val inverts: Set<String>,
    val thresholds: Map<String, Int>,
    val axisExponents: Map<String, Float>,
)

internal fun loadConfig(context: Context): LoadedConfig? {
    val config = readActiveControllerConfig(context) ?: return null
    return LoadedConfig(config.bindings, config.inverts, config.thresholds, config.axisExponents)
}
