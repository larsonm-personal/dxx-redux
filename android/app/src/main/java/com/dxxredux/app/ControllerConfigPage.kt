package com.dxxredux.app

import android.content.Context
import android.widget.Toast
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import java.io.File
import kotlin.math.abs
import kotlin.math.min

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
private val cAssignLabel = Color(0xFFFFAB40)  // amber for function labels
private val cHighlight = Color(0x44FFFF00)    // translucent yellow for selection

// ── Data Model ──────────────────────────────────────────────────────────────

// Virtual button indices for each physical button control
private val BUTTON_CONTROLS = linkedMapOf(
    "A" to 0, "B" to 1, "X" to 2, "Y" to 3,
    "L1" to 4, "R1" to 5, "Select" to 6, "Start" to 7,
    "L3" to 8, "R3" to 9,
    "LT" to 19, "RT" to 21
)

// Virtual axis indices for each physical stick axis
private val AXIS_CONTROLS = linkedMapOf(
    "LS_X" to 0, "LS_Y" to 1,
    "RS_X" to 2, "RS_Y" to 3
)

private data class BtnFunc(val kcIndex: Int, val label: String)
private data class AxisFunc(val kcIndex: Int, val label: String)

private val BUTTON_FUNCTIONS = listOf(
    BtnFunc(0, "Fire Primary"),
    BtnFunc(1, "Fire Secondary"),
    BtnFunc(2, "Accelerate"),
    BtnFunc(3, "Reverse"),
    BtnFunc(4, "Fire Flare"),
    BtnFunc(5, "Slide On"),
    BtnFunc(6, "Slide Left"),
    BtnFunc(7, "Slide Right"),
    BtnFunc(8, "Slide Up"),
    BtnFunc(9, "Slide Down"),
    BtnFunc(10, "Bank On"),
    BtnFunc(11, "Bank Left"),
    BtnFunc(12, "Bank Right"),
    BtnFunc(25, "Rear View"),
    BtnFunc(26, "Drop Bomb"),
    BtnFunc(27, "Afterburner"),
    BtnFunc(28, "Cycle Primary"),
    BtnFunc(29, "Cycle Secondary"),
    BtnFunc(30, "Headlight"),
    BtnFunc(50, "Automap"),
    BtnFunc(52, "Energy\u2192Shield"),
    BtnFunc(54, "Toggle Bomb"),
)

private val AXIS_FUNCTIONS = listOf(
    AxisFunc(13, "Pitch U/D"),
    AxisFunc(15, "Turn L/R"),
    AxisFunc(17, "Slide L/R"),
    AxisFunc(19, "Slide U/D"),
    AxisFunc(21, "Bank L/R"),
    AxisFunc(23, "Throttle"),
)

private val DEFAULT_BINDINGS = mapOf(
    "A" to "Fire Primary",
    "B" to "Fire Secondary",
    "RT" to "Accelerate",
    "LT" to "Reverse",
    "RS_X" to "Turn L/R",
    "RS_Y" to "Pitch U/D",
    "LS_X" to "Slide L/R",
    "LS_Y" to "Slide U/D",
)

// ── Config file I/O ─────────────────────────────────────────────────────────

private const val CONFIG_FILENAME = "gamepad_bindings.cfg"

private fun bindingsToKeySettings(bindings: Map<String, String>): Map<Int, Int> {
    val ks = mutableMapOf<Int, Int>()
    for ((controlId, funcLabel) in bindings) {
        val btnIdx = BUTTON_CONTROLS[controlId]
        if (btnIdx != null) {
            val func = BUTTON_FUNCTIONS.find { it.label == funcLabel }
            if (func != null) ks[func.kcIndex] = btnIdx
            continue
        }
        val axisIdx = AXIS_CONTROLS[controlId]
        if (axisIdx != null) {
            val func = AXIS_FUNCTIONS.find { it.label == funcLabel }
            if (func != null) ks[func.kcIndex] = axisIdx
        }
    }
    return ks
}

private fun saveConfig(context: Context, bindings: Map<String, String>) {
    val ks = bindingsToKeySettings(bindings)
    val sb = StringBuilder()
    sb.appendLine("# gamepad_bindings.cfg")
    for ((idx, value) in ks.toSortedMap()) {
        sb.appendLine("$idx=$value")
    }
    File(context.filesDir, CONFIG_FILENAME).writeText(sb.toString())
}

private fun loadConfig(context: Context): Map<String, String>? {
    val file = File(context.filesDir, CONFIG_FILENAME)
    if (!file.exists()) return null

    val ks = mutableMapOf<Int, Int>()
    file.readLines().forEach { line ->
        val trimmed = line.trim()
        if (trimmed.isEmpty() || trimmed.startsWith("#")) return@forEach
        val parts = trimmed.split("=", limit = 2)
        if (parts.size == 2) {
            val idx = parts[0].toIntOrNull()
            val value = parts[1].toIntOrNull()
            if (idx != null && value != null) ks[idx] = value
        }
    }

    // Reverse-map: KeySettings indices back to control→function
    val bindings = mutableMapOf<String, String>()
    for (func in BUTTON_FUNCTIONS) {
        val virtualBtn = ks[func.kcIndex] ?: continue
        val controlId = BUTTON_CONTROLS.entries.find { it.value == virtualBtn }?.key ?: continue
        bindings[controlId] = func.label
    }
    for (func in AXIS_FUNCTIONS) {
        val virtualAxis = ks[func.kcIndex] ?: continue
        val controlId = AXIS_CONTROLS.entries.find { it.value == virtualAxis }?.key ?: continue
        bindings[controlId] = func.label
    }
    return bindings
}

// ── Assignment logic ────────────────────────────────────────────────────────

private fun assignButtonFunction(
    bindings: MutableMap<String, String>, controlId: String, funcLabel: String?
) {
    if (funcLabel == null) {
        bindings.remove(controlId)
        return
    }
    // Clear this function from any other button control
    val existing = bindings.entries.find {
        it.value == funcLabel && it.key != controlId && it.key in BUTTON_CONTROLS
    }
    if (existing != null) bindings.remove(existing.key)
    bindings[controlId] = funcLabel
}

private fun assignAxisFunction(
    bindings: MutableMap<String, String>, axisControlId: String, funcLabel: String?
) {
    if (funcLabel == null) {
        bindings.remove(axisControlId)
        return
    }
    // Clear this function from any other axis control
    val existing = bindings.entries.find {
        it.value == funcLabel && it.key != axisControlId && it.key in AXIS_CONTROLS
    }
    if (existing != null) bindings.remove(existing.key)
    bindings[axisControlId] = funcLabel
}

// ── Abbreviations for on-canvas labels ──────────────────────────────────────

private fun abbreviate(label: String): String = when (label) {
    "Fire Primary" -> "Fire1"
    "Fire Secondary" -> "Fire2"
    "Accelerate" -> "Accel"
    "Reverse" -> "Rev"
    "Fire Flare" -> "Flare"
    "Slide On" -> "SldOn"
    "Slide Left" -> "Sld L"
    "Slide Right" -> "Sld R"
    "Slide Up" -> "Sld U"
    "Slide Down" -> "Sld D"
    "Bank On" -> "BnkOn"
    "Bank Left" -> "Bnk L"
    "Bank Right" -> "Bnk R"
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
    else -> label.take(5)
}

// ── Main Composable ─────────────────────────────────────────────────────────

@Composable
fun ControllerConfigPage(
    axes: FloatArray,
    dpadAxes: FloatArray,
    axisGeneration: Int,
    pressedButtons: SnapshotStateList<String>,
    onBack: () -> Unit
) {
    @Suppress("UNUSED_EXPRESSION") axisGeneration

    val context = LocalContext.current
    val lx = axes[0]; val ly = axes[1]
    val rx = axes[2]; val ry = axes[3]
    val lt = axes[4]; val rt = axes[5]
    val hatX = dpadAxes[0]; val hatY = dpadAxes[1]

    // Bindings state: control ID → function label
    val bindings = remember { mutableStateMapOf<String, String>() }
    var initialized by remember { mutableStateOf(false) }
    if (!initialized) {
        val saved = loadConfig(context)
        bindings.putAll(saved ?: DEFAULT_BINDINGS)
        initialized = true
    }

    // Touch/dialog state
    val controlBounds = remember { mutableMapOf<String, Rect>() }
    var selectedControl by remember { mutableStateOf<String?>(null) }
    var showButtonPicker by remember { mutableStateOf(false) }
    var showStickPicker by remember { mutableStateOf(false) }

    Surface(
        modifier = Modifier.fillMaxSize(),
        color = MaterialTheme.colorScheme.background
    ) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .safeDrawingPadding()
                .padding(12.dp)
        ) {
            // ── Header ──
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                TextButton(onClick = onBack) {
                    Text("\u2190 Back", fontSize = 14.sp)
                }
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    text = "Controller Layout",
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.primary
                )
            }
            Spacer(modifier = Modifier.height(2.dp))
            Text(
                text = "Tap any control to assign a function",
                fontSize = 13.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Spacer(modifier = Modifier.height(6.dp))

            // ── Controller graphic with touch detection ──
            val textMeasurer = rememberTextMeasurer()

            Canvas(
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .background(Color(0xFF121212))
                    .pointerInput(Unit) {
                        detectTapGestures { offset ->
                            for ((id, rect) in controlBounds) {
                                if (rect.contains(offset)) {
                                    selectedControl = id
                                    if (id == "LS" || id == "RS") {
                                        showStickPicker = true
                                    } else {
                                        showButtonPicker = true
                                    }
                                    break
                                }
                            }
                        }
                    }
            ) {
                val w = size.width
                val h = size.height
                val scale = min(w, h)

                val phoneW = w * 0.30f
                val phoneH = h * 0.55f
                val phoneX = (w - phoneW) / 2f
                val phoneY = (h - phoneH) / 2f

                val gripW = w * 0.22f
                val gripH = h * 0.65f
                val gripY = (h - gripH) / 2f
                val leftGripX = phoneX - gripW + 8f
                val rightGripX = phoneX + phoneW - 8f

                // ── Grips ──
                drawRoundRect(
                    color = cFill, topLeft = Offset(leftGripX, gripY),
                    size = Size(gripW, gripH), cornerRadius = CornerRadius(scale * 0.04f)
                )
                drawRoundRect(
                    color = cOutline, topLeft = Offset(leftGripX, gripY),
                    size = Size(gripW, gripH), cornerRadius = CornerRadius(scale * 0.04f),
                    style = Stroke(2f)
                )
                drawRoundRect(
                    color = cFill, topLeft = Offset(rightGripX, gripY),
                    size = Size(gripW, gripH), cornerRadius = CornerRadius(scale * 0.04f)
                )
                drawRoundRect(
                    color = cOutline, topLeft = Offset(rightGripX, gripY),
                    size = Size(gripW, gripH), cornerRadius = CornerRadius(scale * 0.04f),
                    style = Stroke(2f)
                )

                // ── Phone body ──
                drawRoundRect(
                    color = cPhoneBody, topLeft = Offset(phoneX, phoneY),
                    size = Size(phoneW, phoneH), cornerRadius = CornerRadius(scale * 0.02f)
                )
                val screenMargin = scale * 0.015f
                drawRoundRect(
                    color = cPhoneScreen,
                    topLeft = Offset(phoneX + screenMargin, phoneY + screenMargin),
                    size = Size(phoneW - screenMargin * 2, phoneH - screenMargin * 2),
                    cornerRadius = CornerRadius(scale * 0.01f)
                )

                // ── Left grip controls ──
                val lgCx = leftGripX + gripW / 2f
                val stickR = scale * 0.055f
                val touchPad = scale * 0.03f  // extra padding for touch targets

                // Left stick
                val lsCx = lgCx
                val lsCy = gripY + gripH * 0.30f
                drawStick(lsCx, lsCy, stickR, lx, ly, "LS")
                controlBounds["LS"] = Rect(
                    lsCx - stickR - touchPad, lsCy - stickR - touchPad,
                    lsCx + stickR + touchPad, lsCy + stickR + touchPad
                )
                // Show axis assignments on stick
                val lsxFunc = bindings["LS_X"]
                val lsyFunc = bindings["LS_Y"]
                if (lsxFunc != null || lsyFunc != null) {
                    val lsLabel = buildString {
                        if (lsxFunc != null) append("X:${abbreviate(lsxFunc)}")
                        if (lsxFunc != null && lsyFunc != null) append(" ")
                        if (lsyFunc != null) append("Y:${abbreviate(lsyFunc)}")
                    }
                    drawFuncLabel(textMeasurer, lsLabel, lsCx, lsCy + stickR + scale * 0.03f, scale)
                }
                // Highlight if selected
                if (selectedControl == "LS") {
                    drawCircle(cHighlight, stickR + touchPad, Offset(lsCx, lsCy))
                }

                // D-pad (non-interactive)
                val dpadCx = lgCx
                val dpadCy = gripY + gripH * 0.65f
                val dpadSize = scale * 0.07f
                drawDpad(dpadCx, dpadCy, dpadSize, hatX, hatY)

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
                    cornerRadius = CornerRadius(bumperH / 2f)
                )
                controlBounds["L1"] = Rect(l1X - touchPad, l1Y - touchPad,
                    l1X + bumperW + touchPad, l1Y + bumperH + touchPad)
                drawLabel(textMeasurer, "L1", l1X + bumperW / 2f, l1Y + bumperH + scale * 0.012f, scale)
                bindings["L1"]?.let {
                    drawFuncLabel(textMeasurer, abbreviate(it), l1X + bumperW / 2f,
                        l1Y + bumperH + scale * 0.035f, scale)
                }

                // LT trigger
                val triggerW = gripW * 0.5f
                val triggerH = scale * 0.03f
                val l2X = lgCx - triggerW / 2f
                val l2Y = gripY - triggerH - scale * 0.015f
                drawTrigger(l2X, l2Y, triggerW, triggerH, lt)
                controlBounds["LT"] = Rect(l2X - touchPad, l2Y - touchPad,
                    l2X + triggerW + touchPad, l2Y + triggerH + touchPad)
                drawLabel(textMeasurer, "LT", l2X + triggerW / 2f, l2Y - scale * 0.02f, scale)
                bindings["LT"]?.let {
                    drawFuncLabel(textMeasurer, abbreviate(it), l2X + triggerW / 2f,
                        l2Y - scale * 0.045f, scale)
                }

                // L3 (stick press)
                val l3Pressed = "L3" in pressedButtons
                val l3Cx = lsCx
                val l3Cy = lsCy + stickR + scale * 0.015f
                drawCircle(
                    color = if (l3Pressed) cActive else cInactive.copy(alpha = 0.4f),
                    radius = scale * 0.010f, center = Offset(l3Cx, l3Cy)
                )
                controlBounds["L3"] = Rect(l3Cx - touchPad, l3Cy - touchPad,
                    l3Cx + touchPad, l3Cy + touchPad)

                // ── Right grip controls ──
                val rgCx = rightGripX + gripW / 2f

                // Right stick
                val rsCx = rgCx
                val rsCy = gripY + gripH * 0.65f
                drawStick(rsCx, rsCy, stickR, rx, ry, "RS")
                controlBounds["RS"] = Rect(
                    rsCx - stickR - touchPad, rsCy - stickR - touchPad,
                    rsCx + stickR + touchPad, rsCy + stickR + touchPad
                )
                val rsxFunc = bindings["RS_X"]
                val rsyFunc = bindings["RS_Y"]
                if (rsxFunc != null || rsyFunc != null) {
                    val rsLabel = buildString {
                        if (rsxFunc != null) append("X:${abbreviate(rsxFunc)}")
                        if (rsxFunc != null && rsyFunc != null) append(" ")
                        if (rsyFunc != null) append("Y:${abbreviate(rsyFunc)}")
                    }
                    drawFuncLabel(textMeasurer, rsLabel, rsCx, rsCy + stickR + scale * 0.03f, scale)
                }
                if (selectedControl == "RS") {
                    drawCircle(cHighlight, stickR + touchPad, Offset(rsCx, rsCy))
                }

                // Face buttons ABXY
                val btnCx = rgCx
                val btnCy = gripY + gripH * 0.30f
                val btnSpacing = scale * 0.042f
                val btnR = scale * 0.022f

                data class FaceBtn(val id: String, val cx: Float, val cy: Float)
                val faceButtons = listOf(
                    FaceBtn("Y", btnCx, btnCy - btnSpacing),
                    FaceBtn("A", btnCx, btnCy + btnSpacing),
                    FaceBtn("X", btnCx - btnSpacing, btnCy),
                    FaceBtn("B", btnCx + btnSpacing, btnCy),
                )
                for (fb in faceButtons) {
                    val pressed = fb.id in pressedButtons
                    drawFaceButton(fb.cx, fb.cy, btnR, fb.id, pressed)
                    controlBounds[fb.id] = Rect(
                        fb.cx - btnR - touchPad, fb.cy - btnR - touchPad,
                        fb.cx + btnR + touchPad, fb.cy + btnR + touchPad
                    )
                    bindings[fb.id]?.let {
                        drawFuncLabel(textMeasurer, abbreviate(it), fb.cx,
                            fb.cy + btnR + scale * 0.025f, scale)
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
                    cornerRadius = CornerRadius(bumperH / 2f)
                )
                controlBounds["R1"] = Rect(r1X - touchPad, r1Y - touchPad,
                    r1X + bumperW + touchPad, r1Y + bumperH + touchPad)
                drawLabel(textMeasurer, "R1", r1X + bumperW / 2f, r1Y + bumperH + scale * 0.012f, scale)
                bindings["R1"]?.let {
                    drawFuncLabel(textMeasurer, abbreviate(it), r1X + bumperW / 2f,
                        r1Y + bumperH + scale * 0.035f, scale)
                }

                // RT trigger
                val r2X = rgCx - triggerW / 2f
                val r2Y = gripY - triggerH - scale * 0.015f
                drawTrigger(r2X, r2Y, triggerW, triggerH, rt)
                controlBounds["RT"] = Rect(r2X - touchPad, r2Y - touchPad,
                    r2X + triggerW + touchPad, r2Y + triggerH + touchPad)
                drawLabel(textMeasurer, "RT", r2X + triggerW / 2f, r2Y - scale * 0.02f, scale)
                bindings["RT"]?.let {
                    drawFuncLabel(textMeasurer, abbreviate(it), r2X + triggerW / 2f,
                        r2Y - scale * 0.045f, scale)
                }

                // R3 (stick press)
                val r3Cx = rsCx
                val r3Cy = rsCy + stickR + scale * 0.015f
                val r3Pressed = "R3" in pressedButtons
                drawCircle(
                    color = if (r3Pressed) cActive else cInactive.copy(alpha = 0.4f),
                    radius = scale * 0.010f, center = Offset(r3Cx, r3Cy)
                )
                controlBounds["R3"] = Rect(r3Cx - touchPad, r3Cy - touchPad,
                    r3Cx + touchPad, r3Cy + touchPad)

                // ── Center buttons (Select / Start) ──
                val centerY = gripY + gripH * 0.12f
                val centerBtnR = scale * 0.012f
                val selX = phoneX + phoneW * 0.25f
                val staX = phoneX + phoneW * 0.75f

                val selPressed = "Select" in pressedButtons
                drawCircle(
                    color = if (selPressed) cActive else cInactive,
                    radius = centerBtnR, center = Offset(selX, centerY)
                )
                controlBounds["Select"] = Rect(
                    selX - centerBtnR - touchPad, centerY - centerBtnR - touchPad,
                    selX + centerBtnR + touchPad, centerY + centerBtnR + touchPad
                )
                drawLabel(textMeasurer, "Sel", selX, centerY + centerBtnR + scale * 0.015f, scale)
                bindings["Select"]?.let {
                    drawFuncLabel(textMeasurer, abbreviate(it), selX,
                        centerY + centerBtnR + scale * 0.04f, scale)
                }

                val staPressed = "Start" in pressedButtons
                drawCircle(
                    color = if (staPressed) cActive else cInactive,
                    radius = centerBtnR, center = Offset(staX, centerY)
                )
                controlBounds["Start"] = Rect(
                    staX - centerBtnR - touchPad, centerY - centerBtnR - touchPad,
                    staX + centerBtnR + touchPad, centerY + centerBtnR + touchPad
                )
                drawLabel(textMeasurer, "Sta", staX, centerY + centerBtnR + scale * 0.015f, scale)
                bindings["Start"]?.let {
                    drawFuncLabel(textMeasurer, abbreviate(it), staX,
                        centerY + centerBtnR + scale * 0.04f, scale)
                }
            }

            Spacer(modifier = Modifier.height(6.dp))

            // ── Live readout ──
            val activeButtonsStr = pressedButtons.joinToString(", ").ifEmpty { "none" }
            Text(
                text = "Buttons: $activeButtonsStr",
                fontSize = 11.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Text(
                text = "L-Stick: (${"%.2f".format(lx)}, ${"%.2f".format(ly)})  " +
                       "R-Stick: (${"%.2f".format(rx)}, ${"%.2f".format(ry)})",
                fontSize = 11.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Text(
                text = "Triggers: LT=${"%.2f".format(lt)}  RT=${"%.2f".format(rt)}  " +
                       "D-Pad: (${"%.1f".format(hatX)}, ${"%.1f".format(hatY)})",
                fontSize = 11.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )

            Spacer(modifier = Modifier.height(8.dp))

            // ── Save & Apply button ──
            Button(
                onClick = {
                    saveConfig(context, bindings.toMap())
                    Toast.makeText(
                        context,
                        "Saved! Controls will apply to all pilots on next game load.",
                        Toast.LENGTH_LONG
                    ).show()
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .height(52.dp),
                colors = ButtonDefaults.buttonColors(
                    containerColor = MaterialTheme.colorScheme.primary
                )
            ) {
                Text(
                    text = "ASSIGN CONTROLS TO ALL PILOT CONFIGS",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold
                )
            }
        }
    }

    // ── Dialogs ──

    if (showButtonPicker && selectedControl != null) {
        ButtonFunctionPickerDialog(
            controlLabel = selectedControl!!,
            currentFunc = bindings[selectedControl!!],
            onSelect = { funcLabel ->
                assignButtonFunction(bindings, selectedControl!!, funcLabel)
                showButtonPicker = false
                selectedControl = null
            },
            onDismiss = {
                showButtonPicker = false
                selectedControl = null
            }
        )
    }

    if (showStickPicker && selectedControl != null) {
        val xKey = "${selectedControl}_X"
        val yKey = "${selectedControl}_Y"
        StickPickerDialog(
            stickLabel = if (selectedControl == "LS") "Left Stick" else "Right Stick",
            currentXFunc = bindings[xKey],
            currentYFunc = bindings[yKey],
            onConfirm = { xFunc, yFunc ->
                assignAxisFunction(bindings, xKey, xFunc)
                assignAxisFunction(bindings, yKey, yFunc)
                showStickPicker = false
                selectedControl = null
            },
            onDismiss = {
                showStickPicker = false
                selectedControl = null
            }
        )
    }
}

// ── Picker Dialogs ──────────────────────────────────────────────────────────

@Composable
private fun ButtonFunctionPickerDialog(
    controlLabel: String,
    currentFunc: String?,
    onSelect: (String?) -> Unit,
    onDismiss: () -> Unit
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Assign: $controlLabel") },
        text = {
            LazyColumn(modifier = Modifier.heightIn(max = 400.dp)) {
                item {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable { onSelect(null) }
                            .padding(vertical = 6.dp, horizontal = 4.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        RadioButton(selected = currentFunc == null, onClick = { onSelect(null) })
                        Spacer(Modifier.width(8.dp))
                        Text("None", color = Color.Gray)
                    }
                }
                items(BUTTON_FUNCTIONS) { func ->
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable { onSelect(func.label) }
                            .padding(vertical = 6.dp, horizontal = 4.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        RadioButton(
                            selected = currentFunc == func.label,
                            onClick = { onSelect(func.label) }
                        )
                        Spacer(Modifier.width(8.dp))
                        Text(func.label)
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        }
    )
}

@Composable
private fun StickPickerDialog(
    stickLabel: String,
    currentXFunc: String?,
    currentYFunc: String?,
    onConfirm: (xFunc: String?, yFunc: String?) -> Unit,
    onDismiss: () -> Unit
) {
    var selectedX by remember { mutableStateOf(currentXFunc) }
    var selectedY by remember { mutableStateOf(currentYFunc) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stickLabel) },
        text = {
            Column(modifier = Modifier.heightIn(max = 450.dp)) {
                Text(
                    "X Axis (left/right)",
                    fontWeight = FontWeight.Bold,
                    fontSize = 14.sp,
                    modifier = Modifier.padding(bottom = 4.dp)
                )
                AxisFunctionRadioGroup(
                    selected = selectedX,
                    onSelect = { selectedX = it }
                )
                Spacer(Modifier.height(12.dp))
                Text(
                    "Y Axis (up/down)",
                    fontWeight = FontWeight.Bold,
                    fontSize = 14.sp,
                    modifier = Modifier.padding(bottom = 4.dp)
                )
                AxisFunctionRadioGroup(
                    selected = selectedY,
                    onSelect = { selectedY = it }
                )
            }
        },
        confirmButton = {
            TextButton(onClick = { onConfirm(selectedX, selectedY) }) { Text("OK") }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        }
    )
}

@Composable
private fun AxisFunctionRadioGroup(
    selected: String?,
    onSelect: (String?) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onSelect(null) }
            .padding(vertical = 3.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        RadioButton(selected = selected == null, onClick = { onSelect(null) })
        Spacer(Modifier.width(4.dp))
        Text("None", color = Color.Gray, fontSize = 13.sp)
    }
    for (func in AXIS_FUNCTIONS) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clickable { onSelect(func.label) }
                .padding(vertical = 3.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            RadioButton(
                selected = selected == func.label,
                onClick = { onSelect(func.label) }
            )
            Spacer(Modifier.width(4.dp))
            Text(func.label, fontSize = 13.sp)
        }
    }
}

// ── Canvas drawing helpers ──────────────────────────────────────────────────

private fun DrawScope.drawStick(
    cx: Float, cy: Float, radius: Float,
    deflectX: Float, deflectY: Float, label: String
) {
    drawCircle(color = cStickBg, radius = radius, center = Offset(cx, cy))
    drawCircle(color = cOutline, radius = radius, center = Offset(cx, cy), style = Stroke(1.5f))

    val dotR = radius * 0.4f
    val dotX = cx + deflectX * radius * 0.7f
    val dotY = cy + deflectY * radius * 0.7f
    val isDeflected = abs(deflectX) > 0.1f || abs(deflectY) > 0.1f
    drawCircle(
        color = if (isDeflected) cStickDotActive else cStickDot,
        radius = dotR, center = Offset(dotX, dotY)
    )
}

private fun DrawScope.drawDpad(
    cx: Float, cy: Float, armLen: Float,
    hatX: Float, hatY: Float
) {
    val armW = armLen * 0.35f
    val arms = listOf(
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
    cx: Float, cy: Float, radius: Float,
    label: String, pressed: Boolean
) {
    drawCircle(
        color = if (pressed) cActive else cInactive,
        radius = radius, center = Offset(cx, cy)
    )
    drawCircle(color = cOutline, radius = radius, center = Offset(cx, cy), style = Stroke(1f))
}

private fun DrawScope.drawTrigger(
    x: Float, y: Float, w: Float, h: Float, value: Float
) {
    drawRoundRect(
        color = cTriggerBg, topLeft = Offset(x, y),
        size = Size(w, h), cornerRadius = CornerRadius(h / 2f)
    )
    if (value > 0.02f) {
        val fillW = w * value.coerceIn(0f, 1f)
        drawRoundRect(
            color = cTriggerFill, topLeft = Offset(x, y),
            size = Size(fillW, h), cornerRadius = CornerRadius(h / 2f)
        )
    }
    drawRoundRect(
        color = cOutline, topLeft = Offset(x, y),
        size = Size(w, h), cornerRadius = CornerRadius(h / 2f),
        style = Stroke(1f)
    )
}

private fun DrawScope.drawLabel(
    textMeasurer: androidx.compose.ui.text.TextMeasurer,
    text: String, cx: Float, cy: Float, scale: Float
) {
    val style = TextStyle(
        color = cLabel,
        fontSize = (scale * 0.022f).sp,
        fontWeight = FontWeight.Medium
    )
    val result = textMeasurer.measure(text, style)
    drawText(
        textLayoutResult = result,
        topLeft = Offset(cx - result.size.width / 2f, cy - result.size.height / 2f)
    )
}

private fun DrawScope.drawFuncLabel(
    textMeasurer: androidx.compose.ui.text.TextMeasurer,
    text: String, cx: Float, cy: Float, scale: Float
) {
    val style = TextStyle(
        color = cAssignLabel,
        fontSize = (scale * 0.017f).sp,
        fontWeight = FontWeight.Normal
    )
    val result = textMeasurer.measure(text, style)
    drawText(
        textLayoutResult = result,
        topLeft = Offset(cx - result.size.width / 2f, cy - result.size.height / 2f)
    )
}
