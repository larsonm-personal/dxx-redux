package com.dxxredux.app

import android.content.Context
import android.content.res.Configuration
import android.view.InputDevice
import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import kotlin.math.abs
import kotlin.math.min
import kotlin.math.sqrt

// ── Colors ──────────────────────────────────────────────────────────────────
private val cOutline = Color(0xFF9E9E9E)
private val cFill = Color(0xFF2A2A2A)
private val cActive = Color(0xFF4CAF50)
private val cInactive = Color(0xFF555555)
private val cStickBg = Color(0xFF333333)
private val cStickDot = Color(0xFF888888)
private val cStickDotActive = Color(0xFF66BB6A)
private val cLabel = Color(0xFFBBBBBB)
private val cTriggerBg = Color(0xFF333333)
private val cTriggerFill = Color(0xFF66BB6A)
private val cDpadBg = Color(0xFF333333)
private val cDpadActive = Color(0xFF4CAF50)
private val cPhoneBody = Color(0xFF1A1A1A)
private val cPhoneScreen = Color(0xFF0D47A1)
private val cAssignLabel = Color(0xFFFFAB40) // amber for function labels
private val cHighlight = Color(0x44FFFF00) // translucent yellow for selection

// Shared sizing for picker dialog radio rows
private val PICKER_RADIO_SIZE = 24.dp
private val PICKER_RADIO_GAP = 4.dp
private val PICKER_FONT_SIZE = 13.sp

// ── Data Model ──────────────────────────────────────────────────────────────

// Virtual button indices for each physical button control
private val BUTTON_CONTROLS =
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

// Virtual axis indices for each physical stick/trigger axis
private val AXIS_CONTROLS =
    linkedMapOf(
        "LS_X" to 0,
        "LS_Y" to 1,
        "RS_X" to 2,
        "RS_Y" to 3,
        "LT" to 4,
        "RT" to 5,
    )

// Axis-button SDL indices: axis → (negative button, positive button)
// These match the layout from joy.c axis_button_map registration.
private val AXIS_BUTTON_SDL =
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
 * Update both locations together.  Search for "BUTTON_KC_INDEX" in kconfig.h.
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

// Default to D2 for contexts where gameVariant is unknown
private val BUTTON_KC_INDEX = D2_BUTTON_KC_INDEX

private val AXIS_KC_INDEX =
    linkedMapOf(
        "Pitch U/D" to 13,
        "Turn L/R" to 15,
        "Slide L/R" to 17,
        "Slide U/D" to 19,
        "Bank L/R" to 21,
        "Throttle" to 23,
    )

/*
 * Keyboard function → kc_keyboard[] secondary-slot index.
 * IMPORTANT: These mirror the odd-indexed entries of kc_keyboard[] in
 * d2/main/kconfig.c.  Update both locations together.
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

private val BUTTON_FUNCTIONS = BUTTON_KC_INDEX.keys.toList()
private val AXIS_FUNCTIONS = AXIS_KC_INDEX.keys.toList()

// D-pad direction → virtual joystick button index (must match DPAD_BUTTON_BASE in joy.c)
private val DPAD_CONTROLS =
    linkedMapOf(
        "DUp" to 22,
        "DDown" to 23,
        "DLeft" to 24,
        "DRight" to 25,
    )

private val KB_FUNCTIONS = KB_KC_INDEX.keys.toList()

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

// Default axis-to-button activation threshold (percentage, 5-95)
const val DEFAULT_AXIS_THRESHOLD = 30

// Axis IDs that support per-axis thresholds
private val THRESHOLD_AXES = listOf("LS_X", "LS_Y", "RS_X", "RS_Y", "LT", "RT")

// Build a default thresholds map with all axes at DEFAULT_AXIS_THRESHOLD
private fun defaultThresholds(): Map<String, Int> = THRESHOLD_AXES.associateWith { DEFAULT_AXIS_THRESHOLD }

// Axis functions that implicitly cover discrete button functions
private val AXIS_COVERS_BUTTONS =
    mapOf(
        "Slide L/R" to listOf("Slide Left", "Slide Right"),
        "Slide U/D" to listOf("Slide Up", "Slide Down"),
        "Bank L/R" to listOf("Bank Left", "Bank Right"),
        "Throttle" to listOf("Accelerate", "Reverse"),
    )

// Result from StickPickerDialog including axis-as-buttons mode
private data class StickPickerResult(
    val xFunc: String? = null,
    val yFunc: String? = null,
    val xInvert: Boolean = false,
    val yInvert: Boolean = false,
    val xButtonMode: Boolean = false,
    val yButtonMode: Boolean = false,
    val xNegFunc: String? = null,
    val xPosFunc: String? = null,
    val yNegFunc: String? = null,
    val yPosFunc: String? = null,
)

// ── Config file I/O ─────────────────────────────────────────────────────────

private const val CONFIG_FILENAME = "controller_config.json"

/**
 * Collect (kc_index, value) pairs for joystick settings from human-readable
 * bindings using the given game variant's kconfig index mapping.
 */
private fun buildJoyPairs(
    bindings: Map<String, String>,
    inverts: Set<String>,
    variant: String,
): Pair<IntArray, IntArray> {
    val btnKcMap = buttonKcIndex(variant)
    val indices = mutableListOf<Int>()
    val values = mutableListOf<Int>()
    for ((controlId, funcLabel) in bindings) {
        val btnKcIdx = btnKcMap[funcLabel]
        val btnSdlId = BUTTON_CONTROLS[controlId]
        if (btnKcIdx != null && btnSdlId != null) {
            indices.add(btnKcIdx)
            values.add(btnSdlId)
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
    for ((controlId, funcLabel) in bindings) {
        val dpadBtnIdx = DPAD_CONTROLS[controlId]
        val dpadKcIdx = btnKcMap[funcLabel]
        if (dpadBtnIdx != null && dpadKcIdx != null) {
            indices.add(dpadKcIdx)
            values.add(dpadBtnIdx)
        }
    }
    return Pair(indices.toIntArray(), values.toIntArray())
}

internal fun saveConfig(
    context: Context,
    bindings: Map<String, String>,
    inverts: Set<String>,
    gameVariant: String = "d2",
    thresholds: Map<String, Int> = defaultThresholds(),
) {
    // Build byte arrays for BOTH D1 and D2 since the config file is shared
    // and kconfig indices differ between games (e.g. Automap is index 27 in
    // D1 but index 50 in D2).
    val (d1Idx, d1Val) = buildJoyPairs(bindings, inverts, "d1")
    val (d2Idx, d2Val) = buildJoyPairs(bindings, inverts, "d2")
    val d1JoySettings = NativePilotPatcher.nativeBuildJoySettings(d1Idx, d1Val)
    val d2JoySettings = NativePilotPatcher.nativeBuildJoySettings(d2Idx, d2Val)
    val joySettings = if (gameVariant == "d1") d1JoySettings else d2JoySettings

    val kbIndices = mutableListOf<Int>()
    val kbValues = mutableListOf<Int>()
    val kbSettings =
        NativePilotPatcher.nativeBuildKbSettings(
            kbIndices.toIntArray(),
            kbValues.toIntArray(),
        )

    val controlType = 1 // CONTROL_USING_JOYSTICK

    // Build JSON config for UI reconstruction + byte arrays for re-patching
    val json = JSONObject()
    json.put("version", 1)
    json.put("control_type", controlType)
    json.put("automap_free_flight", 1)

    val bindingsObj = JSONObject()
    for ((k, v) in bindings) bindingsObj.put(k, v)
    json.put("bindings", bindingsObj)

    val invertsArr = JSONArray()
    for (inv in inverts) invertsArr.put(inv)
    json.put("inverts", invertsArr)

    // Meta action bindings: SDL button index or DPAD keycode → meta action ID
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
        // Axis-as-buttons meta actions
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

    // Write game-specific byte arrays so the C code (which is compiled
    // separately for D1 and D2) can read the correct kconfig indices.
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

    File(context.filesDir, CONFIG_FILENAME).writeText(json.toString(2))

    NativePilotPatcher.nativePatchPilotFiles(
        context.filesDir.absolutePath,
        joySettings,
        kbSettings,
        controlType,
        gameVariant,
    )
}

private data class LoadedConfig(
    val bindings: Map<String, String>,
    val inverts: Set<String>,
    val thresholds: Map<String, Int>,
)

private fun loadConfig(context: Context): LoadedConfig? {
    val file = File(context.filesDir, CONFIG_FILENAME)
    if (!file.exists()) return null
    val json = JSONObject(file.readText())
    if (!json.has("bindings")) return null

    val bindingsObj = json.getJSONObject("bindings")
    val bindings = mutableMapOf<String, String>()
    for (key in bindingsObj.keys()) bindings[key] = bindingsObj.getString(key)

    val invertedControls = mutableSetOf<String>()
    if (json.has("inverts")) {
        val arr = json.getJSONArray("inverts")
        for (i in 0 until arr.length()) invertedControls.add(arr.getString(i))
    }

    val thresholds = defaultThresholds().toMutableMap()
    if (json.has("thresholds")) {
        val tObj = json.getJSONObject("thresholds")
        for (key in tObj.keys()) {
            if (key in thresholds) thresholds[key] = tObj.getInt(key).coerceIn(5, 95)
        }
    }
    return LoadedConfig(bindings, invertedControls, thresholds)
}

// ── Assignment logic ────────────────────────────────────────────────────────

private fun assignButtonFunction(
    bindings: MutableMap<String, String>,
    controlId: String,
    funcLabel: String?,
) {
    if (funcLabel == null) {
        bindings.remove(controlId)
        return
    }
    // Clear this function from any other button control
    val existing =
        bindings.entries.find {
            it.value == funcLabel && it.key != controlId && it.key in BUTTON_CONTROLS
        }
    if (existing != null) bindings.remove(existing.key)
    bindings[controlId] = funcLabel
}

private fun assignAxisFunction(
    bindings: MutableMap<String, String>,
    axisControlId: String,
    funcLabel: String?,
) {
    if (funcLabel == null) {
        bindings.remove(axisControlId)
        return
    }
    // Clear this function from any other axis control
    val existing =
        bindings.entries.find {
            it.value == funcLabel && it.key != axisControlId && it.key in AXIS_CONTROLS
        }
    if (existing != null) bindings.remove(existing.key)
    bindings[axisControlId] = funcLabel
}

private fun assignDpadFunction(
    bindings: MutableMap<String, String>,
    controlId: String,
    funcLabel: String?,
) {
    if (funcLabel == null) {
        bindings.remove(controlId)
        return
    }
    // Clear this function from any other d-pad direction
    val existing =
        bindings.entries.find {
            it.value == funcLabel && it.key != controlId && it.key in DPAD_CONTROLS
        }
    if (existing != null) bindings.remove(existing.key)
    bindings[controlId] = funcLabel
}

// ── Abbreviations for on-canvas labels ──────────────────────────────────────

private fun abbreviate(label: String): String =
    when (label) {
        "Fire Primary" -> "Fire1"
        "Fire Secondary" -> "Fire2"
        "Accelerate" -> "Accel"
        "Reverse" -> "Rev"
        "Fire Flare" -> "Flare"
        "Slide On" -> "SldOn"
        "Slide Left" -> "Sld←"
        "Slide Right" -> "Sld→"
        "Slide Up" -> "Sld↑"
        "Slide Down" -> "Sld↓"
        "Bank On" -> "BnkOn"
        "Bank Left" -> "Bnk←"
        "Bank Right" -> "Bnk→"
        "Rear View" -> "Rear"
        "Drop Bomb" -> "Bomb"
        "Afterburner" -> "ABurn"
        "Cycle Primary" -> "Cyc1"
        "Cycle Secondary" -> "Cyc2"
        "Headlight" -> "Light"
        "Automap" -> "Map"
        "Energy\u2192Shield" -> "E\u2192S"
        "Toggle Bomb" -> "TgBmb"
        "Pitch U/D" -> "Pitch"
        "Turn L/R" -> "Turn"
        "Slide L/R" -> "SldLR"
        "Slide U/D" -> "SldUD"
        "Bank L/R" -> "Bank"
        "Throttle" -> "Thrtl"
        "Pitch Forward" -> "Pit\u2191"
        "Pitch Backward" -> "Pit\u2193"
        "Turn Left" -> "Trn\u2190"
        "Turn Right" -> "Trn\u2192"
        else -> label.take(5)
    }

// Returns (negative-direction label, positive-direction label) for an axis function
private fun axisNegLabel(funcLabel: String): String =
    when (funcLabel) {
        "Pitch U/D" -> "Pit\u2191"
        "Turn L/R" -> "Trn\u2190"
        "Slide L/R" -> "Sld\u2190"
        "Slide U/D" -> "Sld\u2191"
        "Bank L/R" -> "Bnk\u2190"
        "Throttle" -> "Thr+"
        else -> funcLabel.take(4)
    }

private fun axisPosLabel(funcLabel: String): String =
    when (funcLabel) {
        "Pitch U/D" -> "Pit\u2193"
        "Turn L/R" -> "Trn\u2192"
        "Slide L/R" -> "Sld\u2192"
        "Slide U/D" -> "Sld\u2193"
        "Bank L/R" -> "Bnk\u2192"
        "Throttle" -> "Thr\u2212"
        else -> funcLabel.take(4)
    }

// ── Main Composable ─────────────────────────────────────────────────────────

@Composable
fun ControllerConfigPage(
    axes: FloatArray,
    dpadAxes: FloatArray,
    axisGeneration: Int,
    pressedButtons: SnapshotStateList<String>,
    gameVariant: String = "d2",
    onBack: () -> Unit,
) {
    BackHandler(onBack = onBack)
    @Suppress("UNUSED_EXPRESSION")
    axisGeneration

    val context = LocalContext.current
    val lx = axes[0]
    val ly = axes[1]
    val rx = axes[2]
    val ry = axes[3]
    val lt = axes[4]
    val rt = axes[5]
    val hatX = dpadAxes[0]
    val hatY = dpadAxes[1]

    // Bindings state: control ID → function label
    val bindings = remember { mutableStateMapOf<String, String>() }
    val inverts = remember { mutableStateListOf<String>() } // inverted axis control IDs
    val thresholds = remember { mutableStateMapOf<String, Int>() }
    var initialized by remember { mutableStateOf(false) }
    if (!initialized) {
        val saved = loadConfig(context)
        if (saved != null) {
            bindings.putAll(saved.bindings)
            inverts.addAll(saved.inverts)
            thresholds.putAll(saved.thresholds)
        } else {
            bindings.putAll(loadDefaultBindings(context))
            thresholds.putAll(defaultThresholds())
        }
        initialized = true
    }

    // SAF file picker for importing controller configs
    val importPickerLauncher =
        rememberLauncherForActivityResult(
            contract =
                androidx.activity.result.contract.ActivityResultContracts
                    .OpenDocument(),
        ) { uri ->
            if (uri == null) return@rememberLauncherForActivityResult
            val msg = ConfigImportExport.importFromUri(context, uri)
            Toast.makeText(context, msg, Toast.LENGTH_LONG).show()
            // Reload the imported config
            val reloaded = loadConfig(context)
            if (reloaded != null) {
                bindings.clear()
                bindings.putAll(reloaded.bindings)
                inverts.clear()
                inverts.addAll(reloaded.inverts)
                thresholds.clear()
                thresholds.putAll(reloaded.thresholds)
            }
        }

    // Touch/dialog state
    val controlBounds = remember { mutableMapOf<String, Rect>() }
    var selectedControl by remember { mutableStateOf<String?>(null) }
    var showButtonPicker by remember { mutableStateOf(false) }
    var showStickPicker by remember { mutableStateOf(false) }
    var showDpadPicker by remember { mutableStateOf(false) }

    val isLandscape = LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE

    // ── Unassigned functions (computed once, used in both layouts) ──
    val assignedBtnFuncs =
        bindings.entries
            .filter { it.key in BUTTON_CONTROLS }
            .map { it.value }
            .toSet()
    val assignedAxisFuncs =
        bindings.entries
            .filter { it.key in AXIS_CONTROLS }
            .map { it.value }
            .toSet()
    val assignedDpadFuncs =
        bindings.entries
            .filter { it.key in DPAD_CONTROLS }
            .map { it.value }
            .toSet()
    val allAssignedBtnLike = assignedBtnFuncs + assignedDpadFuncs
    val coveredByAxis = assignedAxisFuncs.flatMap { AXIS_COVERS_BUTTONS[it].orEmpty() }.toSet()
    val d2OnlyFilter: (String) -> Boolean =
        if (gameVariant == "d1") {
            { it !in TouchBindings.D2_ONLY_BUTTON_LABELS }
        } else {
            { true }
        }
    val unassignedBtns =
        BUTTON_FUNCTIONS.filter { it !in allAssignedBtnLike && it !in coveredByAxis && d2OnlyFilter(it) }
    val coveredByButtons =
        AXIS_FUNCTIONS
            .filter { af ->
                val btns = AXIS_COVERS_BUTTONS[af]
                btns != null && btns.all { it in allAssignedBtnLike }
            }.toSet()
    val unassignedAxes = AXIS_FUNCTIONS.filter { it !in assignedAxisFuncs && it !in coveredByButtons }
    val allUnassigned = (unassignedBtns + unassignedAxes).distinct()

    // ── Reusable composable blocks ──

    val controllerCanvas: @Composable (Modifier) -> Unit = { sizeModifier ->
        val textMeasurer = rememberTextMeasurer()
        Canvas(
            modifier =
                Modifier
                    .fillMaxWidth()
                    .then(sizeModifier)
                    .background(Color(0xFF121212))
                    .pointerInput(Unit) {
                        detectTapGestures { offset ->
                            for ((id, rect) in controlBounds) {
                                if (rect.contains(offset)) {
                                    // Direction labels map back to their parent stick
                                    val resolvedId =
                                        when {
                                            id.startsWith("LS_L") -> "LS"
                                            id.startsWith("RS_L") -> "RS"
                                            else -> id
                                        }
                                    selectedControl = resolvedId
                                    when {
                                        resolvedId == "LS" || resolvedId == "RS" -> showStickPicker = true
                                        resolvedId in DPAD_CONTROLS -> showDpadPicker = true
                                        else -> showButtonPicker = true
                                    }
                                    break
                                }
                            }
                        }
                    },
        ) {
            val w = size.width
            val h = size.height
            val scale = min(w, h)

            val phoneW = w * 0.30f
            val phoneH = h * 0.70f
            val phoneX = (w - phoneW) / 2f
            val phoneY = (h - phoneH) / 2f

            val gripW = w * 0.24f
            val gripH = h * 0.82f
            val gripY = (h - gripH) / 2f
            val leftGripX = phoneX - gripW + 8f
            val rightGripX = phoneX + phoneW - 8f

            // ── Grips ──
            drawRoundRect(
                color = cFill,
                topLeft = Offset(leftGripX, gripY),
                size = Size(gripW, gripH),
                cornerRadius = CornerRadius(scale * 0.04f),
            )
            drawRoundRect(
                color = cOutline,
                topLeft = Offset(leftGripX, gripY),
                size = Size(gripW, gripH),
                cornerRadius = CornerRadius(scale * 0.04f),
                style = Stroke(2f),
            )
            drawRoundRect(
                color = cFill,
                topLeft = Offset(rightGripX, gripY),
                size = Size(gripW, gripH),
                cornerRadius = CornerRadius(scale * 0.04f),
            )
            drawRoundRect(
                color = cOutline,
                topLeft = Offset(rightGripX, gripY),
                size = Size(gripW, gripH),
                cornerRadius = CornerRadius(scale * 0.04f),
                style = Stroke(2f),
            )

            // ── Phone body ──
            drawRoundRect(
                color = cPhoneBody,
                topLeft = Offset(phoneX, phoneY),
                size = Size(phoneW, phoneH),
                cornerRadius = CornerRadius(scale * 0.02f),
            )
            val screenMargin = scale * 0.015f
            drawRoundRect(
                color = cPhoneScreen,
                topLeft = Offset(phoneX + screenMargin, phoneY + screenMargin),
                size = Size(phoneW - screenMargin * 2, phoneH - screenMargin * 2),
                cornerRadius = CornerRadius(scale * 0.01f),
            )

            // ── Left grip controls ──
            val lgCx = leftGripX + gripW / 2f
            val stickR = scale * 0.055f
            val touchPad = scale * 0.03f // extra padding for touch targets
            val btnR = scale * 0.026f

            // Left stick
            val lsCx = lgCx
            val lsCy = gripY + gripH * 0.30f
            drawStick(lsCx, lsCy, stickR, lx, ly, "LS")
            controlBounds["LS"] =
                Rect(
                    lsCx - stickR,
                    lsCy - stickR,
                    lsCx + stickR,
                    lsCy + stickR,
                )
            // Show directional labels around left stick
            val lsxFunc = bindings["LS_X"]
            val lsyFunc = bindings["LS_Y"]
            val sLabelOff = stickR + scale * 0.035f
            val lsxInv = "LS_X" in inverts
            val lsyInv = "LS_Y" in inverts
            // Y axis: axis mode or button mode
            val lsyNeg = bindings["LS_Y_neg"]
            val lsyPos = bindings["LS_Y_pos"]
            if (lsyNeg != null || lsyPos != null) {
                // Button mode Y
                if (lsyNeg != null) {
                    val topC = if (ly < -0.3f) cActive else cAssignLabel
                    drawFuncLabel(textMeasurer, abbreviate(lsyNeg), lsCx, lsCy - sLabelOff, scale, topC)
                }
                if (lsyPos != null) {
                    val botC = if (ly > 0.3f) cActive else cAssignLabel
                    drawFuncLabel(textMeasurer, abbreviate(lsyPos), lsCx, lsCy + sLabelOff, scale, botC)
                }
            } else if (lsyFunc != null) {
                val topLabel = if (lsyInv) axisPosLabel(lsyFunc) else axisNegLabel(lsyFunc)
                val botLabel = if (lsyInv) axisNegLabel(lsyFunc) else axisPosLabel(lsyFunc)
                val topC = if (ly < -0.3f) cActive else cAssignLabel
                drawFuncLabel(textMeasurer, topLabel, lsCx, lsCy - sLabelOff, scale, topC)
                val botC = if (ly > 0.3f) cActive else cAssignLabel
                drawFuncLabel(textMeasurer, botLabel, lsCx, lsCy + sLabelOff, scale, botC)
            }
            // X axis: axis mode or button mode
            val lsxNeg = bindings["LS_X_neg"]
            val lsxPos = bindings["LS_X_pos"]
            if (lsxNeg != null || lsxPos != null) {
                if (lsxNeg != null) {
                    val leftC = if (lx < -0.3f) cActive else cAssignLabel
                    drawFuncLabel(textMeasurer, abbreviate(lsxNeg), lsCx - sLabelOff * 1.5f, lsCy, scale, leftC)
                }
                if (lsxPos != null) {
                    val rightC = if (lx > 0.3f) cActive else cAssignLabel
                    drawFuncLabel(textMeasurer, abbreviate(lsxPos), lsCx + sLabelOff * 1.5f, lsCy, scale, rightC)
                }
            } else if (lsxFunc != null) {
                val leftLabel = if (lsxInv) axisPosLabel(lsxFunc) else axisNegLabel(lsxFunc)
                val rightLabel = if (lsxInv) axisNegLabel(lsxFunc) else axisPosLabel(lsxFunc)
                val leftC = if (lx < -0.3f) cActive else cAssignLabel
                drawFuncLabel(textMeasurer, leftLabel, lsCx - sLabelOff * 1.5f, lsCy, scale, leftC)
                val rightC = if (lx > 0.3f) cActive else cAssignLabel
                drawFuncLabel(textMeasurer, rightLabel, lsCx + sLabelOff * 1.5f, lsCy, scale, rightC)
            }
            // Highlight if selected
            if (selectedControl == "LS") {
                drawCircle(cHighlight, stickR, Offset(lsCx, lsCy))
            }

            // D-pad (interactive)
            val dpadCx = lgCx
            val dpadCy = gripY + gripH * 0.72f
            val dpadSize = scale * 0.07f
            drawDpad(dpadCx, dpadCy, dpadSize, hatX, hatY)
            // D-pad touch bounds for each arm
            val dArmW = dpadSize * 0.35f
            controlBounds["DUp"] =
                Rect(
                    dpadCx - dArmW / 2 - touchPad,
                    dpadCy - dpadSize - touchPad,
                    dpadCx + dArmW / 2 + touchPad,
                    dpadCy - touchPad,
                )
            controlBounds["DDown"] =
                Rect(
                    dpadCx - dArmW / 2 - touchPad,
                    dpadCy + touchPad,
                    dpadCx + dArmW / 2 + touchPad,
                    dpadCy + dpadSize + touchPad,
                )
            controlBounds["DLeft"] =
                Rect(
                    dpadCx - dpadSize - touchPad,
                    dpadCy - dArmW / 2 - touchPad,
                    dpadCx - touchPad,
                    dpadCy + dArmW / 2 + touchPad,
                )
            controlBounds["DRight"] =
                Rect(
                    dpadCx + touchPad,
                    dpadCy - dArmW / 2 - touchPad,
                    dpadCx + dpadSize + touchPad,
                    dpadCy + dArmW / 2 + touchPad,
                )
            // D-pad direction labels
            val dLabelOff = dpadSize + scale * 0.025f
            bindings["DUp"]?.let {
                val c = if (hatY < -0.5f) cActive else cAssignLabel
                drawFuncLabel(textMeasurer, abbreviate(it), dpadCx, dpadCy - dLabelOff, scale, c)
            }
            bindings["DDown"]?.let {
                val c = if (hatY > 0.5f) cActive else cAssignLabel
                drawFuncLabel(textMeasurer, abbreviate(it), dpadCx, dpadCy + dLabelOff, scale, c)
            }
            bindings["DLeft"]?.let {
                val c = if (hatX < -0.5f) cActive else cAssignLabel
                drawFuncLabel(textMeasurer, abbreviate(it), dpadCx - dLabelOff * 1.5f, dpadCy, scale, c)
            }
            bindings["DRight"]?.let {
                val c = if (hatX > 0.5f) cActive else cAssignLabel
                drawFuncLabel(textMeasurer, abbreviate(it), dpadCx + dLabelOff * 1.5f, dpadCy, scale, c)
            }

            // L1 bumper
            val bumperH = scale * 0.025f
            val bumperW = gripW * 0.6f
            val l1X = lgCx - bumperW / 2f
            val l1Y = gripY + scale * 0.02f
            val l1Pressed = "L1" in pressedButtons
            drawRoundRect(
                color = if (l1Pressed) cActive else cInactive,
                topLeft = Offset(l1X, l1Y),
                size = Size(bumperW, bumperH),
                cornerRadius = CornerRadius(bumperH / 2f),
            )
            controlBounds["L1"] =
                Rect(
                    l1X - touchPad,
                    l1Y - touchPad,
                    l1X + bumperW + touchPad,
                    l1Y + bumperH + touchPad,
                )
            drawLabel(textMeasurer, "L1", l1X + bumperW / 2f, l1Y + bumperH + scale * 0.012f, scale)
            bindings["L1"]?.let {
                val c = if (l1Pressed) cActive else cAssignLabel
                drawFuncLabelRightAligned(
                    textMeasurer,
                    abbreviate(it),
                    l1X - scale * 0.01f,
                    l1Y + bumperH / 2f,
                    scale,
                    c,
                )
            }

            // LT trigger
            val triggerW = gripW * 0.5f
            val triggerH = scale * 0.03f
            val l2X = lgCx - triggerW / 2f
            val l2Y = gripY - triggerH - scale * 0.015f
            drawTrigger(l2X, l2Y, triggerW, triggerH, lt)
            controlBounds["LT"] =
                Rect(
                    l2X - touchPad,
                    l2Y - touchPad,
                    l2X + triggerW + touchPad,
                    l2Y + triggerH + touchPad,
                )
            drawLabel(textMeasurer, "LT", l2X + triggerW / 2f, l2Y - scale * 0.02f, scale)
            bindings["LT"]?.let {
                val c = if (lt > 0.1f) cActive else cAssignLabel
                drawFuncLabelRightAligned(
                    textMeasurer,
                    abbreviate(it),
                    l2X - scale * 0.01f,
                    l2Y + triggerH / 2f,
                    scale,
                    c,
                )
            }

            // L3 (stick press) – small circle, down-left from left stick
            val l3Offset = stickR + btnR * 3 // 2.5× button-radius gap from stick edge
            val invSqrt2 = 0.7071f // 1/sqrt(2) for 45° diagonal
            val l3Cx = lsCx - l3Offset * invSqrt2
            val l3Cy = lsCy + l3Offset * invSqrt2
            val l3Pressed = "L3" in pressedButtons
            run {
                val dx = l3Cx - lsCx
                val dy = l3Cy - lsCy
                val dist = sqrt(dx * dx + dy * dy)
                val nx = dx / dist
                val ny = dy / dist
                drawLine(
                    cOutline,
                    Offset(lsCx + stickR * nx, lsCy + stickR * ny),
                    Offset(l3Cx - btnR * nx, l3Cy - btnR * ny),
                    strokeWidth = 1.5f,
                )
            }
            drawFaceButton(textMeasurer, l3Cx, l3Cy, btnR, "Dn", l3Pressed, scale)
            controlBounds["L3"] =
                Rect(
                    l3Cx - btnR - touchPad,
                    l3Cy - btnR - touchPad,
                    l3Cx + btnR + touchPad,
                    l3Cy + btnR + touchPad,
                )
            bindings["L3"]?.let {
                val c = if (l3Pressed) cActive else cAssignLabel
                drawFuncLabel(
                    textMeasurer,
                    abbreviate(it),
                    l3Cx,
                    l3Cy + btnR + scale * 0.02f,
                    scale,
                    c,
                )
            }
            if (selectedControl == "L3") {
                drawCircle(cHighlight, btnR + touchPad, Offset(l3Cx, l3Cy))
            }

            // ── Right grip controls ──
            val rgCx = rightGripX + gripW / 2f

            // Right stick (vertically aligned with D-pad center)
            val rsCx = rgCx
            val rsCy = dpadCy
            drawStick(rsCx, rsCy, stickR, rx, ry, "RS")
            controlBounds["RS"] =
                Rect(
                    rsCx - stickR,
                    rsCy - stickR,
                    rsCx + stickR,
                    rsCy + stickR,
                )
            val rsxFunc = bindings["RS_X"]
            val rsyFunc = bindings["RS_Y"]
            val rsxInv = "RS_X" in inverts
            val rsyInv = "RS_Y" in inverts
            // Y axis: axis mode or button mode
            val rsyNeg = bindings["RS_Y_neg"]
            val rsyPos = bindings["RS_Y_pos"]
            if (rsyNeg != null || rsyPos != null) {
                if (rsyNeg != null) {
                    val topC = if (ry < -0.3f) cActive else cAssignLabel
                    drawFuncLabel(textMeasurer, abbreviate(rsyNeg), rsCx, rsCy - sLabelOff, scale, topC)
                }
                if (rsyPos != null) {
                    val botC = if (ry > 0.3f) cActive else cAssignLabel
                    drawFuncLabel(textMeasurer, abbreviate(rsyPos), rsCx, rsCy + sLabelOff, scale, botC)
                }
            } else if (rsyFunc != null) {
                val topLabel = if (rsyInv) axisPosLabel(rsyFunc) else axisNegLabel(rsyFunc)
                val botLabel = if (rsyInv) axisNegLabel(rsyFunc) else axisPosLabel(rsyFunc)
                val topC = if (ry < -0.3f) cActive else cAssignLabel
                drawFuncLabel(textMeasurer, topLabel, rsCx, rsCy - sLabelOff, scale, topC)
                val botC = if (ry > 0.3f) cActive else cAssignLabel
                drawFuncLabel(textMeasurer, botLabel, rsCx, rsCy + sLabelOff, scale, botC)
            }
            val rsxNeg = bindings["RS_X_neg"]
            val rsxPos = bindings["RS_X_pos"]
            if (rsxNeg != null || rsxPos != null) {
                if (rsxNeg != null) {
                    val leftC = if (rx < -0.3f) cActive else cAssignLabel
                    drawFuncLabel(textMeasurer, abbreviate(rsxNeg), rsCx - sLabelOff * 1.5f, rsCy, scale, leftC)
                }
                if (rsxPos != null) {
                    val rightC = if (rx > 0.3f) cActive else cAssignLabel
                    drawFuncLabel(textMeasurer, abbreviate(rsxPos), rsCx + sLabelOff * 1.5f, rsCy, scale, rightC)
                }
            } else if (rsxFunc != null) {
                val leftLabel = if (rsxInv) axisPosLabel(rsxFunc) else axisNegLabel(rsxFunc)
                val rightLabel = if (rsxInv) axisNegLabel(rsxFunc) else axisPosLabel(rsxFunc)
                val leftC = if (rx < -0.3f) cActive else cAssignLabel
                drawFuncLabel(textMeasurer, leftLabel, rsCx - sLabelOff * 1.5f, rsCy, scale, leftC)
                val rightC = if (rx > 0.3f) cActive else cAssignLabel
                drawFuncLabel(textMeasurer, rightLabel, rsCx + sLabelOff * 1.5f, rsCy, scale, rightC)
            }
            if (selectedControl == "RS") {
                drawCircle(cHighlight, stickR, Offset(rsCx, rsCy))
            }

            // Face buttons ABXY
            val btnCx = rgCx
            val btnCy = gripY + gripH * 0.30f
            val btnSpacing = scale * 0.046f

            data class FaceBtn(
                val id: String,
                val cx: Float,
                val cy: Float,
            )
            val faceButtons =
                listOf(
                    FaceBtn("Y", btnCx, btnCy - btnSpacing),
                    FaceBtn("A", btnCx, btnCy + btnSpacing),
                    FaceBtn("X", btnCx - btnSpacing, btnCy),
                    FaceBtn("B", btnCx + btnSpacing, btnCy),
                )
            for (fb in faceButtons) {
                val pressed = fb.id in pressedButtons
                drawFaceButton(textMeasurer, fb.cx, fb.cy, btnR, fb.id, pressed, scale)
                // Touch bounds = circle only (no touchPad) to avoid overlap
                controlBounds[fb.id] =
                    Rect(
                        fb.cx - btnR,
                        fb.cy - btnR,
                        fb.cx + btnR,
                        fb.cy + btnR,
                    )
                bindings[fb.id]?.let {
                    val c = if (pressed) cActive else cAssignLabel
                    val abbr = abbreviate(it)
                    when (fb.id) {
                        "A" ->
                            drawFuncLabel(
                                textMeasurer,
                                abbr,
                                fb.cx,
                                fb.cy + btnR + scale * 0.024f,
                                scale,
                                c,
                            )
                        "B" ->
                            drawFuncLabelLeftAligned(
                                textMeasurer,
                                abbr,
                                fb.cx + btnR + scale * 0.01f,
                                fb.cy,
                                scale,
                                c,
                            )
                        "X" ->
                            drawFuncLabelRightAligned(
                                textMeasurer,
                                abbr,
                                fb.cx - btnR - scale * 0.01f,
                                fb.cy,
                                scale,
                                c,
                            )
                        "Y" ->
                            drawFuncLabel(
                                textMeasurer,
                                abbr,
                                fb.cx,
                                fb.cy - btnR - scale * 0.024f,
                                scale,
                                c,
                            )
                        else ->
                            drawFuncLabel(
                                textMeasurer,
                                abbr,
                                fb.cx,
                                fb.cy + btnR + scale * 0.025f,
                                scale,
                                c,
                            )
                    }
                }
            }

            // R1 bumper
            val r1X = rgCx - bumperW / 2f
            val r1Y = gripY + scale * 0.02f
            val r1Pressed = "R1" in pressedButtons
            drawRoundRect(
                color = if (r1Pressed) cActive else cInactive,
                topLeft = Offset(r1X, r1Y),
                size = Size(bumperW, bumperH),
                cornerRadius = CornerRadius(bumperH / 2f),
            )
            controlBounds["R1"] =
                Rect(
                    r1X - touchPad,
                    r1Y - touchPad,
                    r1X + bumperW + touchPad,
                    r1Y + bumperH + touchPad,
                )
            drawLabel(textMeasurer, "R1", r1X + bumperW / 2f, r1Y + bumperH + scale * 0.012f, scale)
            bindings["R1"]?.let {
                val c = if (r1Pressed) cActive else cAssignLabel
                drawFuncLabelLeftAligned(
                    textMeasurer,
                    abbreviate(it),
                    r1X + bumperW + scale * 0.01f,
                    r1Y + bumperH / 2f,
                    scale,
                    c,
                )
            }

            // RT trigger
            val r2X = rgCx - triggerW / 2f
            val r2Y = gripY - triggerH - scale * 0.015f
            drawTrigger(r2X, r2Y, triggerW, triggerH, rt)
            controlBounds["RT"] =
                Rect(
                    r2X - touchPad,
                    r2Y - touchPad,
                    r2X + triggerW + touchPad,
                    r2Y + triggerH + touchPad,
                )
            drawLabel(textMeasurer, "RT", r2X + triggerW / 2f, r2Y - scale * 0.02f, scale)
            bindings["RT"]?.let {
                val c = if (rt > 0.1f) cActive else cAssignLabel
                drawFuncLabelLeftAligned(
                    textMeasurer,
                    abbreviate(it),
                    r2X + triggerW + scale * 0.01f,
                    r2Y + triggerH / 2f,
                    scale,
                    c,
                )
            }

            // R3 (stick press) – small circle, up-right from right stick
            val r3Cx = rsCx + l3Offset * invSqrt2
            val r3Cy = rsCy - l3Offset * invSqrt2
            val r3Pressed = "R3" in pressedButtons
            run {
                val dx = r3Cx - rsCx
                val dy = r3Cy - rsCy
                val dist = sqrt(dx * dx + dy * dy)
                val nx = dx / dist
                val ny = dy / dist
                drawLine(
                    cOutline,
                    Offset(rsCx + stickR * nx, rsCy + stickR * ny),
                    Offset(r3Cx - btnR * nx, r3Cy - btnR * ny),
                    strokeWidth = 1.5f,
                )
            }
            drawFaceButton(textMeasurer, r3Cx, r3Cy, btnR, "Dn", r3Pressed, scale)
            controlBounds["R3"] =
                Rect(
                    r3Cx - btnR - touchPad,
                    r3Cy - btnR - touchPad,
                    r3Cx + btnR + touchPad,
                    r3Cy + btnR + touchPad,
                )
            bindings["R3"]?.let {
                val c = if (r3Pressed) cActive else cAssignLabel
                drawFuncLabel(
                    textMeasurer,
                    abbreviate(it),
                    r3Cx,
                    r3Cy - btnR - scale * 0.02f,
                    scale,
                    c,
                )
            }
            if (selectedControl == "R3") {
                drawCircle(cHighlight, btnR + touchPad, Offset(r3Cx, r3Cy))
            }

            // ── Center buttons (Select / Start) ──
            val centerBtnR = btnR // same size as A/B/X/Y
            val centerY = phoneY + phoneH * 0.20f - centerBtnR * 2
            val selX = phoneX + phoneW * 0.25f
            val staX = phoneX + phoneW * 0.75f

            val selPressed = "Select" in pressedButtons
            drawFaceButton(textMeasurer, selX, centerY, centerBtnR, "Sel", selPressed, scale)
            controlBounds["Select"] =
                Rect(
                    selX - centerBtnR - touchPad,
                    centerY - centerBtnR - touchPad,
                    selX + centerBtnR + touchPad,
                    centerY + centerBtnR + touchPad,
                )
            bindings["Select"]?.let {
                val c = if (selPressed) cActive else cAssignLabel
                drawFuncLabel(
                    textMeasurer,
                    abbreviate(it),
                    selX,
                    centerY + centerBtnR + scale * 0.02f,
                    scale,
                    c,
                )
            }

            val staPressed = "Start" in pressedButtons
            drawFaceButton(textMeasurer, staX, centerY, centerBtnR, "Sta", staPressed, scale)
            controlBounds["Start"] =
                Rect(
                    staX - centerBtnR - touchPad,
                    centerY - centerBtnR - touchPad,
                    staX + centerBtnR + touchPad,
                    centerY + centerBtnR + touchPad,
                )
            bindings["Start"]?.let {
                val c = if (staPressed) cActive else cAssignLabel
                drawFuncLabel(
                    textMeasurer,
                    abbreviate(it),
                    staX,
                    centerY + centerBtnR + scale * 0.02f,
                    scale,
                    c,
                )
            }

            // ── Expand touch bounds to cover function labels ──
            val labelPad = scale * 0.035f
            val growBounds = { id: String, cx: Float, cy: Float ->
                controlBounds[id]?.let { r ->
                    controlBounds[id] =
                        Rect(
                            minOf(r.left, cx - labelPad),
                            minOf(r.top, cy - labelPad),
                            maxOf(r.right, cx + labelPad),
                            maxOf(r.bottom, cy + labelPad),
                        )
                }
            }
            // Stick direction labels as separate touch targets (open same picker)
            for ((stickId, sCx, sCy) in listOf(Triple("LS", lsCx, lsCy), Triple("RS", rsCx, rsCy))) {
                controlBounds["${stickId}_LU"] =
                    Rect(sCx - labelPad, sCy - sLabelOff - labelPad, sCx + labelPad, sCy - sLabelOff + labelPad)
                controlBounds["${stickId}_LD"] =
                    Rect(sCx - labelPad, sCy + sLabelOff - labelPad, sCx + labelPad, sCy + sLabelOff + labelPad)
                controlBounds["${stickId}_LL"] =
                    Rect(
                        sCx - sLabelOff * 1.5f - labelPad,
                        sCy - labelPad,
                        sCx - sLabelOff * 1.5f + labelPad,
                        sCy + labelPad,
                    )
                controlBounds["${stickId}_LR"] =
                    Rect(
                        sCx + sLabelOff * 1.5f - labelPad,
                        sCy - labelPad,
                        sCx + sLabelOff * 1.5f + labelPad,
                        sCy + labelPad,
                    )
            }
            // D-pad
            growBounds("DUp", dpadCx, dpadCy - dLabelOff)
            growBounds("DDown", dpadCx, dpadCy + dLabelOff)
            growBounds("DLeft", dpadCx - dLabelOff * 1.5f, dpadCy)
            growBounds("DRight", dpadCx + dLabelOff * 1.5f, dpadCy)
            // Bumpers
            growBounds("L1", l1X - scale * 0.01f, l1Y + bumperH / 2f)
            growBounds("R1", r1X + bumperW + scale * 0.01f, r1Y + bumperH / 2f)
            // Triggers
            growBounds("LT", l2X - scale * 0.01f, l2Y + triggerH / 2f)
            growBounds("RT", r2X + triggerW + scale * 0.01f, r2Y + triggerH / 2f)
            // Face buttons
            for (fb in faceButtons) {
                when (fb.id) {
                    "A" -> growBounds(fb.id, fb.cx, fb.cy + btnR + scale * 0.024f)
                    "B" -> growBounds(fb.id, fb.cx + btnR + scale * 0.01f, fb.cy)
                    "X" -> growBounds(fb.id, fb.cx - btnR - scale * 0.01f, fb.cy)
                    "Y" -> growBounds(fb.id, fb.cx, fb.cy - btnR - scale * 0.024f)
                    else -> growBounds(fb.id, fb.cx, fb.cy + btnR + scale * 0.025f)
                }
            }
            // Center buttons
            growBounds("Select", selX, centerY + centerBtnR + scale * 0.02f)
            growBounds("Start", staX, centerY + centerBtnR + scale * 0.02f)
            // L3 / R3
            growBounds("L3", l3Cx, l3Cy + btnR + scale * 0.02f)
            growBounds("R3", r3Cx, r3Cy - btnR - scale * 0.02f)
        }
    }

    val infoAndButtons: @Composable ColumnScope.() -> Unit = {
        Text(
            text = "Controller Layout",
            fontSize = 20.sp,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.primary,
        )
        Spacer(modifier = Modifier.height(2.dp))
        Text(
            text = "Tap any control to assign a function",
            fontSize = 13.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Spacer(modifier = Modifier.height(6.dp))

        if (allUnassigned.isNotEmpty()) {
            Text(
                text = "Unassigned: ${allUnassigned.joinToString(", ")}",
                fontSize = 10.sp,
                color = Color(0xFFEF5350),
                maxLines = 3,
            )
        }

        // ── Controller detection (poll once/second) ──
        var pollTick by remember { mutableIntStateOf(0) }
        LaunchedEffect(Unit) {
            while (true) {
                kotlinx.coroutines.delay(1000)
                pollTick++
            }
        }
        val gamepads =
            remember(axisGeneration, pollTick) {
                try {
                    InputDevice
                        .getDeviceIds()
                        .toList()
                        .mapNotNull { InputDevice.getDevice(it) }
                        .filter { d ->
                            val src = d.sources
                            src and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
                                src and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
                        }
                } catch (_: Exception) {
                    emptyList()
                }
            }
        val hasController = gamepads.isNotEmpty()
        Text(
            text =
                if (hasController) {
                    "\u2713 ${gamepads.first().name}"
                } else {
                    "\u2717 Not detected"
                },
            color = if (hasController) Color(0xFF4CAF50) else Color(0xFFF44336),
            fontSize = 13.sp,
            fontWeight = FontWeight.SemiBold,
        )
        Spacer(modifier = Modifier.height(4.dp))

        // ── Live readout ──
        val activeButtonsStr = pressedButtons.joinToString(", ").ifEmpty { "none" }
        Text(
            text = "Pressed buttons: $activeButtonsStr",
            fontSize = 11.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Text(
            text =
                "L-Stick: (${"%.2f".format(lx)}, ${"%.2f".format(ly)})  " +
                    "R-Stick: (${"%.2f".format(rx)}, ${"%.2f".format(ry)})",
            fontSize = 11.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Text(
            text =
                "Triggers: LT=${"%.2f".format(lt)}  RT=${"%.2f".format(rt)}  " +
                    "D-Pad: (${"%.1f".format(hatX)}, ${"%.1f".format(hatY)})",
            fontSize = 11.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )

        Spacer(modifier = Modifier.height(8.dp))

        // ── Save & Cancel buttons ──
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            OutlinedButton(
                onClick = onBack,
                modifier = Modifier.weight(1f).height(48.dp),
            ) {
                Text("Cancel", fontSize = 14.sp)
            }
            Button(
                onClick = {
                    saveConfig(context, bindings.toMap(), inverts.toSet(), gameVariant, thresholds.toMap())
                    Toast.makeText(context, "Saved", Toast.LENGTH_SHORT).show()
                    onBack()
                },
                modifier = Modifier.weight(1f).height(48.dp),
                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                colors =
                    ButtonDefaults.buttonColors(
                        containerColor = MaterialTheme.colorScheme.primary,
                    ),
            ) {
                Column(horizontalAlignment = Alignment.CenterHorizontally) {
                    Text(
                        text = "Save",
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Bold,
                    )
                    Text(
                        text = "(to all pilots)",
                        fontSize = 8.sp,
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(4.dp))

        // ── Export / Import buttons ──
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            OutlinedButton(
                onClick = {
                    // Save current state first, then export
                    saveConfig(context, bindings.toMap(), inverts.toSet(), gameVariant, thresholds.toMap())
                    if (!ConfigImportExport.exportControllerConfig(context)) {
                        Toast.makeText(context, "Export failed", Toast.LENGTH_SHORT).show()
                    }
                },
                modifier = Modifier.weight(1f).height(36.dp),
                contentPadding = PaddingValues(horizontal = 4.dp, vertical = 2.dp),
            ) {
                Text("Export", fontSize = 12.sp)
            }
            OutlinedButton(
                onClick = { importPickerLauncher.launch(arrayOf("application/json", "*/*")) },
                modifier = Modifier.weight(1f).height(36.dp),
                contentPadding = PaddingValues(horizontal = 4.dp, vertical = 2.dp),
            ) {
                Text("Import", fontSize = 12.sp)
            }
        }
    }

    // ── Layout ──

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background,
    ) {
        if (isLandscape) {
            Row(
                modifier =
                    Modifier
                        .fillMaxSize()
                        .safeDrawingPadding()
                        .padding(8.dp),
            ) {
                Box(modifier = Modifier.weight(1f).fillMaxHeight().padding(end = 8.dp)) {
                    controllerCanvas(Modifier.fillMaxHeight())
                }
                val rightScroll = rememberScrollState()
                Box(modifier = Modifier.weight(1f).fillMaxHeight()) {
                    Column(
                        modifier =
                            Modifier
                                .fillMaxSize()
                                .verticalScroll(rightScroll)
                                .padding(start = 8.dp),
                    ) {
                        infoAndButtons()
                    }
                    ScrollArrows(rightScroll)
                }
            }
        } else {
            Column(
                modifier =
                    Modifier
                        .fillMaxSize()
                        .safeDrawingPadding()
                        .padding(12.dp),
            ) {
                controllerCanvas(Modifier.weight(1f))
                Spacer(modifier = Modifier.height(6.dp))
                infoAndButtons()
            }
        }
    }

    // ── Dialogs ──

    val assignedButtonFuncs =
        bindings.entries
            .filter {
                it.key in BUTTON_CONTROLS ||
                    it.key in DPAD_CONTROLS ||
                    it.key.endsWith("_neg") ||
                    it.key.endsWith("_pos")
            }.map { it.value }
            .toSet() +
            bindings.entries
                .filter { it.key in AXIS_CONTROLS }
                .flatMap { AXIS_COVERS_BUTTONS[it.value].orEmpty() }
    val assignedAxisFuncsForDialog =
        bindings.entries
            .filter { it.key in AXIS_CONTROLS }
            .map { it.value }
            .toSet()
    val assignedDpadFuncsForDialog =
        bindings.entries
            .filter { it.key in DPAD_CONTROLS }
            .map { it.value }
            .toSet()

    if (showButtonPicker && selectedControl != null) {
        val isTrigger = selectedControl == "LT" || selectedControl == "RT"
        val axisKey =
            when (selectedControl) {
                "LT" -> "LT"
                "RT" -> "RT"
                else -> null
            }
        val axisVal =
            when (selectedControl) {
                "LT" -> lt
                "RT" -> rt
                else -> null
            }
        ButtonFunctionPickerDialog(
            controlLabel = selectedControl!!,
            currentFunc = bindings[selectedControl!!],
            assignedFunctions = assignedButtonFuncs,
            gameVariant = gameVariant,
            axisValue = axisVal,
            threshold = axisKey?.let { thresholds[it] },
            onThresholdChange = axisKey?.let { key -> { v: Int -> thresholds[key] = v } },
            axisFunctions = if (isTrigger) AXIS_FUNCTIONS else emptyList(),
            assignedAxisFunctions = if (isTrigger) assignedAxisFuncsForDialog else emptySet(),
            onSelect = { funcLabel ->
                if (isTrigger && funcLabel != null && funcLabel in AXIS_KC_INDEX) {
                    assignAxisFunction(bindings, selectedControl!!, funcLabel)
                } else {
                    assignButtonFunction(bindings, selectedControl!!, funcLabel)
                }
                showButtonPicker = false
                selectedControl = null
            },
            onDismiss = {
                showButtonPicker = false
                selectedControl = null
            },
        )
    }

    if (showStickPicker && selectedControl != null) {
        val xKey = "${selectedControl}_X"
        val yKey = "${selectedControl}_Y"
        val xNegKey = "${xKey}_neg"
        val xPosKey = "${xKey}_pos"
        val yNegKey = "${yKey}_neg"
        val yPosKey = "${yKey}_pos"
        val xIsButtonMode = xNegKey in bindings || xPosKey in bindings
        val yIsButtonMode = yNegKey in bindings || yPosKey in bindings
        StickPickerDialog(
            stickLabel = if (selectedControl == "LS") "Left Stick" else "Right Stick",
            currentXFunc = bindings[xKey],
            currentYFunc = bindings[yKey],
            currentXInvert = xKey in inverts,
            currentYInvert = yKey in inverts,
            currentXButtonMode = xIsButtonMode,
            currentYButtonMode = yIsButtonMode,
            currentXNegFunc = bindings[xNegKey],
            currentXPosFunc = bindings[xPosKey],
            currentYNegFunc = bindings[yNegKey],
            currentYPosFunc = bindings[yPosKey],
            assignedFunctions = assignedAxisFuncsForDialog,
            assignedButtonFunctions = assignedButtonFuncs,
            gameVariant = gameVariant,
            xAxisValue = if (selectedControl == "LS") lx else rx,
            yAxisValue = if (selectedControl == "LS") ly else ry,
            xThreshold = thresholds[xKey] ?: DEFAULT_AXIS_THRESHOLD,
            yThreshold = thresholds[yKey] ?: DEFAULT_AXIS_THRESHOLD,
            onXThresholdChange = { v -> thresholds[xKey] = v },
            onYThresholdChange = { v -> thresholds[yKey] = v },
            onConfirm = { result ->
                // Clear all axis and axis-button bindings for this stick
                bindings.remove(xKey)
                bindings.remove(yKey)
                bindings.remove(xNegKey)
                bindings.remove(xPosKey)
                bindings.remove(yNegKey)
                bindings.remove(yPosKey)
                inverts.remove(xKey)
                inverts.remove(yKey)
                if (result.xButtonMode) {
                    result.xNegFunc?.let { bindings[xNegKey] = it }
                    result.xPosFunc?.let { bindings[xPosKey] = it }
                } else {
                    result.xFunc?.let { assignAxisFunction(bindings, xKey, it) }
                    if (result.xInvert) inverts.add(xKey)
                }
                if (result.yButtonMode) {
                    result.yNegFunc?.let { bindings[yNegKey] = it }
                    result.yPosFunc?.let { bindings[yPosKey] = it }
                } else {
                    result.yFunc?.let { assignAxisFunction(bindings, yKey, it) }
                    if (result.yInvert) inverts.add(yKey)
                }
                showStickPicker = false
                selectedControl = null
            },
            onDismiss = {
                showStickPicker = false
                selectedControl = null
            },
        )
    }

    if (showDpadPicker && selectedControl != null) {
        DpadFunctionPickerDialog(
            directionLabel =
                when (selectedControl) {
                    "DUp" -> "D-Pad Up"
                    "DDown" -> "D-Pad Down"
                    "DLeft" -> "D-Pad Left"
                    "DRight" -> "D-Pad Right"
                    else -> selectedControl!!
                },
            currentFunc = bindings[selectedControl!!],
            assignedFunctions = assignedDpadFuncsForDialog,
            gameVariant = gameVariant,
            onSelect = { funcLabel ->
                assignDpadFunction(bindings, selectedControl!!, funcLabel)
                showDpadPicker = false
                selectedControl = null
            },
            onDismiss = {
                showDpadPicker = false
                selectedControl = null
            },
        )
    }
}

// ── Scroll Indicators ───────────────────────────────────────────────────────

@Composable
private fun BoxScope.ScrollArrows(scrollState: ScrollState) {
    if (scrollState.canScrollBackward) {
        Surface(
            modifier = Modifier.align(Alignment.TopCenter).padding(top = 4.dp),
            shape = CircleShape,
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.85f),
            shadowElevation = 2.dp,
        ) {
            Icon(
                imageVector = Icons.Default.KeyboardArrowUp,
                contentDescription = "Scroll up",
                modifier = Modifier.size(24.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
    if (scrollState.canScrollForward) {
        Surface(
            modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 4.dp),
            shape = CircleShape,
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.85f),
            shadowElevation = 2.dp,
        ) {
            Icon(
                imageVector = Icons.Default.KeyboardArrowDown,
                contentDescription = "Scroll down",
                modifier = Modifier.size(24.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

// ── Picker Dialogs ──────────────────────────────────────────────────────────

@Composable
private fun ButtonFunctionPickerDialog(
    controlLabel: String,
    currentFunc: String?,
    assignedFunctions: Set<String> = emptySet(),
    gameVariant: String = "d2",
    axisValue: Float? = null,
    threshold: Int? = null,
    onThresholdChange: ((Int) -> Unit)? = null,
    axisFunctions: List<String> = emptyList(),
    assignedAxisFunctions: Set<String> = emptySet(),
    onSelect: (String?) -> Unit,
    onDismiss: () -> Unit,
) {
    var showExtra by remember { mutableStateOf(false) }
    val isD1 = gameVariant == "d1"
    val funcList =
        if (showExtra) {
            TouchBindings.META_BUTTON_LABELS.values.toList()
        } else {
            BUTTON_FUNCTIONS
        }
    val isAxisFunc = currentFunc != null && currentFunc in AXIS_KC_INDEX

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Assign: $controlLabel") },
        text = {
            val btnScrollState = rememberScrollState()
            Box(modifier = Modifier.heightIn(max = 400.dp)) {
                Column(modifier = Modifier.fillMaxWidth().verticalScroll(btnScrollState)) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        TextButton(onClick = { showExtra = !showExtra }) {
                            Text(
                                if (showExtra) "Standard" else "Extra",
                                fontSize = PICKER_FONT_SIZE,
                            )
                        }
                    }
                    Row(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .clickable { onSelect(null) },
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        RadioButton(
                            selected = currentFunc == null,
                            onClick = { onSelect(null) },
                            modifier = Modifier.size(PICKER_RADIO_SIZE),
                        )
                        Spacer(Modifier.width(PICKER_RADIO_GAP))
                        Text("None", color = Color.Gray, fontSize = PICKER_FONT_SIZE)
                    }
                    // Axis functions section (for triggers)
                    if (axisFunctions.isNotEmpty()) {
                        Spacer(Modifier.height(4.dp))
                        Text(
                            "Axis Functions",
                            fontSize = 11.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = MaterialTheme.colorScheme.primary,
                            modifier = Modifier.padding(vertical = 2.dp),
                        )
                        for (func in axisFunctions) {
                            val isAssigned = func in assignedAxisFunctions && func != currentFunc
                            Row(
                                modifier =
                                    Modifier
                                        .fillMaxWidth()
                                        .clickable { onSelect(func) },
                                verticalAlignment = Alignment.CenterVertically,
                            ) {
                                RadioButton(
                                    selected = isAxisFunc && currentFunc == func,
                                    onClick = { onSelect(func) },
                                    modifier = Modifier.size(PICKER_RADIO_SIZE),
                                )
                                Spacer(Modifier.width(PICKER_RADIO_GAP))
                                Text(
                                    func,
                                    fontSize = PICKER_FONT_SIZE,
                                    color =
                                        if (!isAssigned && func != currentFunc) {
                                            Color(0xFFEF5350)
                                        } else {
                                            Color.Unspecified
                                        },
                                )
                            }
                        }
                        Spacer(Modifier.height(4.dp))
                        Text(
                            "Button Functions",
                            fontSize = 11.sp,
                            fontWeight = FontWeight.SemiBold,
                            color = MaterialTheme.colorScheme.primary,
                            modifier = Modifier.padding(vertical = 2.dp),
                        )
                    }
                    for (func in funcList) {
                        val isAssigned = func in assignedFunctions && func != currentFunc
                        val isD2Only =
                            isD1 &&
                                if (showExtra) {
                                    func in TouchBindings.D2_ONLY_META_LABELS
                                } else {
                                    func in TouchBindings.D2_ONLY_BUTTON_LABELS
                                }
                        val displayText = if (isD2Only) "$func (D2 only)" else func
                        Row(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .clickable { onSelect(func) },
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            RadioButton(
                                selected = !isAxisFunc && currentFunc == func,
                                onClick = { onSelect(func) },
                                modifier = Modifier.size(PICKER_RADIO_SIZE),
                            )
                            Spacer(Modifier.width(PICKER_RADIO_GAP))
                            Text(
                                displayText,
                                fontSize = PICKER_FONT_SIZE,
                                fontStyle = if (isD2Only) FontStyle.Italic else FontStyle.Normal,
                                color =
                                    if (isD2Only) {
                                        Color(0xFF999999)
                                    } else if (!isAssigned && func != currentFunc) {
                                        Color(0xFFEF5350)
                                    } else {
                                        Color.Unspecified
                                    },
                            )
                        }
                    }
                    if (threshold != null && onThresholdChange != null) {
                        Spacer(Modifier.height(8.dp))
                        Text("Threshold: $threshold%", fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
                        Slider(
                            value = threshold.toFloat(),
                            onValueChange = { onThresholdChange(it.toInt()) },
                            valueRange = 5f..95f,
                            steps = 17,
                            modifier = Modifier.fillMaxWidth(),
                        )
                        AxisThresholdBar(axisValue ?: 0f, threshold)
                    }
                }
                ScrollArrows(btnScrollState)
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

@Composable
private fun StickPickerDialog(
    stickLabel: String,
    currentXFunc: String?,
    currentYFunc: String?,
    currentXInvert: Boolean,
    currentYInvert: Boolean,
    currentXButtonMode: Boolean = false,
    currentYButtonMode: Boolean = false,
    currentXNegFunc: String? = null,
    currentXPosFunc: String? = null,
    currentYNegFunc: String? = null,
    currentYPosFunc: String? = null,
    assignedFunctions: Set<String> = emptySet(),
    assignedButtonFunctions: Set<String> = emptySet(),
    gameVariant: String = "d2",
    xAxisValue: Float = 0f,
    yAxisValue: Float = 0f,
    xThreshold: Int = DEFAULT_AXIS_THRESHOLD,
    yThreshold: Int = DEFAULT_AXIS_THRESHOLD,
    onXThresholdChange: ((Int) -> Unit)? = null,
    onYThresholdChange: ((Int) -> Unit)? = null,
    onConfirm: (StickPickerResult) -> Unit,
    onDismiss: () -> Unit,
) {
    var selectedX by remember { mutableStateOf(currentXFunc) }
    var selectedY by remember { mutableStateOf(currentYFunc) }
    var invertX by remember { mutableStateOf(currentXInvert) }
    var invertY by remember { mutableStateOf(currentYInvert) }
    var xButtonMode by remember { mutableStateOf(currentXButtonMode) }
    var yButtonMode by remember { mutableStateOf(currentYButtonMode) }
    var xNegFunc by remember { mutableStateOf(currentXNegFunc) }
    var xPosFunc by remember { mutableStateOf(currentXPosFunc) }
    var yNegFunc by remember { mutableStateOf(currentYNegFunc) }
    var yPosFunc by remember { mutableStateOf(currentYPosFunc) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stickLabel) },
        text = {
            val stickScrollState = rememberScrollState()
            Box(modifier = Modifier.heightIn(max = 450.dp)) {
                Column(modifier = Modifier.fillMaxWidth().verticalScroll(stickScrollState)) {
                    // -- X Axis --
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            "X Axis (left/right)",
                            fontWeight = FontWeight.Bold,
                            fontSize = 14.sp,
                            modifier = Modifier.weight(1f),
                        )
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Checkbox(checked = xButtonMode, onCheckedChange = { xButtonMode = it })
                        Text("Use as buttons", fontSize = 12.sp)
                        if (!xButtonMode) {
                            Spacer(Modifier.weight(1f))
                            Checkbox(checked = invertX, onCheckedChange = { invertX = it })
                            Text("Invert", fontSize = 12.sp)
                        }
                    }
                    if (xButtonMode) {
                        AxisButtonPicker("Left (neg)", xNegFunc, assignedButtonFunctions, gameVariant) {
                            xNegFunc = it
                        }
                        AxisButtonPicker("Right (pos)", xPosFunc, assignedButtonFunctions, gameVariant) {
                            xPosFunc = it
                        }
                        if (onXThresholdChange != null) {
                            Text("Threshold: $xThreshold%", fontSize = 11.sp)
                            Slider(
                                value = xThreshold.toFloat(),
                                onValueChange = { onXThresholdChange(it.toInt()) },
                                valueRange = 5f..95f,
                                steps = 17,
                                modifier = Modifier.fillMaxWidth(),
                            )
                            AxisThresholdBar(xAxisValue, xThreshold)
                        }
                    } else {
                        AxisFunctionRadioGroup(
                            selected = selectedX,
                            assignedFunctions = assignedFunctions,
                            onSelect = { selectedX = it },
                        )
                    }
                    Spacer(Modifier.height(8.dp))

                    // -- Y Axis --
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            "Y Axis (up/down)",
                            fontWeight = FontWeight.Bold,
                            fontSize = 14.sp,
                            modifier = Modifier.weight(1f),
                        )
                    }
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Checkbox(checked = yButtonMode, onCheckedChange = { yButtonMode = it })
                        Text("Use as buttons", fontSize = 12.sp)
                        if (!yButtonMode) {
                            Spacer(Modifier.weight(1f))
                            Checkbox(checked = invertY, onCheckedChange = { invertY = it })
                            Text("Invert", fontSize = 12.sp)
                        }
                    }
                    if (yButtonMode) {
                        AxisButtonPicker("Up (neg)", yNegFunc, assignedButtonFunctions, gameVariant) {
                            yNegFunc = it
                        }
                        AxisButtonPicker("Down (pos)", yPosFunc, assignedButtonFunctions, gameVariant) {
                            yPosFunc = it
                        }
                        if (onYThresholdChange != null) {
                            Text("Threshold: $yThreshold%", fontSize = 11.sp)
                            Slider(
                                value = yThreshold.toFloat(),
                                onValueChange = { onYThresholdChange(it.toInt()) },
                                valueRange = 5f..95f,
                                steps = 17,
                                modifier = Modifier.fillMaxWidth(),
                            )
                            AxisThresholdBar(yAxisValue, yThreshold)
                        }
                    } else {
                        AxisFunctionRadioGroup(
                            selected = selectedY,
                            assignedFunctions = assignedFunctions,
                            onSelect = { selectedY = it },
                        )
                    }
                }
                ScrollArrows(stickScrollState)
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    onConfirm(
                        StickPickerResult(
                            xFunc = if (xButtonMode) null else selectedX,
                            yFunc = if (yButtonMode) null else selectedY,
                            xInvert = invertX,
                            yInvert = invertY,
                            xButtonMode = xButtonMode,
                            yButtonMode = yButtonMode,
                            xNegFunc = xNegFunc,
                            xPosFunc = xPosFunc,
                            yNegFunc = yNegFunc,
                            yPosFunc = yPosFunc,
                        ),
                    )
                },
            ) { Text("OK") }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

// Compact function picker for axis-as-buttons within StickPickerDialog
@Composable
private fun AxisButtonPicker(
    label: String,
    currentFunc: String?,
    assignedFunctions: Set<String> = emptySet(),
    gameVariant: String = "d2",
    onSelect: (String?) -> Unit,
) {
    var showPicker by remember { mutableStateOf(false) }
    Row(
        modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(label, fontSize = 12.sp, modifier = Modifier.width(90.dp))
        TextButton(onClick = { showPicker = true }) {
            Text(
                currentFunc ?: "None",
                fontSize = 12.sp,
                color = if (currentFunc != null) Color.Unspecified else Color.Gray,
            )
        }
    }
    if (showPicker) {
        ButtonFunctionPickerDialog(
            controlLabel = label,
            currentFunc = currentFunc,
            assignedFunctions = assignedFunctions,
            gameVariant = gameVariant,
            onSelect = { func ->
                onSelect(func)
                showPicker = false
            },
            onDismiss = { showPicker = false },
        )
    }
}

@Composable
private fun AxisFunctionRadioGroup(
    selected: String?,
    assignedFunctions: Set<String> = emptySet(),
    onSelect: (String?) -> Unit,
) {
    Row(
        modifier =
            Modifier
                .fillMaxWidth()
                .clickable { onSelect(null) },
        verticalAlignment = Alignment.CenterVertically,
    ) {
        RadioButton(
            selected = selected == null,
            onClick = { onSelect(null) },
            modifier = Modifier.size(PICKER_RADIO_SIZE),
        )
        Spacer(Modifier.width(PICKER_RADIO_GAP))
        Text("None", color = Color.Gray, fontSize = PICKER_FONT_SIZE)
    }
    for (func in AXIS_FUNCTIONS) {
        val isAssigned = func in assignedFunctions && func != selected
        Row(
            modifier =
                Modifier
                    .fillMaxWidth()
                    .clickable { onSelect(func) },
            verticalAlignment = Alignment.CenterVertically,
        ) {
            RadioButton(
                selected = selected == func,
                onClick = { onSelect(func) },
                modifier = Modifier.size(PICKER_RADIO_SIZE),
            )
            Spacer(Modifier.width(PICKER_RADIO_GAP))
            Text(
                func,
                fontSize = PICKER_FONT_SIZE,
                color =
                    if (!isAssigned && func != selected) {
                        Color(0xFFEF5350)
                    } else {
                        Color.Unspecified
                    },
            )
        }
    }
}

@Composable
private fun DpadFunctionPickerDialog(
    directionLabel: String,
    currentFunc: String?,
    assignedFunctions: Set<String> = emptySet(),
    gameVariant: String = "d2",
    onSelect: (String?) -> Unit,
    onDismiss: () -> Unit,
) {
    var showExtra by remember { mutableStateOf(false) }
    val isD1 = gameVariant == "d1"
    val funcList =
        if (showExtra) {
            TouchBindings.META_BUTTON_LABELS.values.toList()
        } else {
            KB_FUNCTIONS
        }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Assign: $directionLabel") },
        text = {
            val dpadScrollState = rememberScrollState()
            Box(modifier = Modifier.heightIn(max = 400.dp)) {
                Column(modifier = Modifier.fillMaxWidth().verticalScroll(dpadScrollState)) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        TextButton(onClick = { showExtra = !showExtra }) {
                            Text(
                                if (showExtra) "Standard" else "Extra",
                                fontSize = PICKER_FONT_SIZE,
                            )
                        }
                    }
                    Row(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .clickable { onSelect(null) },
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        RadioButton(
                            selected = currentFunc == null,
                            onClick = { onSelect(null) },
                            modifier = Modifier.size(PICKER_RADIO_SIZE),
                        )
                        Spacer(Modifier.width(PICKER_RADIO_GAP))
                        Text("None", color = Color.Gray, fontSize = PICKER_FONT_SIZE)
                    }
                    for (func in funcList) {
                        val isAssigned = func in assignedFunctions && func != currentFunc
                        val isD2Only =
                            isD1 &&
                                if (showExtra) {
                                    func in TouchBindings.D2_ONLY_META_LABELS
                                } else {
                                    func in TouchBindings.D2_ONLY_BUTTON_LABELS
                                }
                        val displayText = if (isD2Only) "$func (D2 only)" else func
                        Row(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .clickable { onSelect(func) },
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            RadioButton(
                                selected = currentFunc == func,
                                onClick = { onSelect(func) },
                                modifier = Modifier.size(PICKER_RADIO_SIZE),
                            )
                            Spacer(Modifier.width(PICKER_RADIO_GAP))
                            Text(
                                displayText,
                                fontSize = PICKER_FONT_SIZE,
                                fontStyle = if (isD2Only) FontStyle.Italic else FontStyle.Normal,
                                color =
                                    if (isD2Only) {
                                        Color(0xFF999999)
                                    } else if (!isAssigned && func != currentFunc) {
                                        Color(0xFFEF5350)
                                    } else {
                                        Color.Unspecified
                                    },
                            )
                        }
                    }
                }
                ScrollArrows(dpadScrollState)
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

// ── Threshold Bar ───────────────────────────────────────────────────────────

@Composable
private fun AxisThresholdBar(
    axisValue: Float,
    thresholdPct: Int,
    modifier: Modifier = Modifier,
) {
    val absVal = abs(axisValue)
    val thresholdFrac = thresholdPct / 100f
    val aboveThreshold = absVal >= thresholdFrac
    Canvas(
        modifier =
            modifier
                .fillMaxWidth()
                .height(24.dp),
    ) {
        val w = size.width
        val h = size.height
        // Background
        drawRoundRect(Color(0xFF333333), cornerRadius = CornerRadius(4f, 4f))
        // Fill
        val fillW = w * absVal
        if (fillW > 0f) {
            val fillColor = if (aboveThreshold) Color(0xFF4CAF50) else Color(0xFFEF5350)
            drawRoundRect(fillColor, size = Size(fillW, h), cornerRadius = CornerRadius(4f, 4f))
        }
        // Threshold marker
        val markerX = w * thresholdFrac
        drawLine(Color.White, Offset(markerX, 0f), Offset(markerX, h), strokeWidth = 2f)
    }
}

// ── Canvas drawing helpers ──────────────────────────────────────────────────

private fun DrawScope.drawStick(
    cx: Float,
    cy: Float,
    radius: Float,
    deflectX: Float,
    deflectY: Float,
    label: String,
) {
    drawCircle(color = cStickBg, radius = radius, center = Offset(cx, cy))
    drawCircle(color = cOutline, radius = radius, center = Offset(cx, cy), style = Stroke(1.5f))

    val dotR = radius * 0.4f
    val dotX = cx + deflectX * radius * 0.7f
    val dotY = cy + deflectY * radius * 0.7f
    val isDeflected = abs(deflectX) > 0.1f || abs(deflectY) > 0.1f
    drawCircle(
        color = if (isDeflected) cStickDotActive else cStickDot,
        radius = dotR,
        center = Offset(dotX, dotY),
    )
}

private fun DrawScope.drawDpad(
    cx: Float,
    cy: Float,
    armLen: Float,
    hatX: Float,
    hatY: Float,
) {
    val armW = armLen * 0.35f
    val arms =
        listOf(
            Triple(Offset(cx - armW / 2, cy - armLen), Size(armW, armLen), hatY < -0.5f),
            Triple(Offset(cx - armW / 2, cy), Size(armW, armLen), hatY > 0.5f),
            Triple(Offset(cx - armLen, cy - armW / 2), Size(armLen, armW), hatX < -0.5f),
            Triple(Offset(cx, cy - armW / 2), Size(armLen, armW), hatX > 0.5f),
        )
    drawRect(color = cDpadBg, topLeft = Offset(cx - armW / 2, cy - armW / 2), size = Size(armW, armW))
    for ((offset, sz, active) in arms) {
        drawRect(color = if (active) cDpadActive else cDpadBg, topLeft = offset, size = sz)
        drawRect(color = cOutline, topLeft = offset, size = sz, style = Stroke(1f))
    }
}

private fun DrawScope.drawFaceButton(
    textMeasurer: androidx.compose.ui.text.TextMeasurer,
    cx: Float,
    cy: Float,
    radius: Float,
    label: String,
    pressed: Boolean,
    scale: Float,
) {
    drawCircle(
        color = if (pressed) cActive else cInactive,
        radius = radius,
        center = Offset(cx, cy),
    )
    drawCircle(color = cOutline, radius = radius, center = Offset(cx, cy), style = Stroke(1f))
    val style =
        TextStyle(
            color = cLabel,
            fontSize = (scale * 0.016f).sp,
            fontWeight = FontWeight.Bold,
        )
    val result = textMeasurer.measure(label, style)
    drawText(
        textLayoutResult = result,
        topLeft = Offset(cx - result.size.width / 2f, cy - result.size.height / 2f),
    )
}

private fun DrawScope.drawTrigger(
    x: Float,
    y: Float,
    w: Float,
    h: Float,
    value: Float,
) {
    drawRoundRect(
        color = cTriggerBg,
        topLeft = Offset(x, y),
        size = Size(w, h),
        cornerRadius = CornerRadius(h / 2f),
    )
    if (value > 0.02f) {
        val fillW = w * value.coerceIn(0f, 1f)
        drawRoundRect(
            color = cTriggerFill,
            topLeft = Offset(x, y),
            size = Size(fillW, h),
            cornerRadius = CornerRadius(h / 2f),
        )
    }
    drawRoundRect(
        color = cOutline,
        topLeft = Offset(x, y),
        size = Size(w, h),
        cornerRadius = CornerRadius(h / 2f),
        style = Stroke(1f),
    )
}

private fun DrawScope.drawLabel(
    textMeasurer: androidx.compose.ui.text.TextMeasurer,
    text: String,
    cx: Float,
    cy: Float,
    scale: Float,
) {
    val style =
        TextStyle(
            color = cLabel,
            fontSize = (scale * 0.022f).sp,
            fontWeight = FontWeight.Medium,
        )
    val result = textMeasurer.measure(text, style)
    drawText(
        textLayoutResult = result,
        topLeft = Offset(cx - result.size.width / 2f, cy - result.size.height / 2f),
    )
}

private fun DrawScope.drawFuncLabel(
    textMeasurer: androidx.compose.ui.text.TextMeasurer,
    text: String,
    cx: Float,
    cy: Float,
    scale: Float,
    color: Color = cAssignLabel,
) {
    val style =
        TextStyle(
            color = color,
            fontSize = (scale * 0.017f).sp,
            fontWeight = FontWeight.Normal,
        )
    val result = textMeasurer.measure(text, style)
    drawText(
        textLayoutResult = result,
        topLeft = Offset(cx - result.size.width / 2f, cy - result.size.height / 2f),
    )
}

// Right edge at x=rightX, vertically centered at cy
private fun DrawScope.drawFuncLabelRightAligned(
    textMeasurer: androidx.compose.ui.text.TextMeasurer,
    text: String,
    rightX: Float,
    cy: Float,
    scale: Float,
    color: Color = cAssignLabel,
) {
    val style =
        TextStyle(
            color = color,
            fontSize = (scale * 0.017f).sp,
            fontWeight = FontWeight.Normal,
        )
    val result = textMeasurer.measure(text, style)
    drawText(
        textLayoutResult = result,
        topLeft = Offset(rightX - result.size.width, cy - result.size.height / 2f),
    )
}

// Left edge at x=leftX, vertically centered at cy
private fun DrawScope.drawFuncLabelLeftAligned(
    textMeasurer: androidx.compose.ui.text.TextMeasurer,
    text: String,
    leftX: Float,
    cy: Float,
    scale: Float,
    color: Color = cAssignLabel,
) {
    val style =
        TextStyle(
            color = color,
            fontSize = (scale * 0.017f).sp,
            fontWeight = FontWeight.Normal,
        )
    val result = textMeasurer.measure(text, style)
    drawText(
        textLayoutResult = result,
        topLeft = Offset(leftX, cy - result.size.height / 2f),
    )
}
