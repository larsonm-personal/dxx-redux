package com.dxxredux.app

import android.app.Activity
import android.content.Context
import android.content.pm.ActivityInfo
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.launch
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.sqrt
import androidx.compose.ui.geometry.Size as ComposeSize

// ── Colors ──────────────────────────────────────────────────────────────────
private val cStickRing = Color(0xCC888888.toInt())
private val cStickThumb = Color(0xCCCCCCCC.toInt())
private val cButton = Color(0xCC666666.toInt())
private val cButtonLabel = Color(0xFFDDDDDD.toInt())
private val cRadialSeg = Color(0xAA555566.toInt())
private val cSelected = Color(0xFF2196F3)
private val cGrid = Color(0x22FFFFFF)
private val cBackground = Color(0xFF1A1A1A)
private val cCollisionWarn = Color(0xCCFF5722.toInt())

/**
 * Full-screen touch layout editor.
 * Displays controls on a canvas; tap to select, drag to move, bottom panel for properties.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun TouchEditorPage(onBack: () -> Unit) {
    val context = LocalContext.current
    var layout by remember { mutableStateOf(TouchLayoutRepository.load(context)) }
    var selectedType by remember { mutableStateOf<String?>(null) } // "stick", "button"
    var selectedIndex by remember { mutableIntStateOf(-1) }
    var dirty by remember { mutableStateOf(false) }
    var canvasWidth by remember { mutableFloatStateOf(1f) }
    var canvasHeight by remember { mutableFloatStateOf(1f) }

    // Lock orientation to match in-game orientation while editor is open
    val activity = context as? Activity
    DisposableEffect(Unit) {
        val prev = activity?.requestedOrientation
        val prefs = context.getSharedPreferences("dxx_prefs", Context.MODE_PRIVATE)
        val orient = prefs.getString("game_orientation", "landscape")
        activity?.requestedOrientation =
            if (orient == "portrait") {
                ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
            } else {
                ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            }
        onDispose {
            activity?.requestedOrientation = prev ?: ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
        }
    }

    // Dialogs
    var showPresetPicker by remember { mutableStateOf(false) }
    var showAddControl by remember { mutableStateOf(false) }
    var showGlobalSettings by remember { mutableStateOf(false) }
    var showGyroSettings by remember { mutableStateOf(false) }
    var longPressPos by remember { mutableStateOf(Offset.Zero) } // where to place new control

    // Save helper
    fun save() {
        TouchLayoutRepository.save(context, layout)
        dirty = false
    }

    val sheetState =
        rememberBottomSheetScaffoldState(
            bottomSheetState = rememberStandardBottomSheetState(initialValue = SheetValue.PartiallyExpanded),
        )
    val coroutineScope = rememberCoroutineScope()

    BottomSheetScaffold(
        scaffoldState = sheetState,
        sheetPeekHeight = 48.dp,
        sheetContainerColor = Color(0xFF262626),
        sheetContentColor = Color.White,
        sheetDragHandle = null,
        containerColor = cBackground,
        sheetContent = {
            // ── Toolbar row (always visible as peek content) ──
            Row(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 8.dp, vertical = 4.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                TextButton(onClick = {
                    if (dirty) save()
                    onBack()
                }) {
                    Text("Close Editor", fontSize = 12.sp, color = Color.White)
                }
                Spacer(Modifier.weight(1f))
                TextButton(onClick = { showPresetPicker = true }) {
                    Text("Presets", fontSize = 12.sp)
                }
                TextButton(onClick = { showGlobalSettings = true }) {
                    Text("Global", fontSize = 12.sp)
                }
                TextButton(onClick = { showGyroSettings = true }) {
                    Text("Gyro", fontSize = 12.sp)
                }
                IconButton(onClick = { showAddControl = true }, modifier = Modifier.size(36.dp)) {
                    Icon(Icons.Default.Add, "Add control", tint = Color.White)
                }
                TextButton(onClick = { save() }, enabled = dirty) {
                    Text(
                        "Save",
                        fontSize = 12.sp,
                        color =
                            if (dirty) {
                                MaterialTheme.colorScheme.primary
                            } else {
                                MaterialTheme.colorScheme.onSurface.copy(alpha = 0.4f)
                            },
                    )
                }
            }
            HorizontalDivider(color = Color(0xFF444444))

            // ── Properties panel (visible when sheet is expanded) ──
            if (selectedType != null && selectedIndex >= 0) {
                val panelScrollState = rememberScrollState()
                Box(modifier = Modifier.heightIn(max = 300.dp)) {
                    Column(
                        modifier =
                            Modifier
                                .verticalScroll(panelScrollState)
                                .padding(horizontal = 12.dp, vertical = 8.dp),
                    ) {
                        when (selectedType) {
                            "stick" ->
                                StickPropertiesPanel(
                                    stick = layout.sticks[selectedIndex],
                                    onUpdate = { updated ->
                                        layout =
                                            layout.copy(
                                                sticks =
                                                    layout.sticks.toMutableList().also {
                                                        it[selectedIndex] = updated
                                                    },
                                            )
                                        dirty = true
                                    },
                                    onDelete = {
                                        layout =
                                            layout.copy(
                                                sticks =
                                                    layout.sticks.toMutableList().also {
                                                        it.removeAt(selectedIndex)
                                                    },
                                            )
                                        selectedType = null
                                        selectedIndex = -1
                                        dirty = true
                                    },
                                )
                            "button" ->
                                ButtonPropertiesPanel(
                                    button = layout.buttons[selectedIndex],
                                    onUpdate = { updated ->
                                        layout =
                                            layout.copy(
                                                buttons =
                                                    layout.buttons.toMutableList().also {
                                                        it[selectedIndex] = updated
                                                    },
                                            )
                                        dirty = true
                                    },
                                    onDelete = {
                                        layout =
                                            layout.copy(
                                                buttons =
                                                    layout.buttons.toMutableList().also {
                                                        it.removeAt(selectedIndex)
                                                    },
                                            )
                                        selectedType = null
                                        selectedIndex = -1
                                        dirty = true
                                    },
                                )
                            "radial" ->
                                RadialPropertiesPanel(
                                    radial = layout.radialMenus[selectedIndex],
                                    onUpdate = { updated ->
                                        layout =
                                            layout.copy(
                                                radialMenus =
                                                    layout.radialMenus.toMutableList().also {
                                                        it[selectedIndex] = updated
                                                    },
                                            )
                                        dirty = true
                                    },
                                    onDelete = {
                                        layout =
                                            layout.copy(
                                                radialMenus =
                                                    layout.radialMenus.toMutableList().also {
                                                        it.removeAt(selectedIndex)
                                                    },
                                            )
                                        selectedType = null
                                        selectedIndex = -1
                                        dirty = true
                                    },
                                )
                            "slider" ->
                                SliderPropertiesPanel(
                                    slider = layout.sliders[selectedIndex],
                                    onUpdate = { updated ->
                                        layout =
                                            layout.copy(
                                                sliders =
                                                    layout.sliders.toMutableList().also {
                                                        it[selectedIndex] = updated
                                                    },
                                            )
                                        dirty = true
                                    },
                                    onDelete = {
                                        layout =
                                            layout.copy(
                                                sliders =
                                                    layout.sliders.toMutableList().also {
                                                        it.removeAt(selectedIndex)
                                                    },
                                            )
                                        selectedType = null
                                        selectedIndex = -1
                                        dirty = true
                                    },
                                )
                        }
                    }
                    ScrollArrows(panelScrollState)
                }
            } else {
                Box(
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .padding(16.dp),
                    contentAlignment = Alignment.Center,
                ) {
                    Text(
                        "Tap a control to select it, drag to move",
                        color = Color.Gray,
                        fontSize = 13.sp,
                    )
                }
            }
        },
    ) { innerPadding ->
        // ── Full-screen canvas ──
        val textMeasurer = rememberTextMeasurer()

        // Snapshot refs so gesture blocks don't restart when layout changes
        val layoutRef = rememberUpdatedState(layout)
        val selTypeRef = rememberUpdatedState(selectedType)
        val selIdxRef = rememberUpdatedState(selectedIndex)

        Canvas(
            modifier =
                Modifier
                    .fillMaxSize()
                    .padding(innerPadding)
                    .pointerInput(Unit) {
                        detectTapGestures(
                            onTap = { offset ->
                                val hit = hitTest(layoutRef.value, offset, canvasWidth, canvasHeight)
                                if (hit != null) {
                                    selectedType = hit.first
                                    selectedIndex = hit.second
                                } else {
                                    selectedType = null
                                    selectedIndex = -1
                                }
                            },
                            onLongPress = { offset ->
                                val hit = hitTest(layoutRef.value, offset, canvasWidth, canvasHeight)
                                if (hit != null) {
                                    // Long-press on control → select & expand bottom sheet
                                    selectedType = hit.first
                                    selectedIndex = hit.second
                                    coroutineScope.launch { sheetState.bottomSheetState.expand() }
                                } else {
                                    // Long-press on empty space → add control at that position
                                    longPressPos = offset
                                    showAddControl = true
                                }
                            },
                        )
                    }.pointerInput(Unit) {
                        detectDragGestures { change, dragAmount ->
                            change.consume()
                            val st = selTypeRef.value
                            val si = selIdxRef.value
                            if (st != null && si >= 0) {
                                val lay = layoutRef.value
                                val cr = controlRadius(lay, st, si, canvasWidth, canvasHeight)
                                val cc = controlCenter(lay, st, si, canvasWidth, canvasHeight)
                                val maxDist = cr + canvasWidth * 0.10f
                                val fingerDist =
                                    sqrt(
                                        (change.position.x - cc.x) * (change.position.x - cc.x) +
                                            (change.position.y - cc.y) * (change.position.y - cc.y),
                                    )
                                if (fingerDist <= maxDist) {
                                    val dxPct = (dragAmount.x / canvasWidth) * 100f
                                    val dyPct = (dragAmount.y / canvasHeight) * 100f
                                    layout = moveControl(lay, st, si, dxPct, dyPct)
                                    dirty = true
                                }
                            }
                        }
                    },
        ) {
            canvasWidth = size.width
            canvasHeight = size.height
            drawGrid(this)
            drawAllControls(this, layout, selectedType, selectedIndex, textMeasurer)
        }
    }

    // ── Dialogs ──────────────────────────────────────────────────────────────
    if (showPresetPicker) {
        PresetPickerDialog(
            onDismiss = { showPresetPicker = false },
            onSelect = { preset ->
                layout = preset
                selectedType = null
                selectedIndex = -1
                dirty = true
                showPresetPicker = false
            },
        )
    }
    if (showAddControl) {
        // If placed via long-press, use that position; otherwise center
        val addX = if (longPressPos != Offset.Zero) (longPressPos.x / canvasWidth * 100f).coerceIn(5f, 95f) else 50f
        val addY = if (longPressPos != Offset.Zero) (longPressPos.y / canvasHeight * 100f).coerceIn(5f, 95f) else 50f
        AddControlDialog(
            onDismiss = {
                showAddControl = false
                longPressPos = Offset.Zero
            },
            onAddStick = {
                layout =
                    layout.copy(
                        sticks =
                            layout.sticks +
                                AnalogStickControl(
                                    id = "stick_${layout.sticks.size}",
                                    xPct = addX,
                                    yPct = addY,
                                    axisX = TouchBindings.AXIS_RIGHT_X,
                                    axisY = TouchBindings.AXIS_RIGHT_Y,
                                ),
                    )
                selectedType = "stick"
                selectedIndex = layout.sticks.lastIndex
                dirty = true
                showAddControl = false
                longPressPos = Offset.Zero
            },
            onAddButton = {
                layout =
                    layout.copy(
                        buttons =
                            layout.buttons +
                                ButtonControl(
                                    id = "btn_${layout.buttons.size}",
                                    xPct = addX,
                                    yPct = addY,
                                    label = "BTN",
                                    binding = TouchBindings.BTN_FIRE_PRIMARY,
                                ),
                    )
                selectedType = "button"
                selectedIndex = layout.buttons.lastIndex
                dirty = true
                showAddControl = false
                longPressPos = Offset.Zero
            },
            onAddRadial = {
                layout =
                    layout.copy(
                        radialMenus =
                            layout.radialMenus +
                                RadialMenuControl(
                                    id = "menu_${layout.radialMenus.size}",
                                    xPct = addX,
                                    yPct = addY,
                                    segments =
                                        listOf(
                                            RadialSegment("Seg 1", android.view.KeyEvent.KEYCODE_1),
                                            RadialSegment("Seg 2", android.view.KeyEvent.KEYCODE_2),
                                            RadialSegment("Seg 3", android.view.KeyEvent.KEYCODE_3),
                                        ),
                                ),
                    )
                selectedType = "radial"
                selectedIndex = layout.radialMenus.lastIndex
                dirty = true
                showAddControl = false
                longPressPos = Offset.Zero
            },
            onAddSlider = {
                layout =
                    layout.copy(
                        sliders =
                            layout.sliders +
                                SliderControl(
                                    id = "slider_${layout.sliders.size}",
                                    xPct = addX,
                                    yPct = addY,
                                    axis = TouchBindings.AXIS_LTRIGGER,
                                ),
                    )
                selectedType = "slider"
                selectedIndex = layout.sliders.lastIndex
                dirty = true
                showAddControl = false
                longPressPos = Offset.Zero
            },
        )
    }
    if (showGlobalSettings) {
        GlobalSettingsDialog(
            layout = layout,
            onDismiss = { showGlobalSettings = false },
            onUpdate = { updated ->
                layout = updated
                dirty = true
            },
        )
    }
    if (showGyroSettings) {
        GyroSettingsDialog(
            gyro = layout.gyro,
            onDismiss = { showGyroSettings = false },
            onUpdate = { updated ->
                layout = layout.copy(gyro = updated)
                dirty = true
            },
        )
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Canvas drawing
// ═════════════════════════════════════════════════════════════════════════════

private fun drawGrid(scope: DrawScope) {
    val w = scope.size.width
    val h = scope.size.height
    // 10% grid lines
    for (i in 1..9) {
        val x = w * i / 10f
        val y = h * i / 10f
        scope.drawLine(cGrid, Offset(x, 0f), Offset(x, h))
        scope.drawLine(cGrid, Offset(0f, y), Offset(w, y))
    }
    // Center crosshair
    scope.drawLine(Color(0x44FFFFFF), Offset(w / 2, 0f), Offset(w / 2, h), strokeWidth = 1.5f)
    scope.drawLine(Color(0x44FFFFFF), Offset(0f, h / 2), Offset(w, h / 2), strokeWidth = 1.5f)
}

private fun drawAllControls(
    scope: DrawScope,
    layout: TouchLayout,
    selType: String?,
    selIdx: Int,
    textMeasurer: androidx.compose.ui.text.TextMeasurer,
) {
    val w = scope.size.width
    val h = scope.size.height
    // Use geometric mean so controls stay the same apparent size in portrait & landscape
    val baseScale = sqrt(w * h)

    // Draw sticks
    layout.sticks.forEachIndexed { i, stick ->
        val cx = w * stick.xPct / 100f
        val cy = h * stick.yPct / 100f
        val r = baseScale * 0.12f * stick.sizeMult
        val selected = selType == "stick" && selIdx == i
        val alpha = layout.globalOpacity * stick.opacity

        // Floating zone indicator
        if (stick.floating) {
            val fz = stick.floatingZone
            scope.drawRect(
                color = Color(0x1488CCFF),
                topLeft = Offset(w * fz.leftPct / 100f, h * fz.topPct / 100f),
                size =
                    androidx.compose.ui.geometry.Size(
                        w * (fz.rightPct - fz.leftPct) / 100f,
                        h * (fz.bottomPct - fz.topPct) / 100f,
                    ),
            )
        }

        // Ring
        scope.drawCircle(
            color = cStickRing.copy(alpha = alpha),
            radius = r,
            center = Offset(cx, cy),
            style = Stroke(width = 3f),
        )
        // Thumb (at center in editor)
        scope.drawCircle(
            color = cStickThumb.copy(alpha = alpha * 0.6f),
            radius = r * 0.35f,
            center = Offset(cx, cy),
        )
        // Selection highlight
        if (selected) {
            scope.drawCircle(
                color = cSelected,
                radius = r + 4f,
                center = Offset(cx, cy),
                style = Stroke(width = 3f),
            )
        }
        // Label
        val label = TouchBindings.AXIS_LABELS[stick.axisX]?.take(3) ?: "?"
        val textResult =
            textMeasurer.measure(
                label,
                style = TextStyle(fontSize = 10.sp, color = cButtonLabel.copy(alpha = alpha)),
            )
        scope.drawText(
            textResult,
            topLeft =
                Offset(
                    cx - textResult.size.width / 2f,
                    cy - textResult.size.height / 2f,
                ),
        )
    }

    // Draw buttons
    layout.buttons.forEachIndexed { i, btn ->
        val cx = w * btn.xPct / 100f
        val cy = h * btn.yPct / 100f
        val r = baseScale * 0.04f * btn.sizeMult
        val selected = selType == "button" && selIdx == i
        val alpha = layout.globalOpacity * btn.opacity

        if (btn.shape == ButtonShape.ROUNDED_RECT) {
            val side = r * 1.6f
            scope.drawRoundRect(
                color = cButton.copy(alpha = alpha),
                topLeft = Offset(cx - side / 2, cy - side / 2),
                size = ComposeSize(side, side),
                cornerRadius = CornerRadius(side * 0.25f),
            )
            if (selected) {
                scope.drawRoundRect(
                    color = cSelected,
                    topLeft = Offset(cx - side / 2 - 3f, cy - side / 2 - 3f),
                    size = ComposeSize(side + 6f, side + 6f),
                    cornerRadius = CornerRadius(side * 0.25f + 3f),
                    style = Stroke(width = 3f),
                )
            }
        } else {
            scope.drawCircle(
                color = cButton.copy(alpha = alpha),
                radius = r,
                center = Offset(cx, cy),
            )
            if (selected) {
                scope.drawCircle(
                    color = cSelected,
                    radius = r + 3f,
                    center = Offset(cx, cy),
                    style = Stroke(width = 3f),
                )
            }
        }
        val textResult =
            textMeasurer.measure(
                btn.label.take(4),
                style = TextStyle(fontSize = 9.sp, color = cButtonLabel.copy(alpha = alpha)),
            )
        scope.drawText(
            textResult,
            topLeft =
                Offset(
                    cx - textResult.size.width / 2f,
                    cy - textResult.size.height / 2f,
                ),
        )
    }

    // Draw radial menus (as trigger circles with segment count label)
    layout.radialMenus.forEachIndexed { i, rm ->
        val cx = w * rm.xPct / 100f
        val cy = h * rm.yPct / 100f
        val trigR = baseScale * 0.04f * rm.sizeMult
        val wheelR = baseScale * 0.14f * rm.sizeMult
        val selected = selType == "radial" && selIdx == i
        val alpha = layout.globalOpacity * rm.opacity

        // Ghost wheel extent
        val n = rm.segments.size
        if (n > 0) {
            val segAngle = 360f / n
            for (seg in 0 until n) {
                val startDeg = -90f + seg * segAngle
                scope.drawArc(
                    color = cRadialSeg.copy(alpha = alpha * 0.3f),
                    startAngle = startDeg,
                    sweepAngle = segAngle,
                    useCenter = true,
                    topLeft = Offset(cx - wheelR, cy - wheelR),
                    size =
                        androidx.compose.ui.geometry
                            .Size(wheelR * 2, wheelR * 2),
                )
                scope.drawArc(
                    color = cStickRing.copy(alpha = alpha * 0.4f),
                    startAngle = startDeg,
                    sweepAngle = segAngle,
                    useCenter = true,
                    topLeft = Offset(cx - wheelR, cy - wheelR),
                    size =
                        androidx.compose.ui.geometry
                            .Size(wheelR * 2, wheelR * 2),
                    style = Stroke(width = 1f),
                )
                // Segment label
                val midRad = Math.toRadians((startDeg + segAngle / 2).toDouble())
                val lx = cx + cos(midRad).toFloat() * wheelR * 0.65f
                val ly = cy + sin(midRad).toFloat() * wheelR * 0.65f
                val segLabel =
                    textMeasurer.measure(
                        rm.segments[seg].label.take(5),
                        style = TextStyle(fontSize = 7.sp, color = cButtonLabel.copy(alpha = alpha * 0.5f)),
                    )
                scope.drawText(
                    segLabel,
                    topLeft =
                        Offset(
                            lx - segLabel.size.width / 2f,
                            ly - segLabel.size.height / 2f,
                        ),
                )
            }
        }

        // Trigger circle
        scope.drawCircle(color = cButton.copy(alpha = alpha), radius = trigR, center = Offset(cx, cy))
        if (selected) {
            scope.drawCircle(
                color = cSelected,
                radius = wheelR + 4f,
                center = Offset(cx, cy),
                style = Stroke(width = 3f),
            )
        }
        val label =
            textMeasurer.measure(
                rm.id.take(4),
                style = TextStyle(fontSize = 9.sp, color = cButtonLabel.copy(alpha = alpha)),
            )
        scope.drawText(
            label,
            topLeft =
                Offset(
                    cx - label.size.width / 2f,
                    cy - label.size.height / 2f,
                ),
        )
    }

    // Draw sliders
    layout.sliders.forEachIndexed { i, sl ->
        val cx = w * sl.xPct / 100f
        val cy = h * sl.yPct / 100f
        val trackLen = baseScale * 0.10f * sl.sizeMult
        val thumbR = baseScale * 0.015f * sl.sizeMult
        val selected = selType == "slider" && selIdx == i
        val alpha = layout.globalOpacity * sl.opacity
        val vertical = sl.orientation == SliderOrientation.VERTICAL

        // Track line
        val x0: Float
        val y0: Float
        val x1: Float
        val y1: Float
        if (vertical) {
            x0 = cx
            y0 = cy - trackLen
            x1 = cx
            y1 = cy + trackLen
        } else {
            x0 = cx - trackLen
            y0 = cy
            x1 = cx + trackLen
            y1 = cy
        }
        scope.drawLine(
            color = cStickRing.copy(alpha = alpha * 0.6f),
            start = Offset(x0, y0),
            end = Offset(x1, y1),
            strokeWidth = thumbR * 0.6f,
        )
        // Thumb at center (editor preview)
        scope.drawCircle(
            color = cStickThumb.copy(alpha = alpha * 0.8f),
            radius = thumbR,
            center = Offset(cx, cy),
        )
        if (selected) {
            scope.drawLine(
                color = cSelected,
                start = Offset(x0, y0),
                end = Offset(x1, y1),
                strokeWidth = 3f,
            )
            scope.drawCircle(
                color = cSelected,
                radius = thumbR + 3f,
                center = Offset(cx, cy),
                style = Stroke(width = 3f),
            )
        }
        val slLabel =
            textMeasurer.measure(
                sl.id.take(5),
                style = TextStyle(fontSize = 8.sp, color = cButtonLabel.copy(alpha = alpha)),
            )
        val labelOff =
            if (vertical) {
                Offset(cx - slLabel.size.width / 2f, y1 + thumbR + 2f)
            } else {
                Offset(cx - slLabel.size.width / 2f, cy + thumbR + 2f)
            }
        scope.drawText(slLabel, topLeft = labelOff)
    }

    // ── Collision detection: warn when controls overlap >30% ──
    data class ControlBounds(
        val cx: Float,
        val cy: Float,
        val r: Float,
    )
    val allBounds = mutableListOf<ControlBounds>()
    layout.sticks.forEach { s ->
        allBounds.add(
            ControlBounds(
                w * s.xPct / 100f,
                h * s.yPct / 100f,
                baseScale * 0.12f * s.sizeMult,
            ),
        )
    }
    layout.buttons.forEach { b ->
        allBounds.add(
            ControlBounds(
                w * b.xPct / 100f,
                h * b.yPct / 100f,
                baseScale * 0.04f * b.sizeMult,
            ),
        )
    }
    layout.radialMenus.forEach { rm ->
        allBounds.add(
            ControlBounds(
                w * rm.xPct / 100f,
                h * rm.yPct / 100f,
                baseScale * 0.14f * rm.sizeMult,
            ),
        )
    }
    layout.sliders.forEach { sl ->
        allBounds.add(
            ControlBounds(
                w * sl.xPct / 100f,
                h * sl.yPct / 100f,
                baseScale * 0.10f * sl.sizeMult,
            ),
        )
    }

    for (i in allBounds.indices) {
        for (j in i + 1 until allBounds.size) {
            val a = allBounds[i]
            val b = allBounds[j]
            val dist = sqrt((a.cx - b.cx) * (a.cx - b.cx) + (a.cy - b.cy) * (a.cy - b.cy))
            val smaller = min(a.r, b.r)
            val overlap = (a.r + b.r - dist) / (smaller * 2)
            if (overlap > 0.3f) {
                // Draw warning rings on both
                scope.drawCircle(
                    color = cCollisionWarn,
                    radius = a.r + 2f,
                    center = Offset(a.cx, a.cy),
                    style = Stroke(width = 2.5f),
                )
                scope.drawCircle(
                    color = cCollisionWarn,
                    radius = b.r + 2f,
                    center = Offset(b.cx, b.cy),
                    style = Stroke(width = 2.5f),
                )
            }
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Hit testing & movement
// ═════════════════════════════════════════════════════════════════════════════

/** Returns the center position (in canvas pixels) of the given control. */
private fun controlCenter(
    layout: TouchLayout,
    type: String,
    index: Int,
    canvasWidth: Float,
    canvasHeight: Float,
): Offset =
    when (type) {
        "stick" -> {
            val s = layout.sticks[index]
            Offset(canvasWidth * s.xPct / 100f, canvasHeight * s.yPct / 100f)
        }
        "button" -> {
            val b = layout.buttons[index]
            Offset(canvasWidth * b.xPct / 100f, canvasHeight * b.yPct / 100f)
        }
        "radial" -> {
            val r = layout.radialMenus[index]
            Offset(
                canvasWidth * r.xPct / 100f,
                canvasHeight * r.yPct / 100f,
            )
        }
        "slider" -> {
            val s = layout.sliders[index]
            Offset(canvasWidth * s.xPct / 100f, canvasHeight * s.yPct / 100f)
        }
        else -> Offset.Zero
    }

/** Returns the visual radius (in canvas pixels) of the given control. */
private fun controlRadius(
    layout: TouchLayout,
    type: String,
    index: Int,
    canvasWidth: Float,
    canvasHeight: Float,
): Float {
    val baseScale = sqrt(canvasWidth * canvasHeight)
    return when (type) {
        "stick" -> baseScale * 0.12f * layout.sticks[index].sizeMult
        "button" -> baseScale * 0.04f * layout.buttons[index].sizeMult
        "radial" -> baseScale * 0.14f * layout.radialMenus[index].sizeMult
        "slider" -> baseScale * 0.10f * layout.sliders[index].sizeMult
        else -> 0f
    }
}

/** Returns (type, index) of the control at the given canvas offset, or null. */
private fun hitTest(
    layout: TouchLayout,
    offset: Offset,
    canvasWidth: Float,
    canvasHeight: Float,
): Pair<String, Int>? {
    val baseScale = sqrt(canvasWidth * canvasHeight)

    // Check buttons first (smaller targets, should take priority)
    layout.buttons.forEachIndexed { i, btn ->
        val cx = canvasWidth * btn.xPct / 100f
        val cy = canvasHeight * btn.yPct / 100f
        val r = baseScale * 0.04f * btn.sizeMult
        val dist = sqrt((offset.x - cx) * (offset.x - cx) + (offset.y - cy) * (offset.y - cy))
        if (dist <= r * 1.3f) return Pair("button", i)
    }

    // Check radial menus (before sticks, they have smaller triggers)
    layout.radialMenus.forEachIndexed { i, rm ->
        val cx = canvasWidth * rm.xPct / 100f
        val cy = canvasHeight * rm.yPct / 100f
        val r = baseScale * 0.14f * rm.sizeMult
        val dist = sqrt((offset.x - cx) * (offset.x - cx) + (offset.y - cy) * (offset.y - cy))
        if (dist <= r) return Pair("radial", i)
    }

    // Check sliders
    layout.sliders.forEachIndexed { i, sl ->
        val cx = canvasWidth * sl.xPct / 100f
        val cy = canvasHeight * sl.yPct / 100f
        val trackLen = baseScale * 0.10f * sl.sizeMult
        val thumbR = baseScale * 0.015f * sl.sizeMult
        val vertical = sl.orientation == SliderOrientation.VERTICAL
        val along = if (vertical) kotlin.math.abs(offset.y - cy) else kotlin.math.abs(offset.x - cx)
        val across = if (vertical) kotlin.math.abs(offset.x - cx) else kotlin.math.abs(offset.y - cy)
        if (along <= trackLen + thumbR && across <= thumbR * 3) return Pair("slider", i)
    }

    // Check sticks
    layout.sticks.forEachIndexed { i, stick ->
        val cx = canvasWidth * stick.xPct / 100f
        val cy = canvasHeight * stick.yPct / 100f
        val r = baseScale * 0.12f * stick.sizeMult
        val dist = sqrt((offset.x - cx) * (offset.x - cx) + (offset.y - cy) * (offset.y - cy))
        if (dist <= r) return Pair("stick", i)
    }

    return null
}

/** Returns a new layout with the specified control moved by (dxPct, dyPct). */
private fun moveControl(
    layout: TouchLayout,
    type: String,
    index: Int,
    dxPct: Float,
    dyPct: Float,
): TouchLayout =
    when (type) {
        "stick" -> {
            val s = layout.sticks[index]
            val newX = (s.xPct + dxPct).coerceIn(5f, 95f)
            val newY = (s.yPct + dyPct).coerceIn(5f, 95f)
            layout.copy(
                sticks =
                    layout.sticks.toMutableList().also {
                        it[index] = s.copy(xPct = newX, yPct = newY)
                    },
            )
        }
        "button" -> {
            val b = layout.buttons[index]
            val newX = (b.xPct + dxPct).coerceIn(2f, 98f)
            val newY = (b.yPct + dyPct).coerceIn(2f, 98f)
            layout.copy(
                buttons =
                    layout.buttons.toMutableList().also {
                        it[index] = b.copy(xPct = newX, yPct = newY)
                    },
            )
        }
        "radial" -> {
            val rm = layout.radialMenus[index]
            val newX = (rm.xPct + dxPct).coerceIn(5f, 95f)
            val newY = (rm.yPct + dyPct).coerceIn(5f, 95f)
            layout.copy(
                radialMenus =
                    layout.radialMenus.toMutableList().also {
                        it[index] = rm.copy(xPct = newX, yPct = newY)
                    },
            )
        }
        "slider" -> {
            val sl = layout.sliders[index]
            val newX = (sl.xPct + dxPct).coerceIn(5f, 95f)
            val newY = (sl.yPct + dyPct).coerceIn(5f, 95f)
            layout.copy(
                sliders =
                    layout.sliders.toMutableList().also {
                        it[index] = sl.copy(xPct = newX, yPct = newY)
                    },
            )
        }
        else -> layout
    }

// ═════════════════════════════════════════════════════════════════════════════
// Properties panels
// ═════════════════════════════════════════════════════════════════════════════

@Composable
private fun StickPropertiesPanel(
    stick: AnalogStickControl,
    onUpdate: (AnalogStickControl) -> Unit,
    onDelete: () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text("Stick: ${stick.id}", color = Color.White, fontSize = 14.sp)
        IconButton(onClick = onDelete) {
            Icon(Icons.Default.Delete, "Delete", tint = Color(0xFFEF5350))
        }
    }

    // Axis bindings
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        AxisPicker("X Axis", stick.axisX, Modifier.weight(1f)) {
            onUpdate(stick.copy(axisX = it))
        }
        AxisPicker("Y Axis", stick.axisY, Modifier.weight(1f)) {
            onUpdate(stick.copy(axisY = it))
        }
    }

    // Size & opacity
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        LabeledSlider(
            "Size",
            stick.sizeMult,
            TouchBindings.MIN_SIZE,
            TouchBindings.MAX_SIZE,
            Modifier.weight(1f),
        ) {
            onUpdate(stick.copy(sizeMult = it))
        }
        LabeledSlider(
            "Opacity",
            stick.opacity,
            TouchBindings.MIN_OPACITY,
            TouchBindings.MAX_OPACITY,
            Modifier.weight(1f),
        ) {
            onUpdate(stick.copy(opacity = it))
        }
    }

    // Deadzone & sensitivity
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        LabeledSlider("Deadzone", stick.deadzone.toFloat(), 0f, 50f, Modifier.weight(1f)) {
            onUpdate(stick.copy(deadzone = it.toInt()))
        }
        LabeledSlider(
            "Sensitivity",
            stick.sensitivity,
            TouchBindings.MIN_SENSITIVITY,
            TouchBindings.MAX_SENSITIVITY,
            Modifier.weight(1f),
        ) {
            onUpdate(stick.copy(sensitivity = it))
        }
    }

    // Response curve
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        CurvePicker("Curve", stick.responseCurve, Modifier.weight(1f)) {
            onUpdate(stick.copy(responseCurve = it))
        }
        if (stick.responseCurve == ResponseCurve.EXPONENTIAL) {
            LabeledSlider(
                "Exponent",
                stick.exponent,
                TouchBindings.MIN_EXPONENT,
                TouchBindings.MAX_EXPONENT,
                Modifier.weight(1f),
            ) {
                onUpdate(stick.copy(exponent = it))
            }
        }
    }

    // Toggles
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
        LabeledToggle("Floating", stick.floating) { onUpdate(stick.copy(floating = it)) }
        LabeledToggle("Invert X", stick.invertX) { onUpdate(stick.copy(invertX = it)) }
        LabeledToggle("Invert Y", stick.invertY) { onUpdate(stick.copy(invertY = it)) }
        LabeledToggle("Haptic", stick.hapticFeedback) { onUpdate(stick.copy(hapticFeedback = it)) }
    }
}

@Composable
private fun ButtonPropertiesPanel(
    button: ButtonControl,
    onUpdate: (ButtonControl) -> Unit,
    onDelete: () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text("Button: ${button.id}", color = Color.White, fontSize = 14.sp)
        IconButton(onClick = onDelete) {
            Icon(Icons.Default.Delete, "Delete", tint = Color(0xFFEF5350))
        }
    }

    // Binding & label
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        ButtonBindingPicker("Binding", button.binding, Modifier.weight(1f)) {
            onUpdate(button.copy(binding = it))
        }
        LabelEditor("Label", button.label, Modifier.weight(1f)) {
            onUpdate(button.copy(label = it))
        }
    }

    // Size & opacity
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        LabeledSlider(
            "Size",
            button.sizeMult,
            TouchBindings.MIN_SIZE,
            TouchBindings.MAX_SIZE,
            Modifier.weight(1f),
        ) {
            onUpdate(button.copy(sizeMult = it))
        }
        LabeledSlider(
            "Opacity",
            button.opacity,
            TouchBindings.MIN_OPACITY,
            TouchBindings.MAX_OPACITY,
            Modifier.weight(1f),
        ) {
            onUpdate(button.copy(opacity = it))
        }
    }

    // Toggles
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
        LabeledToggle("Toggle", button.toggle) { onUpdate(button.copy(toggle = it)) }
        LabeledToggle("Haptic", button.hapticFeedback) { onUpdate(button.copy(hapticFeedback = it)) }
    }

    // Shape
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text("Shape: ", color = Color.Gray, fontSize = 12.sp)
        ButtonShape.entries.forEach { shape ->
            val selected = button.shape == shape
            TextButton(onClick = { onUpdate(button.copy(shape = shape)) }) {
                Text(
                    shape.name.lowercase().replace('_', ' '),
                    fontSize = 11.sp,
                    color = if (selected) cSelected else Color.Gray,
                )
            }
        }
    }
}

@Composable
private fun RadialPropertiesPanel(
    radial: RadialMenuControl,
    onUpdate: (RadialMenuControl) -> Unit,
    onDelete: () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text("Radial: ${radial.id}", color = Color.White, fontSize = 14.sp)
        IconButton(onClick = onDelete) {
            Icon(Icons.Default.Delete, "Delete", tint = Color(0xFFEF5350))
        }
    }

    // ID & center label
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        LabelEditor("ID", radial.id, Modifier.weight(1f)) {
            onUpdate(radial.copy(id = it))
        }
        LabelEditor("Center", radial.centerLabel, Modifier.weight(1f)) {
            onUpdate(radial.copy(centerLabel = it))
        }
    }

    // Size & opacity
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        LabeledSlider(
            "Size",
            radial.sizeMult,
            TouchBindings.MIN_SIZE,
            TouchBindings.MAX_SIZE,
            Modifier.weight(1f),
        ) {
            onUpdate(radial.copy(sizeMult = it))
        }
        LabeledSlider(
            "Opacity",
            radial.opacity,
            TouchBindings.MIN_OPACITY,
            TouchBindings.MAX_OPACITY,
            Modifier.weight(1f),
        ) {
            onUpdate(radial.copy(opacity = it))
        }
    }

    // Segments info
    Text(
        "Segments: ${radial.segments.size}  |  " +
            radial.segments.joinToString(", ") { it.label },
        color = Color.Gray,
        fontSize = 11.sp,
    )

    // Haptic toggle
    LabeledToggle("Haptic", radial.hapticFeedback) {
        onUpdate(radial.copy(hapticFeedback = it))
    }
}

@Composable
private fun SliderPropertiesPanel(
    slider: SliderControl,
    onUpdate: (SliderControl) -> Unit,
    onDelete: () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text("Slider: ${slider.id}", color = Color.White, fontSize = 14.sp)
        IconButton(onClick = onDelete) {
            Icon(Icons.Default.Delete, "Delete", tint = Color(0xFFEF5350))
        }
    }

    // Axis & orientation
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        AxisPicker("Axis", slider.axis, Modifier.weight(1f)) {
            onUpdate(slider.copy(axis = it))
        }
        Column(Modifier.weight(1f)) {
            Text("Orientation", color = Color.Gray, fontSize = 11.sp)
            Row {
                SliderOrientation.entries.forEach { ori ->
                    TextButton(onClick = { onUpdate(slider.copy(orientation = ori)) }) {
                        Text(
                            ori.name.lowercase(),
                            fontSize = 11.sp,
                            color = if (slider.orientation == ori) cSelected else Color.Gray,
                        )
                    }
                }
            }
        }
    }

    // Size & opacity
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        LabeledSlider(
            "Size",
            slider.sizeMult,
            TouchBindings.MIN_SIZE,
            TouchBindings.MAX_SIZE,
            Modifier.weight(1f),
        ) {
            onUpdate(slider.copy(sizeMult = it))
        }
        LabeledSlider(
            "Opacity",
            slider.opacity,
            TouchBindings.MIN_OPACITY,
            TouchBindings.MAX_OPACITY,
            Modifier.weight(1f),
        ) {
            onUpdate(slider.copy(opacity = it))
        }
    }

    // Sensitivity & curve
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        LabeledSlider(
            "Sensitivity",
            slider.sensitivity,
            TouchBindings.MIN_SENSITIVITY,
            TouchBindings.MAX_SENSITIVITY,
            Modifier.weight(1f),
        ) {
            onUpdate(slider.copy(sensitivity = it))
        }
        CurvePicker("Curve", slider.responseCurve, Modifier.weight(1f)) {
            onUpdate(slider.copy(responseCurve = it))
        }
    }

    // Spring back toggle
    LabeledToggle("Spring Back", slider.springBack) {
        onUpdate(slider.copy(springBack = it))
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Reusable editor widgets
// ═════════════════════════════════════════════════════════════════════════════

@Composable
private fun LabeledSlider(
    label: String,
    value: Float,
    min: Float,
    max: Float,
    modifier: Modifier = Modifier,
    onChange: (Float) -> Unit,
) {
    Column(modifier = modifier) {
        Text("$label: ${"%.2f".format(value)}", color = Color.Gray, fontSize = 11.sp)
        Slider(
            value = value,
            onValueChange = onChange,
            valueRange = min..max,
            modifier = Modifier.height(28.dp),
        )
    }
}

@Composable
private fun LabeledToggle(
    label: String,
    checked: Boolean,
    onChange: (Boolean) -> Unit,
) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Checkbox(
            checked = checked,
            onCheckedChange = onChange,
            modifier = Modifier.size(20.dp),
        )
        Spacer(Modifier.width(2.dp))
        Text(label, color = Color.Gray, fontSize = 11.sp)
    }
}

@Composable
private fun AxisPicker(
    label: String,
    current: Int,
    modifier: Modifier = Modifier,
    onChange: (Int) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    val currentLabel = TouchBindings.AXIS_LABELS[current] ?: "Axis $current"

    Column(modifier = modifier) {
        Text(label, color = Color.Gray, fontSize = 11.sp)
        Box {
            TextButton(onClick = { expanded = true }) {
                Text(currentLabel, fontSize = 11.sp)
            }
            DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                TouchBindings.AXIS_LABELS.forEach { (idx, name) ->
                    DropdownMenuItem(
                        text = { Text(name, fontSize = 12.sp) },
                        onClick = {
                            onChange(idx)
                            expanded = false
                        },
                    )
                }
            }
        }
    }
}

@Composable
private fun ButtonBindingPicker(
    label: String,
    current: Int,
    modifier: Modifier = Modifier,
    onChange: (Int) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    var showExtra by remember { mutableStateOf(false) }
    val currentLabel = TouchBindings.ALL_BUTTON_LABELS[current] ?: "Button $current"
    val entries =
        if (showExtra) TouchBindings.META_BUTTON_LABELS else TouchBindings.BUTTON_LABELS

    Column(modifier = modifier) {
        Text(label, color = Color.Gray, fontSize = 11.sp)
        TextButton(onClick = { expanded = true }) {
            Text(currentLabel, fontSize = 11.sp)
        }
        if (expanded) {
            AlertDialog(
                onDismissRequest = { expanded = false },
                title = { Text(label, fontSize = 14.sp) },
                text = {
                    val scrollState = rememberScrollState()
                    Box(Modifier.heightIn(max = 300.dp)) {
                        Column(Modifier.verticalScroll(scrollState)) {
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                verticalAlignment = Alignment.CenterVertically,
                            ) {
                                TextButton(onClick = { showExtra = !showExtra }) {
                                    Text(
                                        if (showExtra) "Standard" else "Extra",
                                        fontSize = 12.sp,
                                    )
                                }
                            }
                            entries.forEach { (idx, name) ->
                                Text(
                                    name,
                                    modifier =
                                        Modifier
                                            .fillMaxWidth()
                                            .clickable {
                                                onChange(idx)
                                                expanded = false
                                            }.padding(vertical = 8.dp, horizontal = 4.dp),
                                    fontSize = 12.sp,
                                    color =
                                        if (idx == current) {
                                            MaterialTheme.colorScheme.primary
                                        } else {
                                            MaterialTheme.colorScheme.onSurface
                                        },
                                )
                            }
                        }
                        ScrollArrows(scrollState)
                    }
                },
                confirmButton = {
                    TextButton(onClick = { expanded = false }) { Text("Cancel") }
                },
            )
        }
    }
}

@Composable
private fun CurvePicker(
    label: String,
    current: ResponseCurve,
    modifier: Modifier = Modifier,
    onChange: (ResponseCurve) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }

    Column(modifier = modifier) {
        Text(label, color = Color.Gray, fontSize = 11.sp)
        Box {
            TextButton(onClick = { expanded = true }) {
                Text(current.name.lowercase(), fontSize = 11.sp)
            }
            DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                ResponseCurve.entries.forEach { curve ->
                    DropdownMenuItem(
                        text = { Text(curve.name.lowercase().replace('_', ' '), fontSize = 12.sp) },
                        onClick = {
                            onChange(curve)
                            expanded = false
                        },
                    )
                }
            }
        }
    }
}

@Composable
private fun LabelEditor(
    label: String,
    current: String,
    modifier: Modifier = Modifier,
    onChange: (String) -> Unit,
) {
    var text by remember(current) { mutableStateOf(current) }

    Column(modifier = modifier) {
        Text(label, color = Color.Gray, fontSize = 11.sp)
        OutlinedTextField(
            value = text,
            onValueChange = {
                if (it.length <= 6) {
                    text = it
                    onChange(it)
                }
            },
            singleLine = true,
            modifier = Modifier.height(48.dp).fillMaxWidth(),
            textStyle = TextStyle(fontSize = 12.sp, color = Color.White),
        )
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Dialogs
// ═════════════════════════════════════════════════════════════════════════════

@Composable
private fun PresetPickerDialog(
    onDismiss: () -> Unit,
    onSelect: (TouchLayout) -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Load Preset") },
        text = {
            Column {
                Text(
                    "This will replace your current layout.",
                    fontSize = 13.sp,
                    color = Color(0xFFFF9800),
                )
                Spacer(Modifier.height(12.dp))
                TouchLayoutRepository.allPresets().forEach { preset ->
                    TextButton(
                        onClick = { onSelect(preset) },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text(preset.name, fontSize = 14.sp)
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

@Composable
private fun AddControlDialog(
    onDismiss: () -> Unit,
    onAddStick: () -> Unit,
    onAddButton: () -> Unit,
    onAddRadial: () -> Unit,
    onAddSlider: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Add Control") },
        text = {
            Column {
                TextButton(onClick = onAddStick, modifier = Modifier.fillMaxWidth()) {
                    Text("Analog Stick")
                }
                TextButton(onClick = onAddButton, modifier = Modifier.fillMaxWidth()) {
                    Text("Button")
                }
                TextButton(onClick = onAddRadial, modifier = Modifier.fillMaxWidth()) {
                    Text("Radial Menu")
                }
                TextButton(onClick = onAddSlider, modifier = Modifier.fillMaxWidth()) {
                    Text("Slider")
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

@Composable
private fun GlobalSettingsDialog(
    layout: TouchLayout,
    onDismiss: () -> Unit,
    onUpdate: (TouchLayout) -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Global Settings") },
        text = {
            Column {
                Text(
                    "Global Opacity: ${"%.0f".format(layout.globalOpacity * 100)}%",
                    fontSize = 13.sp,
                    color = Color.Gray,
                )
                Slider(
                    value = layout.globalOpacity,
                    onValueChange = { onUpdate(layout.copy(globalOpacity = it)) },
                    valueRange = TouchBindings.MIN_GLOBAL_OPACITY..TouchBindings.MAX_GLOBAL_OPACITY,
                )
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss) { Text("OK") }
        },
        dismissButton = {},
    )
}

@Composable
private fun GyroSettingsDialog(
    gyro: GyroConfig,
    onDismiss: () -> Unit,
    onUpdate: (GyroConfig) -> Unit,
) {
    var enabled by remember { mutableStateOf(gyro.enabled) }
    var activation by remember { mutableStateOf(gyro.activation) }
    var sensX by remember { mutableFloatStateOf(gyro.sensitivityX) }
    var sensY by remember { mutableFloatStateOf(gyro.sensitivityY) }
    var invertX by remember { mutableStateOf(gyro.invertX) }
    var invertY by remember { mutableStateOf(gyro.invertY) }
    var deadzone by remember { mutableFloatStateOf(gyro.deadzone) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Gyro Settings") },
        text = {
            val scrollState = rememberScrollState()
            Box {
                Column(modifier = Modifier.verticalScroll(scrollState)) {
                    LabeledToggle("Enabled", enabled) { enabled = it }

                    if (enabled) {
                        Spacer(Modifier.height(8.dp))
                        Text("Activation Mode", color = Color.Gray, fontSize = 12.sp)
                        GyroActivation.entries.forEach { mode ->
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                RadioButton(
                                    selected = activation == mode,
                                    onClick = { activation = mode },
                                    modifier = Modifier.size(20.dp),
                                )
                                Spacer(Modifier.width(4.dp))
                                Text(
                                    mode.name.lowercase().replace('_', ' '),
                                    fontSize = 12.sp,
                                    color = Color.White,
                                )
                            }
                        }

                        Spacer(Modifier.height(8.dp))
                        LabeledSlider("Sensitivity X", sensX, 0.1f, 5f) { sensX = it }
                        LabeledSlider("Sensitivity Y", sensY, 0.1f, 5f) { sensY = it }
                        LabeledSlider("Deadzone", deadzone, 0f, 0.1f) { deadzone = it }

                        Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                            LabeledToggle("Invert X", invertX) { invertX = it }
                            LabeledToggle("Invert Y", invertY) { invertY = it }
                        }
                    }
                }
                ScrollArrows(scrollState)
            }
        },
        confirmButton = {
            TextButton(onClick = {
                onUpdate(
                    gyro.copy(
                        enabled = enabled,
                        activation = activation,
                        sensitivityX = sensX,
                        sensitivityY = sensY,
                        invertX = invertX,
                        invertY = invertY,
                        deadzone = deadzone,
                    ),
                )
                onDismiss()
            }) { Text("OK") }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Cancel") }
        },
    )
}

// ═════════════════════════════════════════════════════════════════════════════
// Scroll Indicators
// ═════════════════════════════════════════════════════════════════════════════

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
