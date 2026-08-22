package com.dxxredux.app

import android.app.Activity
import android.content.Context
import android.content.pm.ActivityInfo
import android.content.res.Configuration
import android.widget.Toast
import androidx.activity.compose.BackHandler
import androidx.activity.compose.rememberLauncherForActivityResult
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
import androidx.compose.ui.focus.FocusDirection
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.drawText
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import kotlinx.coroutines.launch
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.roundToInt
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
private val cReticleBright = Color(0xFF20FF20)
private val cReticleDark = Color(0x99408040)
private const val SELECTED_TYPE_STICK_ZONE_EDGE = "stickZoneEdge"
private const val SELECTED_TYPE_AXIS_REGION_EDGE = "axisRegionEdge"
private const val SELECTED_TYPE_MORE_ACTIONS = "moreActions"
private const val TOUCH_EDITOR_TOOLBAR_ACTION_COUNT = 9
private const val TOUCH_EDITOR_TOOLBAR_SAVE_INDEX = 8

internal fun nextTouchEditorToolbarFocusIndex(
    currentIndex: Int,
    saveEnabled: Boolean,
    direction: Int,
): Int {
    if (direction == 0) return currentIndex.coerceIn(0, TOUCH_EDITOR_TOOLBAR_ACTION_COUNT - 1)
    var index = currentIndex.coerceIn(0, TOUCH_EDITOR_TOOLBAR_ACTION_COUNT - 1)
    repeat(TOUCH_EDITOR_TOOLBAR_ACTION_COUNT - 1) {
        index = (index + direction + TOUCH_EDITOR_TOOLBAR_ACTION_COUNT) % TOUCH_EDITOR_TOOLBAR_ACTION_COUNT
        if (saveEnabled || index != TOUCH_EDITOR_TOOLBAR_SAVE_INDEX) return index
    }
    return currentIndex.coerceIn(0, TOUCH_EDITOR_TOOLBAR_ACTION_COUNT - 1)
}

internal enum class FloatingZoneEdge {
    LEFT,
    TOP,
    RIGHT,
    BOTTOM,
}

private val floatingZoneEdges = FloatingZoneEdge.values()

internal fun encodeFloatingZoneEdgeSelection(
    index: Int,
    edge: FloatingZoneEdge,
): Int = index * floatingZoneEdges.size + edge.ordinal

internal fun decodeFloatingZoneEdgeSelection(selectionIndex: Int): Pair<Int, FloatingZoneEdge> {
    val edgeIndex = selectionIndex % floatingZoneEdges.size
    return selectionIndex / floatingZoneEdges.size to floatingZoneEdges[edgeIndex]
}

internal fun defaultTouchEditorEdgeHitSlopPx(buttonRadius: Float): Float = max(buttonRadius * 0.3f, 12f)

internal data class DefaultReticlePreviewGeometry(
    val center: Offset,
    val unit: Float,
)

internal fun defaultReticlePreviewGeometry(
    width: Float,
    height: Float,
): DefaultReticlePreviewGeometry =
    DefaultReticlePreviewGeometry(
        center = Offset(width / 2f, height / 2f),
        unit = min(width, height) / 240f,
    )

internal fun resizeFloatingZone(
    zone: FloatingZone,
    edge: FloatingZoneEdge,
    dxPct: Float,
    dyPct: Float,
    minSizePct: Float = MIN_TOUCH_ZONE_SIZE_PCT,
): FloatingZone =
    when (edge) {
        FloatingZoneEdge.LEFT -> {
            val newLeft = (zone.leftPct + dxPct).coerceIn(0f, zone.rightPct - minSizePct)
            zone.copy(leftPct = newLeft)
        }

        FloatingZoneEdge.TOP -> {
            val newTop = (zone.topPct + dyPct).coerceIn(0f, zone.bottomPct - minSizePct)
            zone.copy(topPct = newTop)
        }

        FloatingZoneEdge.RIGHT -> {
            val newRight = (zone.rightPct + dxPct).coerceIn(zone.leftPct + minSizePct, 100f)
            zone.copy(rightPct = newRight)
        }

        FloatingZoneEdge.BOTTOM -> {
            val newBottom = (zone.bottomPct + dyPct).coerceIn(zone.topPct + minSizePct, 100f)
            zone.copy(bottomPct = newBottom)
        }
    }

internal fun setFloatingZoneEdge(
    zone: FloatingZone,
    edge: FloatingZoneEdge,
    valuePct: Float,
): FloatingZone =
    when (edge) {
        FloatingZoneEdge.LEFT -> resizeFloatingZone(zone, edge, valuePct - zone.leftPct, 0f)
        FloatingZoneEdge.TOP -> resizeFloatingZone(zone, edge, 0f, valuePct - zone.topPct)
        FloatingZoneEdge.RIGHT -> resizeFloatingZone(zone, edge, valuePct - zone.rightPct, 0f)
        FloatingZoneEdge.BOTTOM -> resizeFloatingZone(zone, edge, 0f, valuePct - zone.bottomPct)
    }

private fun resolveEditorSelection(
    type: String?,
    index: Int,
): Pair<String, Int>? =
    when (type) {
        null -> null
        SELECTED_TYPE_STICK_ZONE_EDGE -> decodeFloatingZoneEdgeSelection(index).let { "stick" to it.first }
        SELECTED_TYPE_AXIS_REGION_EDGE -> decodeFloatingZoneEdgeSelection(index).let { "axisRegion" to it.first }
        else -> type to index
    }

private fun selectedZoneEdge(
    selectedType: String?,
    selectedIndex: Int,
    edgeSelectionType: String,
    controlIndex: Int,
): FloatingZoneEdge? {
    if (selectedType != edgeSelectionType) return null
    val (selectedControlIndex, edge) = decodeFloatingZoneEdgeSelection(selectedIndex)
    return if (selectedControlIndex == controlIndex) edge else null
}

private fun DrawScope.drawSelectedZoneEdge(
    left: Float,
    top: Float,
    right: Float,
    bottom: Float,
    edge: FloatingZoneEdge,
) {
    when (edge) {
        FloatingZoneEdge.LEFT -> drawLine(cSelected, Offset(left, top), Offset(left, bottom), strokeWidth = 4f)
        FloatingZoneEdge.TOP -> drawLine(cSelected, Offset(left, top), Offset(right, top), strokeWidth = 4f)
        FloatingZoneEdge.RIGHT -> drawLine(cSelected, Offset(right, top), Offset(right, bottom), strokeWidth = 4f)
        FloatingZoneEdge.BOTTOM -> drawLine(cSelected, Offset(left, bottom), Offset(right, bottom), strokeWidth = 4f)
    }
}

private fun addFloatingZoneEdgeHits(
    hits: MutableList<Pair<String, Int>>,
    selectionType: String,
    controlIndex: Int,
    zone: FloatingZone,
    offset: Offset,
    canvasWidth: Float,
    canvasHeight: Float,
    edgeHitSlopPx: Float,
) {
    val left = canvasWidth * zone.leftPct / 100f
    val top = canvasHeight * zone.topPct / 100f
    val right = canvasWidth * zone.rightPct / 100f
    val bottom = canvasHeight * zone.bottomPct / 100f

    if (offset.x in (left - edgeHitSlopPx)..(left + edgeHitSlopPx) &&
        offset.y in (top - edgeHitSlopPx)..(bottom + edgeHitSlopPx)
    ) {
        hits.add(selectionType to encodeFloatingZoneEdgeSelection(controlIndex, FloatingZoneEdge.LEFT))
    }
    if (offset.x in (left - edgeHitSlopPx)..(right + edgeHitSlopPx) &&
        offset.y in (top - edgeHitSlopPx)..(top + edgeHitSlopPx)
    ) {
        hits.add(selectionType to encodeFloatingZoneEdgeSelection(controlIndex, FloatingZoneEdge.TOP))
    }
    if (offset.x in (right - edgeHitSlopPx)..(right + edgeHitSlopPx) &&
        offset.y in (top - edgeHitSlopPx)..(bottom + edgeHitSlopPx)
    ) {
        hits.add(selectionType to encodeFloatingZoneEdgeSelection(controlIndex, FloatingZoneEdge.RIGHT))
    }
    if (offset.x in (left - edgeHitSlopPx)..(right + edgeHitSlopPx) &&
        offset.y in (bottom - edgeHitSlopPx)..(bottom + edgeHitSlopPx)
    ) {
        hits.add(selectionType to encodeFloatingZoneEdgeSelection(controlIndex, FloatingZoneEdge.BOTTOM))
    }
}

/**
 * Full-screen touch layout editor.
 * Displays controls on a canvas; tap to select, drag to move, bottom panel for properties.
 */
@OptIn(ExperimentalLayoutApi::class, ExperimentalMaterial3Api::class)
@Composable
fun TouchEditorPage(
    gameVariant: String = "d2",
    onBack: () -> Unit,
) {
    BackHandler(onBack = onBack)
    val context = LocalContext.current
    var touchSlots by remember { mutableStateOf(TouchLayoutSlotRepository.load(context)) }
    var layout by remember { mutableStateOf(touchSlots.activeSlot.value) }
    val activeSlotName = touchSlots.activeSlot.name
    var selectedType by remember { mutableStateOf<String?>(null) }
    var selectedIndex by remember { mutableIntStateOf(-1) }
    var dirty by remember { mutableStateOf(false) }
    var canvasWidth by remember { mutableFloatStateOf(1f) }
    var canvasHeight by remember { mutableFloatStateOf(1f) }

    // Cycling state for stacked controls: tracks the last hit list and position in cycle
    var cycleHits by remember { mutableStateOf<List<Pair<String, Int>>>(emptyList()) }
    var cycleIndex by remember { mutableIntStateOf(0) }
    var lastTapOffset by remember { mutableStateOf(Offset.Unspecified) }

    // Live gyro values for editor diagnostic preview
    var gyroYaw by remember { mutableFloatStateOf(0f) }
    var gyroPitch by remember { mutableFloatStateOf(0f) }
    var gyroRoll by remember { mutableFloatStateOf(0f) }
    var editorGyroManager by remember { mutableStateOf<GyroInputManager?>(null) }
    // Buffered ref values from calibration -- written to layout on save, not on every callback
    val pendingRef = remember { mutableStateOf<Triple<Float, Float, Float>?>(null) }
    val hasDiagnostics = layout.diagnostics.isNotEmpty()
    val gyroEnabled = layout.gyro.enabled
    if (hasDiagnostics && gyroEnabled) {
        // Create the manager once; don't restart on every config change
        DisposableEffect(Unit) {
            val gm = GyroInputManager(context)
            gm.setConfig(layout.gyro)
            gm.diagnosticCallback = { y, p, r ->
                gyroYaw = y
                gyroPitch = p
                gyroRoll = r
            }
            gm.onCalibrated = { az, pi, ro ->
                pendingRef.value = Triple(az, pi, ro)
                dirty = true
            }
            editorGyroManager = gm
            gm.resume()
            onDispose {
                gm.pause()
                editorGyroManager = null
            }
        }
        // Push config changes to existing manager without restarting it
        val gyroConfig = layout.gyro
        LaunchedEffect(gyroConfig) {
            editorGyroManager?.setConfig(gyroConfig)
        }
    }

    // Lock orientation and hide system bars to match in-game while editor is open
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
        // Hide system bars so controls at the top of the screen are reachable
        val window = activity?.window
        val insetsController =
            window?.let {
                WindowCompat.setDecorFitsSystemWindows(it, false)
                WindowInsetsControllerCompat(it, it.decorView).apply {
                    hide(WindowInsetsCompat.Type.systemBars())
                    systemBarsBehavior =
                        WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
                }
            }
        onDispose {
            activity?.requestedOrientation = prev ?: ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
            insetsController?.show(WindowInsetsCompat.Type.systemBars())
        }
    }

    // Dialogs
    var showSlotDialog by remember { mutableStateOf(false) }
    var showPresetPicker by remember { mutableStateOf(false) }
    var showAddControl by remember { mutableStateOf(false) }
    var showGlobalSettings by remember { mutableStateOf(false) }
    var showGyroSettings by remember { mutableStateOf(false) }
    var longPressPos by remember { mutableStateOf(Offset.Zero) } // where to place new control
    val configImportScope = rememberCoroutineScope()

    // SAF file picker for importing touch layouts
    val importPickerLauncher =
        rememberLauncherForActivityResult(
            contract =
                androidx.activity.result.contract.ActivityResultContracts
                    .OpenDocument(),
        ) { uri ->
            if (uri == null) return@rememberLauncherForActivityResult
            configImportScope.launch {
                val msg = ConfigImportExport.importFromUri(context, uri)
                Toast.makeText(context, msg, Toast.LENGTH_LONG).show()
                // Reload after import
                touchSlots = TouchLayoutSlotRepository.load(context)
                layout = touchSlots.activeSlot.value
                dirty = false
            }
        }

    // Save helper
    fun save() {
        // Flush any pending gyro calibration reference into the layout before saving
        val ref = pendingRef.value
        if (ref != null) {
            layout =
                layout.copy(
                    gyro =
                        layout.gyro.copy(
                            refAzimuth = ref.first,
                            refPitch = ref.second,
                            refRoll = ref.third,
                        ),
                )
            pendingRef.value = null
        }
        touchSlots = TouchLayoutSlotRepository.saveActiveLayout(context, layout)
        dirty = false
    }

    val sheetState =
        rememberBottomSheetScaffoldState(
            bottomSheetState = rememberStandardBottomSheetState(initialValue = SheetValue.PartiallyExpanded),
        )
    val coroutineScope = rememberCoroutineScope()
    val isPortrait = LocalConfiguration.current.orientation == Configuration.ORIENTATION_PORTRAIT
    val targetOverlayBounds = activity?.windowManager?.currentWindowMetrics?.bounds
    val targetOverlayWidth = targetOverlayBounds?.width()?.toFloat()?.takeIf { it > 0f } ?: canvasWidth
    val targetOverlayHeight = targetOverlayBounds?.height()?.toFloat()?.takeIf { it > 0f } ?: canvasHeight
    val density = LocalDensity.current
    var toolbarHeightPx by remember { mutableIntStateOf(0) }
    val measuredToolbarPeekHeight =
        if (toolbarHeightPx > 0) {
            with(density) { toolbarHeightPx.toDp() }
        } else {
            0.dp
        }
    val toolbarPeekHeight =
        if (measuredToolbarPeekHeight > 48.dp) {
            measuredToolbarPeekHeight
        } else if (isPortrait) {
            108.dp
        } else {
            48.dp
        }
    val toolbarFocusRequesters = remember { List(TOUCH_EDITOR_TOOLBAR_ACTION_COUNT) { FocusRequester() } }
    RequestLauncherControllerFocus(toolbarFocusRequesters[0], true)

    fun moveToolbarFocus(
        fromIndex: Int,
        direction: Int,
    ): Boolean {
        val nextIndex = nextTouchEditorToolbarFocusIndex(fromIndex, dirty, direction)
        if (nextIndex == fromIndex) return false
        toolbarFocusRequesters[nextIndex].requestFocusSafely()
        return true
    }

    fun toolbarButtonModifier(index: Int): Modifier =
        Modifier
            .focusRequester(toolbarFocusRequesters[index])
            .onPreviewKeyEvent { event ->
                val direction =
                    when (event.key) {
                        Key.DirectionLeft -> -1
                        Key.DirectionRight -> 1
                        else -> 0
                    }
                if (direction == 0) return@onPreviewKeyEvent false
                if (event.type == KeyEventType.KeyDown) {
                    moveToolbarFocus(index, direction)
                }
                true
            }.tvFocusBorder()

    BottomSheetScaffold(
        scaffoldState = sheetState,
        sheetPeekHeight = toolbarPeekHeight,
        sheetContainerColor = Color(0xFF262626),
        sheetContentColor = Color.White,
        sheetDragHandle = null,
        containerColor = cBackground,
        sheetContent = {
            // ── Toolbar row (always visible as peek content) ──
            FlowRow(
                modifier =
                    Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 8.dp, vertical = 4.dp)
                        .onSizeChanged { toolbarHeightPx = it.height },
                horizontalArrangement = Arrangement.spacedBy(4.dp),
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                Text(
                    text = "slot: $activeSlotName",
                    modifier = Modifier.widthIn(max = 96.dp).padding(end = 4.dp),
                    fontSize = 9.sp,
                    color = Color(0x99FFFFFF.toInt()),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                TextButton(onClick = {
                    if (dirty) save()
                    onBack()
                }, modifier = toolbarButtonModifier(0)) {
                    Text("Close Editor", fontSize = 12.sp, color = Color.White)
                }
                TextButton(onClick = { showSlotDialog = true }, modifier = toolbarButtonModifier(1)) {
                    Text("Slots", fontSize = 12.sp)
                }
                TextButton(onClick = { showPresetPicker = true }, modifier = toolbarButtonModifier(2)) {
                    Text("Presets", fontSize = 12.sp)
                }
                TextButton(onClick = { showGlobalSettings = true }, modifier = toolbarButtonModifier(3)) {
                    Text("Global", fontSize = 12.sp)
                }
                TextButton(onClick = { showGyroSettings = true }, modifier = toolbarButtonModifier(4)) {
                    Text("Gyro", fontSize = 12.sp)
                }
                IconButton(
                    onClick = { showAddControl = true },
                    modifier = toolbarButtonModifier(5).size(36.dp),
                ) {
                    Icon(Icons.Default.Add, "Add control", tint = Color.White)
                }
                TextButton(onClick = {
                    save()
                    if (!ConfigImportExport.exportTouchLayout(context)) {
                        Toast.makeText(context, "Export failed", Toast.LENGTH_SHORT).show()
                    }
                }, modifier = toolbarButtonModifier(6)) {
                    Text("Export", fontSize = 12.sp, color = Color.White)
                }
                TextButton(onClick = {
                    importPickerLauncher.launch(arrayOf("application/json", "*/*"))
                }, modifier = toolbarButtonModifier(7)) {
                    Text("Import", fontSize = 12.sp, color = Color.White)
                }
                TextButton(onClick = { save() }, enabled = dirty, modifier = toolbarButtonModifier(8)) {
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
                val selectedTarget = resolveEditorSelection(selectedType, selectedIndex)
                if (selectedTarget != null) {
                    val (panelType, panelIndex) = selectedTarget
                    val panelScrollState = rememberScrollState()
                    Box(modifier = Modifier.heightIn(max = 300.dp)) {
                        Column(
                            modifier =
                                Modifier
                                    .verticalScroll(panelScrollState)
                                    .padding(horizontal = 12.dp, vertical = 8.dp),
                        ) {
                            when (panelType) {
                                "stick" -> {
                                    StickPropertiesPanel(
                                        stick = layout.sticks[panelIndex],
                                        gameVariant = gameVariant,
                                        onUpdate = { updated ->
                                            layout =
                                                layout.copy(
                                                    sticks =
                                                        layout.sticks.toMutableList().also {
                                                            it[panelIndex] = updated
                                                        },
                                                )
                                            dirty = true
                                        },
                                        onDelete = {
                                            layout =
                                                layout.copy(
                                                    sticks =
                                                        layout.sticks.toMutableList().also {
                                                            it.removeAt(panelIndex)
                                                        },
                                                )
                                            selectedType = null
                                            selectedIndex = -1
                                            dirty = true
                                        },
                                    )
                                }

                                "button" -> {
                                    ButtonPropertiesPanel(
                                        button = layout.buttons[panelIndex],
                                        gameVariant = gameVariant,
                                        onUpdate = { updated ->
                                            layout =
                                                layout.copy(
                                                    buttons =
                                                        layout.buttons.toMutableList().also {
                                                            it[panelIndex] = updated
                                                        },
                                                )
                                            dirty = true
                                        },
                                        onDelete = {
                                            layout =
                                                layout.copy(
                                                    buttons =
                                                        layout.buttons.toMutableList().also {
                                                            it.removeAt(panelIndex)
                                                        },
                                                )
                                            selectedType = null
                                            selectedIndex = -1
                                            dirty = true
                                        },
                                    )
                                }

                                "radial" -> {
                                    RadialPropertiesPanel(
                                        radial = layout.radialMenus[panelIndex],
                                        gameVariant = gameVariant,
                                        canvasWidth = canvasWidth,
                                        canvasHeight = canvasHeight,
                                        onUpdate = { updated ->
                                            layout =
                                                layout.copy(
                                                    radialMenus =
                                                        layout.radialMenus.toMutableList().also {
                                                            it[panelIndex] = updated
                                                        },
                                                )
                                            dirty = true
                                        },
                                        onDelete = {
                                            layout =
                                                layout.copy(
                                                    radialMenus =
                                                        layout.radialMenus.toMutableList().also {
                                                            it.removeAt(panelIndex)
                                                        },
                                                )
                                            selectedType = null
                                            selectedIndex = -1
                                            dirty = true
                                        },
                                    )
                                }

                                "slider" -> {
                                    SliderPropertiesPanel(
                                        slider = layout.sliders[panelIndex],
                                        onUpdate = { updated ->
                                            layout =
                                                layout.copy(
                                                    sliders =
                                                        layout.sliders.toMutableList().also {
                                                            it[panelIndex] = updated
                                                        },
                                                )
                                            dirty = true
                                        },
                                        onDelete = {
                                            layout =
                                                layout.copy(
                                                    sliders =
                                                        layout.sliders.toMutableList().also {
                                                            it.removeAt(panelIndex)
                                                        },
                                                )
                                            selectedType = null
                                            selectedIndex = -1
                                            dirty = true
                                        },
                                    )
                                }

                                "diagnostic" -> {
                                    DiagnosticPropertiesPanel(
                                        diag = layout.diagnostics[panelIndex],
                                        onUpdate = { updated ->
                                            layout =
                                                layout.copy(
                                                    diagnostics =
                                                        layout.diagnostics.toMutableList().also {
                                                            it[panelIndex] = updated
                                                        },
                                                )
                                            dirty = true
                                        },
                                        onDelete = {
                                            layout =
                                                layout.copy(
                                                    diagnostics =
                                                        layout.diagnostics.toMutableList().also {
                                                            it.removeAt(panelIndex)
                                                        },
                                                )
                                            selectedType = null
                                            selectedIndex = -1
                                            dirty = true
                                        },
                                    )
                                }

                                "axisRegion" -> {
                                    AxisRegionPropertiesPanel(
                                        region = layout.axisRegions[panelIndex],
                                        onUpdate = { updated ->
                                            layout =
                                                layout.copy(
                                                    axisRegions =
                                                        layout.axisRegions.toMutableList().also {
                                                            it[panelIndex] = updated
                                                        },
                                                )
                                            dirty = true
                                        },
                                        onDelete = {
                                            layout =
                                                layout.copy(
                                                    axisRegions =
                                                        layout.axisRegions.toMutableList().also {
                                                            it.removeAt(panelIndex)
                                                        },
                                                )
                                            selectedType = null
                                            selectedIndex = -1
                                            dirty = true
                                        },
                                    )
                                }

                                SELECTED_TYPE_MORE_ACTIONS -> {
                                    MoreActionsPropertiesPanel(
                                        control = layout.moreActions,
                                        onUpdate = { updated ->
                                            layout = layout.copy(moreActions = updated)
                                            dirty = true
                                        },
                                    )
                                }
                            }
                        }
                        ScrollArrows(panelScrollState)
                    }
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
        val targetOverlayWidthRef = rememberUpdatedState(targetOverlayWidth)
        val targetOverlayHeightRef = rememberUpdatedState(targetOverlayHeight)

        Box(modifier = Modifier.fillMaxSize()) {
            Canvas(
                modifier =
                    Modifier
                        .fillMaxSize()
                        .padding(innerPadding)
                        .pointerInput(Unit) {
                            detectTapGestures(
                                onTap = { offset ->
                                    val hits =
                                        hitTestAll(
                                            layoutRef.value,
                                            offset,
                                            canvasWidth,
                                            canvasHeight,
                                            targetOverlayWidthRef.value,
                                            targetOverlayHeightRef.value,
                                        )
                                    if (hits.isEmpty()) {
                                        selectedType = null
                                        selectedIndex = -1
                                        cycleHits = emptyList()
                                    } else if (hits.size == 1) {
                                        selectedType = hits[0].first
                                        selectedIndex = hits[0].second
                                        cycleHits = hits
                                        cycleIndex = 0
                                    } else {
                                        // Multiple controls stacked: cycle through them
                                        val nearPrev =
                                            lastTapOffset != Offset.Unspecified &&
                                                (offset - lastTapOffset).getDistance() < 30f
                                        val sameList = nearPrev && cycleHits == hits
                                        val current = Pair(selectedType, selectedIndex)
                                        val nextIdx =
                                            if (sameList) {
                                                // Advance to next that isn't the current selection
                                                val start = (cycleIndex + 1) % hits.size
                                                if (hits[start] != current) {
                                                    start
                                                } else {
                                                    (start + 1) % hits.size
                                                }
                                            } else {
                                                // New tap location: if current is in hits, pick next; else first
                                                val curPos = hits.indexOf(current)
                                                if (curPos >= 0) (curPos + 1) % hits.size else 0
                                            }
                                        cycleHits = hits
                                        cycleIndex = nextIdx
                                        selectedType = hits[nextIdx].first
                                        selectedIndex = hits[nextIdx].second
                                    }
                                    lastTapOffset = offset

                                    // In the editor, tapping a gyro recenter button triggers recenter
                                    if (selectedType == "button" && selectedIndex >= 0) {
                                        val btn = layoutRef.value.buttons.getOrNull(selectedIndex)
                                        if (btn?.binding == TouchBindings.BTN_GYRO_RECENTER) {
                                            editorGyroManager?.calibrate()
                                        }
                                    }
                                },
                                onLongPress = { offset ->
                                    val hit =
                                        hitTest(
                                            layoutRef.value,
                                            offset,
                                            canvasWidth,
                                            canvasHeight,
                                            targetOverlayWidthRef.value,
                                            targetOverlayHeightRef.value,
                                        )
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
                            var dragMovesSelectedControl = false
                            detectDragGestures(
                                onDragStart = { offset ->
                                    val selected = Pair(selTypeRef.value, selIdxRef.value)
                                    dragMovesSelectedControl =
                                        hitTestAll(
                                            layoutRef.value,
                                            offset,
                                            canvasWidth,
                                            canvasHeight,
                                            targetOverlayWidthRef.value,
                                            targetOverlayHeightRef.value,
                                        ).contains(selected)
                                },
                                onDragEnd = { dragMovesSelectedControl = false },
                                onDragCancel = { dragMovesSelectedControl = false },
                            ) { change, dragAmount ->
                                change.consume()
                                val st = selTypeRef.value
                                val si = selIdxRef.value
                                if (dragMovesSelectedControl && st != null && si >= 0) {
                                    val lay = layoutRef.value
                                    val dxPct = (dragAmount.x / canvasWidth) * 100f
                                    val dyPct = (dragAmount.y / canvasHeight) * 100f
                                    layout = moveControl(lay, st, si, dxPct, dyPct, canvasWidth, canvasHeight)
                                    dirty = true
                                }
                            }
                        },
            ) {
                canvasWidth = size.width
                canvasHeight = size.height
                drawGrid(this)
                drawDefaultReticlePreview(this)
                drawAllControls(
                    this,
                    layout,
                    selectedType,
                    selectedIndex,
                    textMeasurer,
                    targetOverlayWidth,
                    targetOverlayHeight,
                    gyroYaw,
                    gyroPitch,
                    gyroRoll,
                )
            }
        }
    }

    // ── Dialogs ──────────────────────────────────────────────────────────────
    if (showSlotDialog) {
        ConfigSlotDialog(
            title = "Touch Slots",
            slotNames = touchSlots.slots.map { slot -> slot.name },
            activeIndex = touchSlots.safeActiveIndex,
            onSelectSlot = { slotIndex ->
                if (dirty) save()
                touchSlots = TouchLayoutSlotRepository.selectSlot(context, slotIndex)
                layout = touchSlots.activeSlot.value
                selectedType = null
                selectedIndex = -1
                dirty = false
            },
            onRenameActiveSlot = { name ->
                touchSlots = TouchLayoutSlotRepository.renameActiveSlot(context, name)
            },
            onNewSlot = { name ->
                if (dirty) save()
                touchSlots = TouchLayoutSlotRepository.addDefaultSlot(context, name)
                layout = touchSlots.activeSlot.value
                selectedType = null
                selectedIndex = -1
                dirty = false
            },
            onDuplicateActiveSlot = { name ->
                touchSlots = TouchLayoutSlotRepository.duplicateActiveSlot(context, name, layout)
                layout = touchSlots.activeSlot.value
                selectedType = null
                selectedIndex = -1
                dirty = false
            },
            onDeleteActiveSlot = {
                touchSlots = TouchLayoutSlotRepository.deleteActiveSlot(context)
                layout = touchSlots.activeSlot.value
                selectedType = null
                selectedIndex = -1
                dirty = false
            },
            onDismiss = { showSlotDialog = false },
        )
    }
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
            onAddRadial = { presentation ->
                layout =
                    layout.copy(
                        radialMenus =
                            layout.radialMenus +
                                RadialMenuControl(
                                    id = "menu_${layout.radialMenus.size}",
                                    xPct = addX,
                                    yPct = addY,
                                    presentation = presentation,
                                    segments =
                                        listOf(
                                            RadialSegment(
                                                "Fire Primary",
                                                TouchBindings.BTN_FIRE_PRIMARY,
                                                bindingType = "action",
                                            ),
                                            RadialSegment(
                                                "Fire Secondary",
                                                TouchBindings.BTN_FIRE_SECONDARY,
                                                bindingType = "action",
                                            ),
                                            RadialSegment(
                                                "Fire Flare",
                                                TouchBindings.BTN_FIRE_FLARE,
                                                bindingType = "action",
                                            ),
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
            onAddDiagnostic = { diagType ->
                layout =
                    layout.copy(
                        diagnostics =
                            layout.diagnostics +
                                DiagnosticControl(
                                    id = "diag_${layout.diagnostics.size}",
                                    xPct = addX,
                                    yPct = addY,
                                    type = diagType,
                                ),
                    )
                selectedType = "diagnostic"
                selectedIndex = layout.diagnostics.lastIndex
                dirty = true
                showAddControl = false
                longPressPos = Offset.Zero
            },
            onAddAxisRegion = {
                layout =
                    layout.copy(
                        axisRegions =
                            layout.axisRegions +
                                AxisRegionControl(
                                    id = "region_${layout.axisRegions.size}",
                                ),
                    )
                selectedType = "axisRegion"
                selectedIndex = layout.axisRegions.lastIndex
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
            onRecenter = { editorGyroManager?.calibrate() },
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
}

/** Launcher-drawn approximation of the shared D1/D2 default reticle */
private fun drawDefaultReticlePreview(scope: DrawScope) {
    val (center, unit) = defaultReticlePreviewGeometry(scope.size.width, scope.size.height)
    val strokeWidth = max(1.5f, unit * 0.55f)

    fun point(
        x: Float,
        y: Float,
    ) = Offset(center.x + x * unit, center.y - y * unit)

    fun bar(
        color: Color,
        first: Offset,
        second: Offset,
        third: Offset,
        fourth: Offset,
    ) {
        val path =
            Path().apply {
                moveTo(first.x, first.y)
                lineTo(second.x, second.y)
                lineTo(third.x, third.y)
                lineTo(fourth.x, fourth.y)
                close()
            }
        scope.drawPath(path, color)
    }

    // Center brackets
    scope.drawLine(cReticleBright, point(-4f, 2f), point(-2f, 0f), strokeWidth)
    scope.drawLine(cReticleBright, point(-3f, -4f), point(-2f, -3f), strokeWidth)
    scope.drawLine(cReticleBright, point(4f, 2f), point(2f, 0f), strokeWidth)
    scope.drawLine(cReticleBright, point(3f, -4f), point(2f, -3f), strokeWidth)

    // Primary weapon bars
    bar(cReticleBright, point(-5.5f, -5f), point(-6.5f, -7.5f), point(-10f, -7f), point(-10f, -8.7f))
    bar(cReticleDark, point(-10f, -7f), point(-10f, -8.7f), point(-15f, -8.5f), point(-15f, -9.5f))
    bar(cReticleBright, point(5.5f, -5f), point(6.5f, -7.5f), point(10f, -7f), point(10f, -8.7f))
    bar(cReticleDark, point(10f, -7f), point(10f, -8.7f), point(15f, -8.5f), point(15f, -9.5f))

    // Secondary weapon indicators
    scope.drawCircle(cReticleBright, 2f * unit, point(-10f, -2f), style = Stroke(strokeWidth))
    scope.drawCircle(cReticleDark, 2f * unit, point(10f, -2f), style = Stroke(strokeWidth))
}

private fun drawAllControls(
    scope: DrawScope,
    layout: TouchLayout,
    selType: String?,
    selIdx: Int,
    textMeasurer: androidx.compose.ui.text.TextMeasurer,
    targetSurfaceWidth: Float,
    targetSurfaceHeight: Float,
    gyroYaw: Float = 0f,
    gyroPitch: Float = 0f,
    gyroRoll: Float = 0f,
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
        val selectedEdge = selectedZoneEdge(selType, selIdx, SELECTED_TYPE_STICK_ZONE_EDGE, i)
        val alpha = layout.globalOpacity * stick.opacity
        val hasEditableZone = stick.floating || stick.mouseMode
        var zoneLeft = 0f
        var zoneTop = 0f
        var zoneRight = 0f
        var zoneBottom = 0f

        // Floating zone indicator
        if (hasEditableZone) {
            val fz = stick.floatingZone
            zoneLeft = w * fz.leftPct / 100f
            zoneTop = h * fz.topPct / 100f
            zoneRight = w * fz.rightPct / 100f
            zoneBottom = h * fz.bottomPct / 100f
            val fzTopLeft = Offset(zoneLeft, zoneTop)
            val fzSize =
                androidx.compose.ui.geometry.Size(
                    zoneRight - zoneLeft,
                    zoneBottom - zoneTop,
                )
            scope.drawRect(
                color = Color(0x1488CCFF),
                topLeft = fzTopLeft,
                size = fzSize,
            )
            if (stick.mouseMode) {
                // In mouse mode, draw a visible border instead of the stick circle
                scope.drawRect(
                    color = cStickRing.copy(alpha = alpha),
                    topLeft = fzTopLeft,
                    size = fzSize,
                    style = Stroke(width = 2f),
                )
            }
        }

        if (stick.mouseMode) {
            // Mouse mode: selection highlight is a rect around the floating zone
            val fz = stick.floatingZone
            val fzTopLeft = Offset(w * fz.leftPct / 100f, h * fz.topPct / 100f)
            val fzSize =
                androidx.compose.ui.geometry.Size(
                    w * (fz.rightPct - fz.leftPct) / 100f,
                    h * (fz.bottomPct - fz.topPct) / 100f,
                )
            if (selected) {
                scope.drawRect(
                    color = cSelected,
                    topLeft = Offset(w * fz.leftPct / 100f - 3f, h * fz.topPct / 100f - 3f),
                    size =
                        androidx.compose.ui.geometry.Size(
                            w * (fz.rightPct - fz.leftPct) / 100f + 6f,
                            h * (fz.bottomPct - fz.topPct) / 100f + 6f,
                        ),
                    style = Stroke(width = 3f),
                )
            } else if (selectedEdge != null) {
                scope.drawSelectedZoneEdge(
                    left = fzTopLeft.x,
                    top = fzTopLeft.y,
                    right = fzTopLeft.x + fzSize.width,
                    bottom = fzTopLeft.y + fzSize.height,
                    edge = selectedEdge,
                )
            }
        } else {
            // Normal stick mode: draw ring and thumb
            scope.drawCircle(
                color = cStickRing.copy(alpha = alpha),
                radius = r,
                center = Offset(cx, cy),
                style = Stroke(width = 3f),
            )
            scope.drawCircle(
                color = cStickThumb.copy(alpha = alpha * 0.6f),
                radius = r * 0.35f,
                center = Offset(cx, cy),
            )
            if (selected) {
                scope.drawCircle(
                    color = cSelected,
                    radius = r + 4f,
                    center = Offset(cx, cy),
                    style = Stroke(width = 3f),
                )
            }
        }
        if (selectedEdge != null && hasEditableZone && !stick.mouseMode) {
            scope.drawSelectedZoneEdge(zoneLeft, zoneTop, zoneRight, zoneBottom, selectedEdge)
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

    // Draw built-in More actions button
    val more = layout.moreActions
    val moreBase = min(w, h)
    val moreH = h * 0.05f * more.sizeMult
    val moreW = max(w * 0.16f, moreBase * 0.16f) * more.sizeMult
    val moreCx = w * more.xPct / 100f
    val moreCy = h * more.yPct / 100f
    val moreSelected = selType == SELECTED_TYPE_MORE_ACTIONS
    val moreAlpha = layout.globalOpacity * more.opacity
    scope.drawRoundRect(
        color = cButton.copy(alpha = moreAlpha),
        topLeft = Offset(moreCx - moreW / 2f, moreCy - moreH / 2f),
        size = ComposeSize(moreW, moreH),
        cornerRadius = CornerRadius(moreH * 0.45f),
    )
    if (moreSelected) {
        scope.drawRoundRect(
            color = cSelected,
            topLeft = Offset(moreCx - moreW / 2f - 3f, moreCy - moreH / 2f - 3f),
            size = ComposeSize(moreW + 6f, moreH + 6f),
            cornerRadius = CornerRadius(moreH * 0.45f + 3f),
            style = Stroke(width = 3f),
        )
    }
    val moreLabel =
        textMeasurer.measure(
            "More",
            style = TextStyle(fontSize = 9.sp, color = cButtonLabel.copy(alpha = moreAlpha)),
        )
    scope.drawText(
        moreLabel,
        topLeft = Offset(moreCx - moreLabel.size.width / 2f, moreCy - moreLabel.size.height / 2f),
    )

    // Draw radial menus (as trigger circles with segment count label)
    layout.radialMenus.forEachIndexed { i, rm ->
        val cx = w * rm.xPct / 100f
        val cy = h * rm.yPct / 100f
        val trigR = baseScale * 0.04f * rm.sizeMult
        val wheelR = baseScale * 0.14f * rm.ringSizeMult
        val selected = selType == "radial" && selIdx == i
        val alpha = layout.globalOpacity * rm.opacity

        if (rm.presentation == SelectorPresentation.SCROLL_STRIP) {
            val halfSpan = w * rm.stripDragSpanWidthPct / 200f
            val vertical = rm.stripOrientation == SliderOrientation.VERTICAL
            val crossOffset = scrollStripRowCrossOffset(trigR, rm.stripRowOffset, rm.stripCardScale)
            val rowCx = cx + if (vertical) crossOffset else 0f
            val rowCy = cy + if (vertical) 0f else crossOffset
            val start = if (vertical) Offset(rowCx, cy - halfSpan) else Offset(cx - halfSpan, rowCy)
            val end = if (vertical) Offset(rowCx, cy + halfSpan) else Offset(cx + halfSpan, rowCy)
            scope.drawLine(
                color = cStickRing.copy(alpha = alpha * 0.7f),
                start = start,
                end = end,
                strokeWidth = 2f,
            )
            scope.drawLine(
                color = cStickRing.copy(alpha = alpha * 0.7f),
                start =
                    if (vertical) {
                        Offset(rowCx - trigR * 0.3f, cy - halfSpan)
                    } else {
                        Offset(
                            cx - halfSpan,
                            rowCy - trigR * 0.3f,
                        )
                    },
                end =
                    if (vertical) {
                        Offset(rowCx + trigR * 0.3f, cy - halfSpan)
                    } else {
                        Offset(
                            cx - halfSpan,
                            rowCy + trigR * 0.3f,
                        )
                    },
                strokeWidth = 2f,
            )
            scope.drawLine(
                color = cStickRing.copy(alpha = alpha * 0.7f),
                start =
                    if (vertical) {
                        Offset(rowCx - trigR * 0.3f, cy + halfSpan)
                    } else {
                        Offset(
                            cx + halfSpan,
                            rowCy - trigR * 0.3f,
                        )
                    },
                end =
                    if (vertical) {
                        Offset(rowCx + trigR * 0.3f, cy + halfSpan)
                    } else {
                        Offset(
                            cx + halfSpan,
                            rowCy + trigR * 0.3f,
                        )
                    },
                strokeWidth = 2f,
            )
        } else {
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
        }

        // Trigger circle
        scope.drawCircle(color = cButton.copy(alpha = alpha), radius = trigR, center = Offset(cx, cy))
        if (selected) {
            scope.drawCircle(
                color = cSelected,
                radius = if (rm.presentation == SelectorPresentation.SCROLL_STRIP) trigR + 4f else wheelR + 4f,
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

    // Draw diagnostics
    layout.diagnostics.forEachIndexed { i, d ->
        val cx = w * d.xPct / 100f
        val cy = h * d.yPct / 100f
        val boxW = baseScale * 0.12f * d.sizeMult
        val boxH = baseScale * 0.06f * d.sizeMult
        val selected = selType == "diagnostic" && selIdx == i
        val alpha = layout.globalOpacity * d.opacity
        val musicGeometry =
            if (d.type == DiagnosticType.MUSIC) {
                musicDiagnosticGeometry(cx, cy, targetSurfaceWidth, targetSurfaceHeight, d.sizeMult)
            } else {
                null
            }
        val previewLeft = musicGeometry?.buttonGroupLeft ?: (cx - boxW / 2f)
        val previewTop = musicGeometry?.buttonGroupTop ?: (cy - boxH / 2f)
        val previewWidth = musicGeometry?.let { it.buttonGroupRight - it.buttonGroupLeft } ?: boxW
        val previewHeight = musicGeometry?.let { it.buttonGroupBottom - it.buttonGroupTop } ?: boxH

        scope.drawRoundRect(
            color = Color(0x44000000).copy(alpha = alpha * 0.4f),
            topLeft = Offset(previewLeft, previewTop),
            size =
                androidx.compose.ui.geometry
                    .Size(previewWidth, previewHeight),
            cornerRadius = CornerRadius(4f, 4f),
        )
        val lineStyle = TextStyle(fontSize = 7.sp, color = cButtonLabel.copy(alpha = alpha))
        if (d.type == DiagnosticType.MUSIC) {
            // Music control preview -- match in-game TouchOverlayView formulas
            val geometry = checkNotNull(musicGeometry)
            val btnR = geometry.buttonRadius
            val btnY = geometry.buttonY
            val prevCX = geometry.previousButtonX
            val nextCX = geometry.nextButtonX
            scope.drawCircle(
                color = cButton.copy(alpha = alpha * 0.5f),
                radius = btnR,
                center = Offset(prevCX, btnY),
            )
            scope.drawCircle(
                color = cButton.copy(alpha = alpha * 0.5f),
                radius = btnR,
                center = Offset(nextCX, btnY),
            )
            scope.drawCircle(
                color = cButtonLabel.copy(alpha = alpha * 0.4f),
                radius = btnR,
                center = Offset(prevCX, btnY),
                style = Stroke(width = 3f),
            )
            scope.drawCircle(
                color = cButtonLabel.copy(alpha = alpha * 0.4f),
                radius = btnR,
                center = Offset(nextCX, btnY),
                style = Stroke(width = 3f),
            )
            val arrowStyle =
                TextStyle(
                    fontSize = with(scope) { geometry.arrowTextSize.toSp() },
                    color = cButtonLabel.copy(alpha = alpha),
                )
            val prevArrow = textMeasurer.measure(MUSIC_PREVIOUS_GLYPH, style = arrowStyle)
            scope.drawText(
                prevArrow,
                topLeft =
                    Offset(
                        prevCX - prevArrow.size.width / 2f,
                        btnY - prevArrow.size.height / 2f,
                    ),
            )
            val nextArrow = textMeasurer.measure(MUSIC_NEXT_GLYPH, style = arrowStyle)
            scope.drawText(
                nextArrow,
                topLeft =
                    Offset(
                        nextCX - nextArrow.size.width / 2f,
                        btnY - nextArrow.size.height / 2f,
                    ),
            )
            val trackLabel =
                textMeasurer.measure(
                    "\u266B Track Name",
                    style =
                        lineStyle.copy(
                            fontSize = with(scope) { geometry.labelTextSize.toSp() },
                        ),
                )
            scope.drawText(trackLabel, topLeft = Offset(geometry.labelX, btnY - trackLabel.size.height / 2f))
        } else if (d.type == DiagnosticType.SETTINGS) {
            // Settings grid icon: draw a small 3x3 dot grid
            val dotR = baseScale * 0.004f * d.sizeMult
            val dotGap = baseScale * 0.015f * d.sizeMult
            for (row in -1..1) {
                for (col in -1..1) {
                    scope.drawCircle(
                        color = cButtonLabel.copy(alpha = alpha),
                        radius = dotR,
                        center = Offset(cx + col * dotGap, cy + row * dotGap),
                    )
                }
            }
        } else {
            val l1 = textMeasurer.measure("Yaw:   ${(gyroYaw * 100).toInt()}%", style = lineStyle)
            val l2 = textMeasurer.measure("Roll:  ${(gyroPitch * 100).toInt()}%", style = lineStyle)
            val l3 = textMeasurer.measure("Pitch: ${(gyroRoll * 100).toInt()}%", style = lineStyle)
            val lineH = l1.size.height * 1.1f
            val startY = cy - lineH * 1.5f
            val textX = cx - boxW / 2 + 4f
            scope.drawText(l1, topLeft = Offset(textX, startY))
            scope.drawText(l2, topLeft = Offset(textX, startY + lineH))
            scope.drawText(l3, topLeft = Offset(textX, startY + lineH * 2))
        }
        if (selected) {
            scope.drawRoundRect(
                color = cSelected,
                topLeft = Offset(previewLeft - 2f, previewTop - 2f),
                size =
                    androidx.compose.ui.geometry
                        .Size(previewWidth + 4f, previewHeight + 4f),
                cornerRadius = CornerRadius(4f, 4f),
                style = Stroke(width = 3f),
            )
        }
    }

    // Draw axis regions
    layout.axisRegions.forEachIndexed { i, ar ->
        val z = ar.zone
        val left = w * z.leftPct / 100f
        val top = h * z.topPct / 100f
        val right = w * z.rightPct / 100f
        val bottom = h * z.bottomPct / 100f
        val selected = selType == "axisRegion" && selIdx == i
        val selectedEdge = selectedZoneEdge(selType, selIdx, SELECTED_TYPE_AXIS_REGION_EDGE, i)
        val alpha = layout.globalOpacity * ar.opacity

        scope.drawRect(
            color = Color(0x3388AAFF).copy(alpha = alpha),
            topLeft = Offset(left, top),
            size = ComposeSize(right - left, bottom - top),
        )
        scope.drawRect(
            color = cStickRing.copy(alpha = alpha),
            topLeft = Offset(left, top),
            size = ComposeSize(right - left, bottom - top),
            style = Stroke(width = 2f),
        )
        if (selected) {
            scope.drawRect(
                color = cSelected,
                topLeft = Offset(left - 2f, top - 2f),
                size = ComposeSize(right - left + 4f, bottom - top + 4f),
                style = Stroke(width = 3f),
            )
        } else if (selectedEdge != null) {
            scope.drawSelectedZoneEdge(left, top, right, bottom, selectedEdge)
        }
        val arLabel =
            textMeasurer.measure(
                ar.id.take(6),
                style = TextStyle(fontSize = 8.sp, color = cButtonLabel.copy(alpha = alpha)),
            )
        val cx = (left + right) / 2f
        val cy = (top + bottom) / 2f
        scope.drawText(arLabel, topLeft = Offset(cx - arLabel.size.width / 2f, cy - arLabel.size.height / 2f))
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
    allBounds.add(
        ControlBounds(
            w * layout.moreActions.xPct / 100f,
            h * layout.moreActions.yPct / 100f,
            max(w * 0.08f, min(w, h) * 0.08f) * layout.moreActions.sizeMult,
        ),
    )
    layout.radialMenus.forEach { rm ->
        allBounds.add(
            ControlBounds(
                w * rm.xPct / 100f,
                h * rm.yPct / 100f,
                baseScale * 0.05f * rm.sizeMult,
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

        "diagnostic" -> {
            val d = layout.diagnostics[index]
            Offset(canvasWidth * d.xPct / 100f, canvasHeight * d.yPct / 100f)
        }

        SELECTED_TYPE_MORE_ACTIONS -> {
            val more = layout.moreActions
            Offset(canvasWidth * more.xPct / 100f, canvasHeight * more.yPct / 100f)
        }

        else -> {
            Offset.Zero
        }
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
        "stick" -> {
            baseScale * 0.12f * layout.sticks[index].sizeMult
        }

        "button" -> {
            baseScale * 0.04f * layout.buttons[index].sizeMult
        }

        "radial" -> {
            baseScale * 0.14f * layout.radialMenus[index].sizeMult
        }

        "slider" -> {
            baseScale * 0.10f * layout.sliders[index].sizeMult
        }

        "diagnostic" -> {
            baseScale * 0.06f * layout.diagnostics[index].sizeMult
        }

        SELECTED_TYPE_MORE_ACTIONS -> {
            max(canvasWidth * 0.08f, min(canvasWidth, canvasHeight) * 0.08f) *
                layout.moreActions.sizeMult
        }

        else -> {
            0f
        }
    }
}

/** Returns (type, index) of the control at the given canvas offset, or null. */
private fun hitTest(
    layout: TouchLayout,
    offset: Offset,
    canvasWidth: Float,
    canvasHeight: Float,
    targetSurfaceWidth: Float = canvasWidth,
    targetSurfaceHeight: Float = canvasHeight,
): Pair<String, Int>? {
    val all = hitTestAll(layout, offset, canvasWidth, canvasHeight, targetSurfaceWidth, targetSurfaceHeight)
    return all.firstOrNull()
}

/** Returns all controls hit at the given offset, in priority order. */
internal fun hitTestAll(
    layout: TouchLayout,
    offset: Offset,
    canvasWidth: Float,
    canvasHeight: Float,
    targetSurfaceWidth: Float = canvasWidth,
    targetSurfaceHeight: Float = canvasHeight,
): List<Pair<String, Int>> {
    val baseScale = sqrt(canvasWidth * canvasHeight)
    val hits = mutableListOf<Pair<String, Int>>()
    val regionHits = mutableListOf<Pair<String, Int>>()
    val edgeHitSlopPx = defaultTouchEditorEdgeHitSlopPx(baseScale * 0.04f)

    // Check buttons first (smaller targets, should take priority)
    layout.buttons.forEachIndexed { i, btn ->
        val cx = canvasWidth * btn.xPct / 100f
        val cy = canvasHeight * btn.yPct / 100f
        val r = baseScale * 0.04f * btn.sizeMult
        val dist = sqrt((offset.x - cx) * (offset.x - cx) + (offset.y - cy) * (offset.y - cy))
        if (dist <= r * 1.3f) hits.add(Pair("button", i))
    }

    val more = layout.moreActions
    val moreBase = min(canvasWidth, canvasHeight)
    val moreH = canvasHeight * 0.05f * more.sizeMult
    val moreW = max(canvasWidth * 0.16f, moreBase * 0.16f) * more.sizeMult
    val moreCx = canvasWidth * more.xPct / 100f
    val moreCy = canvasHeight * more.yPct / 100f
    if (offset.x in (moreCx - moreW / 2f)..(moreCx + moreW / 2f) &&
        offset.y in (moreCy - moreH / 2f)..(moreCy + moreH / 2f)
    ) {
        hits.add(Pair(SELECTED_TYPE_MORE_ACTIONS, 0))
    }

    // Check editable stick-zone and axis-region edges before larger body hits.
    layout.sticks.forEachIndexed { i, stick ->
        if (!stick.floating && !stick.mouseMode) return@forEachIndexed
        addFloatingZoneEdgeHits(
            hits = regionHits,
            selectionType = SELECTED_TYPE_STICK_ZONE_EDGE,
            controlIndex = i,
            zone = stick.floatingZone,
            offset = offset,
            canvasWidth = canvasWidth,
            canvasHeight = canvasHeight,
            edgeHitSlopPx = edgeHitSlopPx,
        )
    }
    layout.axisRegions.forEachIndexed { i, ar ->
        addFloatingZoneEdgeHits(
            hits = regionHits,
            selectionType = SELECTED_TYPE_AXIS_REGION_EDGE,
            controlIndex = i,
            zone = ar.zone,
            offset = offset,
            canvasWidth = canvasWidth,
            canvasHeight = canvasHeight,
            edgeHitSlopPx = edgeHitSlopPx,
        )
    }

    // Check radial menus (before sticks, they have smaller triggers)
    layout.radialMenus.forEachIndexed { i, rm ->
        val cx = canvasWidth * rm.xPct / 100f
        val cy = canvasHeight * rm.yPct / 100f
        val r =
            if (rm.presentation == SelectorPresentation.SCROLL_STRIP) {
                baseScale * 0.04f * rm.sizeMult
            } else {
                baseScale * 0.14f * rm.sizeMult
            }
        val dist = sqrt((offset.x - cx) * (offset.x - cx) + (offset.y - cy) * (offset.y - cy))
        if (dist <= r) hits.add(Pair("radial", i))
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
        if (along <= trackLen + thumbR && across <= thumbR * 3) hits.add(Pair("slider", i))
    }

    // Check sticks
    layout.sticks.forEachIndexed { i, stick ->
        if (stick.mouseMode) {
            // Mouse mode: hit-test the floating zone rect
            val fz = stick.floatingZone
            val left = canvasWidth * fz.leftPct / 100f
            val top = canvasHeight * fz.topPct / 100f
            val right = canvasWidth * fz.rightPct / 100f
            val bottom = canvasHeight * fz.bottomPct / 100f
            if (offset.x in left..right && offset.y in top..bottom) regionHits.add(Pair("stick", i))
        } else {
            val cx = canvasWidth * stick.xPct / 100f
            val cy = canvasHeight * stick.yPct / 100f
            val r = baseScale * 0.12f * stick.sizeMult
            val dist = sqrt((offset.x - cx) * (offset.x - cx) + (offset.y - cy) * (offset.y - cy))
            if (dist <= r) regionHits.add(Pair("stick", i))
        }
    }

    // Check diagnostics
    layout.diagnostics.forEachIndexed { i, d ->
        val cx = canvasWidth * d.xPct / 100f
        val cy = canvasHeight * d.yPct / 100f
        val hit =
            if (d.type == DiagnosticType.MUSIC) {
                val geometry =
                    musicDiagnosticGeometry(cx, cy, targetSurfaceWidth, targetSurfaceHeight, d.sizeMult)
                offset.x in geometry.buttonGroupLeft..geometry.buttonGroupRight &&
                    offset.y in geometry.buttonGroupTop..geometry.buttonGroupBottom
            } else {
                val r = baseScale * 0.06f * d.sizeMult
                val dist = sqrt((offset.x - cx) * (offset.x - cx) + (offset.y - cy) * (offset.y - cy))
                dist <= r
            }
        if (hit) hits.add(Pair("diagnostic", i))
    }

    // Check axis regions
    layout.axisRegions.forEachIndexed { i, ar ->
        val z = ar.zone
        val left = canvasWidth * z.leftPct / 100f
        val top = canvasHeight * z.topPct / 100f
        val right = canvasWidth * z.rightPct / 100f
        val bottom = canvasHeight * z.bottomPct / 100f
        if (offset.x in left..right && offset.y in top..bottom) regionHits.add(Pair("axisRegion", i))
    }

    // Broad stick and drag regions sit behind discrete controls in the editor.
    return hits.ifEmpty { regionHits }
}

/** Returns a new layout with the specified control moved by (dxPct, dyPct). */
private fun moveControl(
    layout: TouchLayout,
    type: String,
    index: Int,
    dxPct: Float,
    dyPct: Float,
    canvasWidth: Float,
    canvasHeight: Float,
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
            val candidate = rm.copy(xPct = rm.xPct + dxPct, yPct = rm.yPct + dyPct)
            val clamped = clampRadialEditorPosition(candidate, canvasWidth, canvasHeight)
            layout.copy(
                radialMenus =
                    layout.radialMenus.toMutableList().also {
                        it[index] = clamped
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

        "diagnostic" -> {
            val d = layout.diagnostics[index]
            val newX = (d.xPct + dxPct).coerceIn(5f, 95f)
            val newY = (d.yPct + dyPct).coerceIn(5f, 95f)
            layout.copy(
                diagnostics =
                    layout.diagnostics.toMutableList().also {
                        it[index] = d.copy(xPct = newX, yPct = newY)
                    },
            )
        }

        SELECTED_TYPE_MORE_ACTIONS -> {
            val more = layout.moreActions
            val newX = (more.xPct + dxPct).coerceIn(2f, 98f)
            val newY = (more.yPct + dyPct).coerceIn(2f, 98f)
            layout.copy(moreActions = more.copy(xPct = newX, yPct = newY))
        }

        "axisRegion" -> {
            val ar = layout.axisRegions[index]
            val z = ar.zone
            val w = z.rightPct - z.leftPct
            val h = z.bottomPct - z.topPct
            val newLeft = (z.leftPct + dxPct).coerceIn(0f, 100f - w)
            val newTop = (z.topPct + dyPct).coerceIn(0f, 100f - h)
            layout.copy(
                axisRegions =
                    layout.axisRegions.toMutableList().also {
                        it[index] =
                            ar.copy(
                                zone = FloatingZone(newLeft, newTop, newLeft + w, newTop + h),
                            )
                    },
            )
        }

        SELECTED_TYPE_STICK_ZONE_EDGE -> {
            val (stickIndex, edge) = decodeFloatingZoneEdgeSelection(index)
            val stick = layout.sticks[stickIndex]
            layout.copy(
                sticks =
                    layout.sticks.toMutableList().also {
                        it[stickIndex] =
                            stick.copy(
                                floatingZone = resizeFloatingZone(stick.floatingZone, edge, dxPct, dyPct),
                            )
                    },
            )
        }

        SELECTED_TYPE_AXIS_REGION_EDGE -> {
            val (regionIndex, edge) = decodeFloatingZoneEdgeSelection(index)
            val region = layout.axisRegions[regionIndex]
            layout.copy(
                axisRegions =
                    layout.axisRegions.toMutableList().also {
                        it[regionIndex] =
                            region.copy(
                                zone = resizeFloatingZone(region.zone, edge, dxPct, dyPct),
                            )
                    },
            )
        }

        else -> {
            layout
        }
    }

private fun clampRadialEditorPosition(
    radial: RadialMenuControl,
    canvasWidth: Float,
    canvasHeight: Float,
): RadialMenuControl {
    if (radial.presentation != SelectorPresentation.SCROLL_STRIP) {
        return radial.copy(xPct = radial.xPct.coerceIn(5f, 95f), yPct = radial.yPct.coerceIn(5f, 95f))
    }
    val vertical = radial.stripOrientation == SliderOrientation.VERTICAL
    return if (vertical) {
        radial.copy(
            xPct = radial.xPct.coerceIn(5f, 95f),
            yPct =
                clampScrollStripCenterPct(
                    radial.yPct,
                    radial.stripDragSpanWidthPct,
                    canvasWidth,
                    canvasHeight,
                    vertical = true,
                ),
        )
    } else {
        radial.copy(
            xPct =
                clampScrollStripCenterPct(
                    radial.xPct,
                    radial.stripDragSpanWidthPct,
                    canvasWidth,
                    canvasHeight,
                    vertical = false,
                ),
            yPct = radial.yPct.coerceIn(5f, 95f),
        )
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Properties panels
// ═════════════════════════════════════════════════════════════════════════════

private fun enabledDefaultExtremeAction(stick: AnalogStickControl): StickExtremeAction {
    val existing = stick.extremeActions.firstOrNull() ?: StickExtremeAction()
    return normalizeStickExtremeAction(existing.copy(enabled = true))
}

private fun stickWithExtremeAction(
    stick: AnalogStickControl,
    action: StickExtremeAction,
): AnalogStickControl =
    stick.copy(
        extremeActions =
            if (action.enabled) {
                listOf(normalizeStickExtremeAction(action))
            } else {
                emptyList()
            },
    )

@Composable
private fun StickPropertiesPanel(
    stick: AnalogStickControl,
    gameVariant: String = "d2",
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

    val xLabel = TouchBindings.AXIS_LABELS[stick.axisX] ?: "Axis ${stick.axisX}"
    val yLabel = TouchBindings.AXIS_LABELS[stick.axisY] ?: "Axis ${stick.axisY}"

    // Axis bindings or button mode bindings
    LabeledToggle("Button Mode", stick.buttonMode) {
        onUpdate(stick.copy(buttonMode = it))
    }
    if (stick.buttonMode) {
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ButtonBindingPicker("$xLabel Neg", stick.negXBinding, Modifier.weight(1f), gameVariant) {
                onUpdate(stick.copy(negXBinding = it))
            }
            ButtonBindingPicker("$xLabel Pos", stick.posXBinding, Modifier.weight(1f), gameVariant) {
                onUpdate(stick.copy(posXBinding = it))
            }
        }
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ButtonBindingPicker("$yLabel Neg", stick.negYBinding, Modifier.weight(1f), gameVariant) {
                onUpdate(stick.copy(negYBinding = it))
            }
            ButtonBindingPicker("$yLabel Pos", stick.posYBinding, Modifier.weight(1f), gameVariant) {
                onUpdate(stick.copy(posYBinding = it))
            }
        }
    } else {
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            AxisPicker(xLabel, stick.axisX, Modifier.weight(1f)) {
                onUpdate(stick.copy(axisX = it))
            }
            AxisPicker(yLabel, stick.axisY, Modifier.weight(1f)) {
                onUpdate(stick.copy(axisY = it))
            }
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
            "Sens: $xLabel",
            stick.sensitivityX,
            TouchBindings.MIN_SENSITIVITY,
            TouchBindings.MAX_SENSITIVITY,
            Modifier.weight(1f),
        ) {
            onUpdate(stick.copy(sensitivityX = it))
        }
        LabeledSlider(
            "Sens: $yLabel",
            stick.sensitivityY,
            TouchBindings.MIN_SENSITIVITY,
            TouchBindings.MAX_SENSITIVITY,
            Modifier.weight(1f),
        ) {
            onUpdate(stick.copy(sensitivityY = it))
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
        LabeledToggle("Floating", stick.floating) {
            if (it && !stick.floating) {
                // Auto-compute a zone around the stick's current position
                onUpdate(
                    stick.copy(
                        floating = true,
                        floatingZone =
                            FloatingZone(
                                leftPct = (stick.xPct - 20f).coerceIn(0f, 100f),
                                topPct = (stick.yPct - 30f).coerceIn(0f, 100f),
                                rightPct = (stick.xPct + 20f).coerceIn(0f, 100f),
                                bottomPct = (stick.yPct + 30f).coerceIn(0f, 100f),
                            ),
                    ),
                )
            } else {
                onUpdate(stick.copy(floating = it))
            }
        }
        LabeledToggle("Mouse Mode", stick.mouseMode) {
            if (it && !stick.mouseMode) {
                // Auto-compute a zone if no floating zone set yet
                val fz =
                    if (!stick.floating) {
                        FloatingZone(
                            leftPct = (stick.xPct - 20f).coerceIn(0f, 100f),
                            topPct = (stick.yPct - 30f).coerceIn(0f, 100f),
                            rightPct = (stick.xPct + 20f).coerceIn(0f, 100f),
                            bottomPct = (stick.yPct + 30f).coerceIn(0f, 100f),
                        )
                    } else {
                        stick.floatingZone
                    }
                onUpdate(stick.copy(mouseMode = true, floatingZone = fz))
            } else {
                onUpdate(stick.copy(mouseMode = it))
            }
        }
        LabeledToggle("Inv: $xLabel", stick.invertX) { onUpdate(stick.copy(invertX = it)) }
        LabeledToggle("Inv: $yLabel", stick.invertY) { onUpdate(stick.copy(invertY = it)) }
        LabeledToggle("Haptic", stick.hapticFeedback) { onUpdate(stick.copy(hapticFeedback = it)) }
    }
    if (stick.mouseMode) {
        LabeledToggle("Edge Continuous Movement", stick.mouseEdgeContinuousMovement) {
            onUpdate(stick.copy(mouseEdgeContinuousMovement = it))
        }
        if (stick.mouseEdgeContinuousMovement) {
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                LabeledSlider(
                    "Edge Size %",
                    stick.mouseEdgeRegionPct,
                    TouchBindings.MIN_MOUSE_EDGE_REGION_PCT,
                    TouchBindings.MAX_MOUSE_EDGE_REGION_PCT,
                    Modifier.weight(1f),
                ) {
                    onUpdate(stick.copy(mouseEdgeRegionPct = it))
                }
                LabeledSlider(
                    "Edge Max Rate %",
                    stick.mouseEdgeMaxRatePct,
                    TouchBindings.MIN_MOUSE_EDGE_MAX_RATE_PCT,
                    TouchBindings.MAX_MOUSE_EDGE_MAX_RATE_PCT,
                    Modifier.weight(1f),
                ) {
                    onUpdate(stick.copy(mouseEdgeMaxRatePct = it))
                }
            }
        }
    }

    // Extreme action (available for all stick modes)
    val extremeAction = stick.extremeActions.firstOrNull()
    val extremeEnabled = extremeAction?.enabled == true
    LabeledToggle("Extreme Action", extremeEnabled) { enabled ->
        onUpdate(
            if (enabled) {
                stickWithExtremeAction(stick, enabledDefaultExtremeAction(stick))
            } else {
                stick.copy(extremeActions = emptyList())
            },
        )
    }
    if (extremeEnabled) {
        val action = normalizeStickExtremeAction(extremeAction)
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ButtonBindingPicker("Extreme Action Binding", action.binding, Modifier.weight(1f), gameVariant) {
                onUpdate(stickWithExtremeAction(stick, action.copy(binding = it)))
            }
            StickExtremeEnumPicker(
                label = "Extreme Action Axis",
                current = action.axis,
                entries =
                    listOf(
                        StickExtremeAxis.X to "X",
                        StickExtremeAxis.Y to "Y",
                    ),
                modifier = Modifier.weight(1f),
            ) {
                onUpdate(stickWithExtremeAction(stick, action.copy(axis = it)))
            }
        }
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            StickExtremeEnumPicker(
                label = "Extreme Action Direction",
                current = action.direction,
                entries =
                    listOf(
                        StickExtremeDirection.NEGATIVE to "Negative",
                        StickExtremeDirection.POSITIVE to "Positive",
                    ),
                modifier = Modifier.weight(1f),
            ) {
                onUpdate(stickWithExtremeAction(stick, action.copy(direction = it)))
            }
            StickExtremeEnumPicker(
                label = "Extreme Action Mode",
                current = action.mode,
                entries =
                    listOf(
                        StickExtremeActionMode.HOLD to "Hold",
                        StickExtremeActionMode.PULSE_ON_ENTER to "Tap once",
                    ),
                modifier = Modifier.weight(1f),
            ) {
                onUpdate(stickWithExtremeAction(stick, action.copy(mode = it)))
            }
        }
        LabeledSlider(
            "Extreme Action Threshold",
            action.threshold,
            TouchBindings.MIN_STICK_EXTREME_THRESHOLD,
            TouchBindings.MAX_STICK_EXTREME_THRESHOLD,
            Modifier.fillMaxWidth(),
        ) {
            onUpdate(
                stickWithExtremeAction(
                    stick,
                    action.copy(
                        threshold = it,
                        releaseThreshold =
                            (
                                it - TouchBindings.DEFAULT_STICK_EXTREME_THRESHOLD +
                                    TouchBindings.DEFAULT_STICK_EXTREME_RELEASE_THRESHOLD
                            ),
                    ),
                ),
            )
        }
    }

    // Double-tap action (available for all stick modes)
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        DoubleTapBindingPicker(
            "Double-Tap Action",
            stick.doubleTapBinding,
            Modifier.weight(1f),
            gameVariant,
        ) {
            onUpdate(stick.copy(doubleTapBinding = it))
        }
    }

    // Double-tap mode (shown when a double-tap binding is set)
    if (stick.doubleTapBinding >= 0) {
        Text("Double-Tap Mode", color = Color.Gray, fontSize = 12.sp)
        val modes = DoubleTapMode.entries
        val modeLabels =
            mapOf(
                DoubleTapMode.SINGLE_FIRE to "Plain (1 per complete double-tap)",
                DoubleTapMode.REPEAT_FIRE to "Repeat (each tap 2..N = fire)",
                DoubleTapMode.LATCH_DOUBLE to "Latch (double-tap toggles)",
                DoubleTapMode.LATCH_SINGLE to "Latch (double-tap on, single tap off)",
                DoubleTapMode.HOLD_FIRE to "Hold (double-tap and hold starts, lift stops)",
            )
        modes.forEach { m ->
            Row(verticalAlignment = Alignment.CenterVertically) {
                RadioButton(
                    selected = stick.doubleTapMode == m,
                    onClick = { onUpdate(stick.copy(doubleTapMode = m)) },
                    modifier = Modifier.size(20.dp),
                )
                Spacer(Modifier.width(4.dp))
                Text(modeLabels[m] ?: m.name, fontSize = 12.sp, color = Color.White)
            }
        }
    }

    // Floating zone bounds (shown when floating or mouse mode is enabled)
    if (stick.floating || stick.mouseMode) {
        val fz = stick.floatingZone
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            LabeledSlider("Left %", fz.leftPct, 0f, 100f, Modifier.weight(1f)) {
                onUpdate(stick.copy(floatingZone = setFloatingZoneEdge(fz, FloatingZoneEdge.LEFT, it)))
            }
            LabeledSlider("Right %", fz.rightPct, 0f, 100f, Modifier.weight(1f)) {
                onUpdate(stick.copy(floatingZone = setFloatingZoneEdge(fz, FloatingZoneEdge.RIGHT, it)))
            }
        }
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            LabeledSlider("Top %", fz.topPct, 0f, 100f, Modifier.weight(1f)) {
                onUpdate(stick.copy(floatingZone = setFloatingZoneEdge(fz, FloatingZoneEdge.TOP, it)))
            }
            LabeledSlider("Bottom %", fz.bottomPct, 0f, 100f, Modifier.weight(1f)) {
                onUpdate(stick.copy(floatingZone = setFloatingZoneEdge(fz, FloatingZoneEdge.BOTTOM, it)))
            }
        }
    }
}

@Composable
private fun <T> StickExtremeEnumPicker(
    label: String,
    current: T,
    entries: List<Pair<T, String>>,
    modifier: Modifier = Modifier,
    onChange: (T) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    val currentLabel = entries.firstOrNull { it.first == current }?.second ?: current.toString()

    Column(modifier = modifier) {
        Text(label, color = Color.Gray, fontSize = 11.sp)
        Box {
            TextButton(onClick = { expanded = true }) {
                Text(currentLabel, fontSize = 11.sp)
            }
            DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                entries.forEach { (value, name) ->
                    DropdownMenuItem(
                        text = { Text(name, fontSize = 12.sp) },
                        onClick = {
                            onChange(value)
                            expanded = false
                        },
                    )
                }
            }
        }
    }
}

@Composable
private fun ButtonPropertiesPanel(
    button: ButtonControl,
    gameVariant: String = "d2",
    onUpdate: (ButtonControl) -> Unit,
    onDelete: () -> Unit,
) {
    val longPressBinding =
        if (button.longPressBinding >= 0) {
            button.longPressBinding
        } else {
            TouchBindings.META_GYRO_TOGGLE
        }

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
        ButtonBindingPicker("Binding", button.binding, Modifier.weight(1f), gameVariant) {
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
        LabeledToggle("Latch", button.toggle) { onUpdate(button.copy(toggle = it)) }
        LabeledToggle("Haptic", button.hapticFeedback) { onUpdate(button.copy(hapticFeedback = it)) }
    }

    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
        LabeledToggle("Long Press", button.longPressEnabled) { enabled ->
            onUpdate(
                button.copy(
                    longPressEnabled = enabled,
                    longPressBinding =
                        if (enabled && button.longPressBinding < 0) {
                            TouchBindings.META_GYRO_TOGGLE
                        } else {
                            button.longPressBinding
                        },
                    longPressDurationMs = normalizeButtonLongPressDurationMs(button.longPressDurationMs),
                ),
            )
        }
    }

    if (button.longPressEnabled) {
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ButtonBindingPicker("Long-Press Binding", longPressBinding, Modifier.weight(1f), gameVariant) {
                onUpdate(button.copy(longPressBinding = it))
            }
            LabeledIntSlider(
                "Hold ms",
                button.longPressDurationMs,
                TouchBindings.MIN_LONG_PRESS_DURATION_MS,
                TouchBindings.MAX_LONG_PRESS_DURATION_MS,
                Modifier.weight(1f),
            ) {
                onUpdate(button.copy(longPressDurationMs = it))
            }
        }
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
    gameVariant: String = "d2",
    canvasWidth: Float,
    canvasHeight: Float,
    onUpdate: (RadialMenuControl) -> Unit,
    onDelete: () -> Unit,
) {
    fun updateRadial(updated: RadialMenuControl) {
        onUpdate(clampRadialEditorPosition(updated, canvasWidth, canvasHeight))
    }
    val isPreset = radial.id in TouchBindings.RADIAL_PRESET_IDS
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

    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text("Layout: ", color = Color.Gray, fontSize = 12.sp)
        SelectorPresentation.entries.forEach { presentation ->
            TextButton(onClick = { updateRadial(radial.copy(presentation = presentation)) }) {
                Text(
                    if (presentation == SelectorPresentation.WHEEL) "wheel" else "scroll strip",
                    fontSize = 11.sp,
                    color = if (radial.presentation == presentation) cSelected else Color.Gray,
                )
            }
        }
    }

    if (radial.presentation == SelectorPresentation.SCROLL_STRIP) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text("Drag: ", color = Color.Gray, fontSize = 12.sp)
            SliderOrientation.entries.forEach { orientation ->
                TextButton(onClick = { updateRadial(radial.copy(stripOrientation = orientation)) }) {
                    Text(
                        orientation.name.lowercase(),
                        fontSize = 11.sp,
                        color = if (radial.stripOrientation == orientation) cSelected else Color.Gray,
                    )
                }
            }
        }
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text("Options row: ", color = Color.Gray, fontSize = 12.sp)
            ScrollStripRowOffset.entries.forEach { rowOffset ->
                TextButton(onClick = { updateRadial(radial.copy(stripRowOffset = rowOffset)) }) {
                    Text(
                        rowOffset.symbol,
                        fontSize = 14.sp,
                        color = if (radial.stripRowOffset == rowOffset) cSelected else Color.Gray,
                    )
                }
            }
        }
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            LabeledSlider(
                "Drag area %W",
                radial.stripDragSpanWidthPct,
                5f,
                80f,
                Modifier.weight(1f),
            ) {
                updateRadial(radial.copy(stripDragSpanWidthPct = it))
            }
            LabeledSlider(
                "Center zoom",
                radial.stripSelectedScale,
                1f,
                3f,
                Modifier.weight(1f),
            ) {
                updateRadial(radial.copy(stripSelectedScale = it))
            }
        }
        LabeledSlider(
            "Text/button scale",
            radial.stripCardScale,
            MIN_SCROLL_STRIP_CARD_SCALE,
            MAX_SCROLL_STRIP_CARD_SCALE,
            Modifier.fillMaxWidth(),
        ) {
            updateRadial(radial.copy(stripCardScale = it))
        }
        LabeledSlider(
            "Label angle",
            radial.stripLabelAngleDeg,
            -90f,
            90f,
            Modifier.fillMaxWidth(),
        ) {
            updateRadial(radial.copy(stripLabelAngleDeg = it))
        }
    }

    // Preset selector
    var presetExpanded by remember { mutableStateOf(false) }
    val presetOptions =
        buildList {
            add("Custom" to "")
            for ((id, label) in TouchBindings.RADIAL_PRESET_LABELS) {
                if (id == "Guide" && gameVariant != "d2") continue
                add(label to id)
            }
        }
    val currentPresetLabel =
        if (isPreset) {
            TouchBindings.RADIAL_PRESET_LABELS[radial.id] ?: radial.id
        } else {
            "Custom"
        }
    Box {
        TextButton(onClick = { presetExpanded = true }) {
            Text("Preset: $currentPresetLabel", fontSize = 12.sp)
        }
        DropdownMenu(expanded = presetExpanded, onDismissRequest = { presetExpanded = false }) {
            presetOptions.forEach { (label, id) ->
                DropdownMenuItem(
                    text = { Text(label) },
                    onClick = {
                        presetExpanded = false
                        if (id.isEmpty()) return@DropdownMenuItem // already custom
                        val segs = TouchBindings.RADIAL_PRESET_SEGMENTS[id] ?: return@DropdownMenuItem
                        val center = TouchBindings.RADIAL_PRESET_CENTER[id]
                        onUpdate(
                            radial.copy(
                                id = id,
                                segments = segs,
                                centerLabel = center?.first ?: "",
                                centerBinding = center?.second ?: -1,
                            ),
                        )
                    },
                )
            }
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
            "Button",
            radial.sizeMult,
            TouchBindings.MIN_SIZE,
            TouchBindings.MAX_SIZE,
            Modifier.weight(1f),
        ) {
            updateRadial(radial.copy(sizeMult = it))
        }
        LabeledSlider(
            "Ring",
            radial.ringSizeMult,
            TouchBindings.MIN_SIZE,
            TouchBindings.MAX_SIZE,
            Modifier.weight(1f),
        ) {
            updateRadial(radial.copy(ringSizeMult = it))
        }
    }
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
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

    // Segments
    if (isPreset) {
        Text(
            "Preset segments (not editable): " +
                radial.segments.joinToString(", ") { it.label },
            color = Color.Gray,
            fontSize = 11.sp,
        )
    } else {
        Text("Segments", color = Color.Gray, fontSize = 12.sp)
        radial.segments.forEachIndexed { idx, seg ->
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                LabelEditor("", seg.label, Modifier.weight(1f)) { newLabel ->
                    val newSegs = radial.segments.toMutableList()
                    newSegs[idx] = seg.copy(label = newLabel)
                    onUpdate(radial.copy(segments = newSegs))
                }
                SegmentBindingPicker(
                    current = seg.binding,
                    gameVariant = gameVariant,
                    modifier = Modifier.weight(1f),
                ) { newBinding ->
                    val newSegs = radial.segments.toMutableList()
                    val bindingLabel =
                        TouchBindings.ALL_BUTTON_LABELS[newBinding]
                            ?: "Button $newBinding"
                    newSegs[idx] =
                        seg.copy(
                            binding = newBinding,
                            label = bindingLabel,
                            bindingType = "action",
                        )
                    onUpdate(radial.copy(segments = newSegs))
                }
                IconButton(
                    onClick = {
                        if (radial.segments.size > 1) {
                            val newSegs = radial.segments.toMutableList()
                            newSegs.removeAt(idx)
                            onUpdate(radial.copy(segments = newSegs))
                        }
                    },
                    modifier = Modifier.size(24.dp),
                ) {
                    Icon(Icons.Default.Delete, "Remove", tint = Color(0xFFEF5350))
                }
            }
        }
        if (radial.segments.size < 12) {
            TextButton(onClick = {
                val newSeg =
                    RadialSegment(
                        label = "New",
                        binding = TouchBindings.BTN_FIRE_PRIMARY,
                        bindingType = "action",
                    )
                onUpdate(radial.copy(segments = radial.segments + newSeg))
            }) {
                Text("+ Add Segment", fontSize = 11.sp)
            }
        }
    }

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

@Composable
private fun DiagnosticPropertiesPanel(
    diag: DiagnosticControl,
    onUpdate: (DiagnosticControl) -> Unit,
    onDelete: () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text("Info: ${diag.type.label}", color = Color.White, fontSize = 14.sp)
        IconButton(onClick = onDelete) {
            Icon(Icons.Default.Delete, "Delete", tint = Color(0xFFEF5350))
        }
    }

    // Type selector
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        DiagnosticType.entries.forEach { dt ->
            Row(verticalAlignment = Alignment.CenterVertically) {
                RadioButton(
                    selected = diag.type == dt,
                    onClick = { onUpdate(diag.copy(type = dt)) },
                )
                Text(dt.label, color = Color.White, fontSize = 12.sp)
            }
        }
    }

    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        LabeledSlider(
            "Size",
            diag.sizeMult,
            TouchBindings.MIN_SIZE,
            TouchBindings.MAX_SIZE,
            Modifier.weight(1f),
        ) {
            onUpdate(diag.copy(sizeMult = it))
        }
        LabeledSlider(
            "Opacity",
            diag.opacity,
            TouchBindings.MIN_OPACITY,
            TouchBindings.MAX_OPACITY,
            Modifier.weight(1f),
        ) {
            onUpdate(diag.copy(opacity = it))
        }
    }
}

@Composable
private fun AxisRegionPropertiesPanel(
    region: AxisRegionControl,
    onUpdate: (AxisRegionControl) -> Unit,
    onDelete: () -> Unit,
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text("Axis Region: ${region.id}", color = Color.White, fontSize = 14.sp)
        IconButton(onClick = onDelete) {
            Icon(Icons.Default.Delete, "Delete", tint = Color(0xFFEF5350))
        }
    }

    // Axis & orientation
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        AxisPicker("Axis", region.axis, Modifier.weight(1f)) {
            onUpdate(region.copy(axis = it))
        }
        Column(Modifier.weight(1f)) {
            Text("Orientation", color = Color.Gray, fontSize = 11.sp)
            Row {
                SliderOrientation.entries.forEach { ori ->
                    TextButton(onClick = { onUpdate(region.copy(orientation = ori)) }) {
                        Text(
                            ori.name.lowercase(),
                            fontSize = 11.sp,
                            color = if (region.orientation == ori) cSelected else Color.Gray,
                        )
                    }
                }
            }
        }
    }

    // Zone edges
    Text("Zone (% of screen)", color = Color.Gray, fontSize = 11.sp)
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        LabeledSlider("Left", region.zone.leftPct, 0f, 100f, Modifier.weight(1f)) {
            onUpdate(region.copy(zone = setFloatingZoneEdge(region.zone, FloatingZoneEdge.LEFT, it)))
        }
        LabeledSlider("Top", region.zone.topPct, 0f, 100f, Modifier.weight(1f)) {
            onUpdate(region.copy(zone = setFloatingZoneEdge(region.zone, FloatingZoneEdge.TOP, it)))
        }
    }
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        LabeledSlider("Right", region.zone.rightPct, 0f, 100f, Modifier.weight(1f)) {
            onUpdate(region.copy(zone = setFloatingZoneEdge(region.zone, FloatingZoneEdge.RIGHT, it)))
        }
        LabeledSlider("Bottom", region.zone.bottomPct, 0f, 100f, Modifier.weight(1f)) {
            onUpdate(region.copy(zone = setFloatingZoneEdge(region.zone, FloatingZoneEdge.BOTTOM, it)))
        }
    }

    // Sensitivity, opacity, curve
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        LabeledSlider(
            "Sensitivity",
            region.sensitivity,
            TouchBindings.MIN_SENSITIVITY,
            TouchBindings.MAX_SENSITIVITY,
            Modifier.weight(1f),
        ) {
            onUpdate(region.copy(sensitivity = it))
        }
        LabeledSlider(
            "Opacity",
            region.opacity,
            TouchBindings.MIN_OPACITY,
            TouchBindings.MAX_OPACITY,
            Modifier.weight(1f),
        ) {
            onUpdate(region.copy(opacity = it))
        }
    }
    LabeledToggle("Invert", region.invert) { onUpdate(region.copy(invert = it)) }
    CurvePicker("Curve", region.responseCurve, Modifier.fillMaxWidth()) {
        onUpdate(region.copy(responseCurve = it))
    }
}

@Composable
private fun MoreActionsPropertiesPanel(
    control: MoreActionsControl,
    onUpdate: (MoreActionsControl) -> Unit,
) {
    Text("More Actions (anything unassigned)", color = Color.White, fontSize = 14.sp)

    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        LabeledSlider(
            "Size",
            control.sizeMult,
            TouchBindings.MIN_SIZE,
            TouchBindings.MAX_SIZE,
            Modifier.weight(1f),
        ) {
            onUpdate(control.copy(sizeMult = it))
        }
        LabeledSlider(
            "Opacity",
            control.opacity,
            TouchBindings.MIN_OPACITY,
            TouchBindings.MAX_OPACITY,
            Modifier.weight(1f),
        ) {
            onUpdate(control.copy(opacity = it))
        }
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
            modifier = Modifier.height(28.dp).tvFocusBorder(),
        )
    }
}

@Composable
private fun LabeledIntSlider(
    label: String,
    value: Int,
    min: Int,
    max: Int,
    modifier: Modifier = Modifier,
    onChange: (Int) -> Unit,
) {
    Column(modifier = modifier) {
        Text("$label: $value", color = Color.Gray, fontSize = 11.sp)
        Slider(
            value = value.toFloat(),
            onValueChange = { onChange(it.roundToInt().coerceIn(min, max)) },
            valueRange = min.toFloat()..max.toFloat(),
            modifier = Modifier.height(28.dp).tvFocusBorder(),
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
private fun SegmentBindingPicker(
    current: Int,
    gameVariant: String = "d2",
    modifier: Modifier = Modifier,
    onChange: (Int) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    var showExtra by remember { mutableStateOf(false) }
    val currentLabel = TouchBindings.ALL_BUTTON_LABELS[current] ?: "Binding $current"
    val isD1 = gameVariant == "d1"
    val entries =
        if (showExtra) TouchBindings.META_BUTTON_LABELS else TouchBindings.BUTTON_LABELS

    Column(modifier = modifier) {
        TextButton(onClick = { expanded = true }, modifier = Modifier.height(28.dp)) {
            Text(currentLabel, fontSize = 10.sp)
        }
        if (expanded) {
            AlertDialog(
                onDismissRequest = { expanded = false },
                title = { Text("Segment Binding", fontSize = 14.sp) },
                text = {
                    val scrollState = rememberScrollState()
                    Box(Modifier.heightIn(max = 300.dp)) {
                        Column(Modifier.verticalScroll(scrollState)) {
                            OutlinedButton(
                                onClick = { showExtra = !showExtra },
                                border =
                                    androidx.compose.foundation.BorderStroke(
                                        2.dp,
                                        Color(0xFF2196F3),
                                    ),
                                shape =
                                    androidx.compose.foundation.shape
                                        .RoundedCornerShape(8.dp),
                            ) {
                                Text(
                                    if (showExtra) "view standard buttons" else "view extra buttons",
                                    fontSize = 12.sp,
                                )
                            }
                            entries.forEach { (idx, name) ->
                                val isD2Only =
                                    isD1 &&
                                        (
                                            idx in TouchBindings.D2_ONLY_BUTTONS ||
                                                idx in TouchBindings.D2_ONLY_META_ACTIONS
                                        )
                                val disabled = isD2Only
                                val suffix = if (isD2Only) " (D2 only)" else ""
                                Text(
                                    "$name$suffix",
                                    modifier =
                                        Modifier
                                            .fillMaxWidth()
                                            .then(
                                                if (disabled) {
                                                    Modifier
                                                } else {
                                                    Modifier.clickable {
                                                        onChange(idx)
                                                        expanded = false
                                                    }
                                                },
                                            ).padding(vertical = 8.dp, horizontal = 4.dp),
                                    fontSize = 12.sp,
                                    fontStyle = if (disabled) FontStyle.Italic else FontStyle.Normal,
                                    color =
                                        if (disabled) {
                                            Color(0xFF999999)
                                        } else if (idx == current) {
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
private fun ButtonBindingPicker(
    label: String,
    current: Int,
    modifier: Modifier = Modifier,
    gameVariant: String = "d2",
    onChange: (Int) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    var showExtra by remember { mutableStateOf(false) }
    val currentLabel = TouchBindings.ALL_BUTTON_LABELS[current] ?: "Button $current"
    val isD1 = gameVariant == "d1"
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
                                OutlinedButton(
                                    onClick = { showExtra = !showExtra },
                                    border =
                                        androidx.compose.foundation.BorderStroke(
                                            2.dp,
                                            Color(0xFF2196F3),
                                        ),
                                    shape =
                                        androidx.compose.foundation.shape
                                            .RoundedCornerShape(8.dp),
                                ) {
                                    Text(
                                        if (showExtra) "view standard buttons" else "view extra buttons",
                                        fontSize = 12.sp,
                                    )
                                }
                            }
                            entries.forEach { (idx, name) ->
                                val isD2Only =
                                    isD1 &&
                                        (
                                            idx in TouchBindings.D2_ONLY_BUTTONS ||
                                                idx in TouchBindings.D2_ONLY_META_ACTIONS
                                        )
                                val disabled = isD2Only
                                val suffix = if (isD2Only) " (D2 only)" else ""
                                Text(
                                    "$name$suffix",
                                    modifier =
                                        Modifier
                                            .fillMaxWidth()
                                            .then(
                                                if (disabled) {
                                                    Modifier
                                                } else {
                                                    Modifier.clickable {
                                                        onChange(idx)
                                                        expanded = false
                                                    }
                                                },
                                            ).padding(vertical = 8.dp, horizontal = 4.dp),
                                    fontSize = 12.sp,
                                    fontStyle = if (disabled) FontStyle.Italic else FontStyle.Normal,
                                    color =
                                        if (disabled) {
                                            Color(0xFF999999)
                                        } else if (idx == current) {
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

/** Like ButtonBindingPicker but with a "None" option (returns -1). */
@Composable
private fun DoubleTapBindingPicker(
    label: String,
    current: Int,
    modifier: Modifier = Modifier,
    gameVariant: String = "d2",
    onChange: (Int) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    val currentLabel = if (current < 0) "None" else (TouchBindings.ALL_BUTTON_LABELS[current] ?: "Button $current")
    val isD1 = gameVariant == "d1"
    val entries = TouchBindings.BUTTON_LABELS

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
                            Text(
                                "None",
                                modifier =
                                    Modifier
                                        .fillMaxWidth()
                                        .clickable {
                                            onChange(-1)
                                            expanded = false
                                        }.padding(vertical = 8.dp, horizontal = 4.dp),
                                fontSize = 12.sp,
                                color =
                                    if (current <
                                        0
                                    ) {
                                        MaterialTheme.colorScheme.primary
                                    } else {
                                        MaterialTheme.colorScheme.onSurface
                                    },
                            )
                            entries.forEach { (idx, name) ->
                                val isD2Only = isD1 && idx in TouchBindings.D2_ONLY_BUTTONS
                                val disabled = isD2Only
                                val suffix = if (isD2Only) " (D2 only)" else ""
                                Text(
                                    "$name$suffix",
                                    modifier =
                                        Modifier
                                            .fillMaxWidth()
                                            .then(
                                                if (disabled) {
                                                    Modifier
                                                } else {
                                                    Modifier.clickable {
                                                        onChange(idx)
                                                        expanded =
                                                            false
                                                    }
                                                },
                                            ).padding(vertical = 8.dp, horizontal = 4.dp),
                                    fontSize = 12.sp,
                                    fontStyle = if (disabled) FontStyle.Italic else FontStyle.Normal,
                                    color =
                                        if (disabled) {
                                            Color(0xFF999999)
                                        } else if (idx == current) {
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
            modifier = Modifier.height(48.dp).fillMaxWidth().dpadTextFieldNavigation(),
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
    val context = LocalContext.current
    val presets = remember(context) { TouchLayoutRepository.allPresets(context) }
    val firstPresetFocus = remember { FocusRequester() }
    val cancelFocus = remember { FocusRequester() }
    RequestLauncherControllerFocus(if (presets.isNotEmpty()) firstPresetFocus else cancelFocus, true, presets.size)
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
                presets.forEachIndexed { index, preset ->
                    TextButton(
                        onClick = { onSelect(preset) },
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .then(if (index == 0) Modifier.focusRequester(firstPresetFocus) else Modifier)
                                .tvFocusBorder(),
                    ) {
                        Text(preset.name, fontSize = 14.sp)
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss, modifier = Modifier.focusRequester(cancelFocus).tvFocusBorder()) {
                Text("Cancel")
            }
        },
    )
}

@Composable
private fun AddControlDialog(
    onDismiss: () -> Unit,
    onAddStick: () -> Unit,
    onAddButton: () -> Unit,
    onAddRadial: (SelectorPresentation) -> Unit,
    onAddSlider: () -> Unit,
    onAddDiagnostic: (DiagnosticType) -> Unit,
    onAddAxisRegion: () -> Unit,
) {
    var showInfoSubMenu by remember { mutableStateOf(false) }
    var showSelectorSubMenu by remember { mutableStateOf(false) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Add Control") },
        text = {
            val scrollState = rememberScrollState()
            Box(Modifier.heightIn(max = 300.dp)) {
                Column(modifier = Modifier.verticalScroll(scrollState)) {
                    TextButton(onClick = onAddStick, modifier = Modifier.fillMaxWidth()) {
                        Text("Analog Stick")
                    }
                    TextButton(onClick = onAddButton, modifier = Modifier.fillMaxWidth()) {
                        Text("Button")
                    }
                    TextButton(
                        onClick = { showSelectorSubMenu = !showSelectorSubMenu },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Multi-selector")
                    }
                    if (showSelectorSubMenu) {
                        TextButton(
                            onClick = { onAddRadial(SelectorPresentation.WHEEL) },
                            modifier = Modifier.fillMaxWidth().padding(start = 24.dp),
                        ) {
                            Text("Wheel")
                        }
                        TextButton(
                            onClick = { onAddRadial(SelectorPresentation.SCROLL_STRIP) },
                            modifier = Modifier.fillMaxWidth().padding(start = 24.dp),
                        ) {
                            Text("Scroll strip")
                        }
                    }
                    TextButton(onClick = onAddSlider, modifier = Modifier.fillMaxWidth()) {
                        Text("Slider")
                    }
                    TextButton(
                        onClick = { showInfoSubMenu = !showInfoSubMenu },
                        modifier = Modifier.fillMaxWidth(),
                    ) {
                        Text("Info")
                    }
                    if (showInfoSubMenu) {
                        DiagnosticType.entries.forEach { dt ->
                            TextButton(
                                onClick = { onAddDiagnostic(dt) },
                                modifier = Modifier.fillMaxWidth().padding(start = 24.dp),
                            ) {
                                Text("info: ${dt.label}")
                            }
                        }
                    }
                    TextButton(onClick = onAddAxisRegion, modifier = Modifier.fillMaxWidth()) {
                        Text("Axis Region")
                    }
                }
                ScrollArrows(scrollState)
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
    val focusManager = LocalFocusManager.current
    val sliderFocus = remember { FocusRequester() }
    val okFocus = remember { FocusRequester() }
    RequestLauncherControllerFocus(sliderFocus, true, layout.globalOpacity)
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
                    modifier =
                        Modifier
                            .focusRequester(sliderFocus)
                            .onPreviewKeyEvent { event ->
                                val isVertical =
                                    event.key == Key.DirectionUp ||
                                        event.key == Key.DirectionDown
                                if (!isVertical) return@onPreviewKeyEvent false
                                if (event.type == KeyEventType.KeyDown) {
                                    if (event.key == Key.DirectionDown) {
                                        okFocus.requestFocusSafely()
                                    } else {
                                        focusManager.moveFocus(FocusDirection.Up)
                                    }
                                }
                                true
                            }.tvFocusBorder(),
                )
            }
        },
        confirmButton = {
            TextButton(onClick = onDismiss, modifier = Modifier.focusRequester(okFocus).tvFocusBorder()) {
                Text("OK")
            }
        },
        dismissButton = {},
    )
}

@Composable
private fun GyroAxisPicker(
    label: String,
    current: Int,
    modifier: Modifier = Modifier,
    onChange: (Int) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    val currentLabel =
        if (current < 0) {
            "Disabled"
        } else {
            (
                TouchBindings.GYRO_AXIS_LABELS[current]
                    ?: TouchBindings.AXIS_LABELS[current] ?: "Axis $current"
            )
        }

    Column(modifier = modifier) {
        Text(label, color = Color.Gray, fontSize = 11.sp)
        Box {
            TextButton(onClick = { expanded = true }) {
                Text(currentLabel, fontSize = 11.sp)
            }
            DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                DropdownMenuItem(
                    text = { Text("Disabled", fontSize = 12.sp) },
                    onClick = {
                        onChange(-1)
                        expanded = false
                    },
                )
                TouchBindings.GYRO_AXIS_LABELS.forEach { (idx, name) ->
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
private fun GyroSettingsDialog(
    gyro: GyroConfig,
    onDismiss: () -> Unit,
    onUpdate: (GyroConfig) -> Unit,
    onRecenter: () -> Unit = {},
) {
    var enabled by remember { mutableStateOf(gyro.enabled) }
    var activation by remember { mutableStateOf(gyro.activation) }
    var mode by remember { mutableStateOf(gyro.mode) }
    var invertX by remember { mutableStateOf(gyro.invertX) }
    var invertY by remember { mutableStateOf(gyro.invertY) }
    var invertZ by remember { mutableStateOf(gyro.invertZ) }
    var deadzone by remember { mutableFloatStateOf(gyro.deadzone) }
    var deadzoneX by remember { mutableFloatStateOf(gyro.deadzoneX) }
    var deadzoneY by remember { mutableFloatStateOf(gyro.deadzoneY) }
    var deadzoneZ by remember { mutableFloatStateOf(gyro.deadzoneZ) }
    var maxAngleX by remember { mutableFloatStateOf(gyro.maxAngleX) }
    var maxAngleY by remember { mutableFloatStateOf(gyro.maxAngleY) }
    var maxAngleZ by remember { mutableFloatStateOf(gyro.maxAngleZ) }
    var axisX by remember { mutableIntStateOf(gyro.axisX) }
    var axisY by remember { mutableIntStateOf(gyro.axisY) }
    var axisZ by remember { mutableIntStateOf(gyro.axisZ) }
    var refAzimuth by remember { mutableStateOf(gyro.refAzimuth) }
    var refPitch by remember { mutableStateOf(gyro.refPitch) }
    var refRoll by remember { mutableStateOf(gyro.refRoll) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("Gyro Settings")
                Spacer(Modifier.width(8.dp))
                Checkbox(
                    checked = enabled,
                    onCheckedChange = { enabled = it },
                    modifier = Modifier.size(20.dp),
                )
            }
        },
        text = {
            val scrollState = rememberScrollState()
            Box {
                Column(modifier = Modifier.verticalScroll(scrollState)) {
                    LabeledToggle("Enabled", enabled) { enabled = it }

                    if (enabled) {
                        Spacer(Modifier.height(8.dp))
                        Text("Activation Mode", color = Color.Gray, fontSize = 12.sp)
                        GyroActivation.entries.forEach { act ->
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                RadioButton(
                                    selected = activation == act,
                                    onClick = { activation = act },
                                    modifier = Modifier.size(20.dp),
                                )
                                Spacer(Modifier.width(4.dp))
                                Text(
                                    act.name.lowercase().replace('_', ' '),
                                    fontSize = 12.sp,
                                    color = Color.White,
                                )
                            }
                        }

                        Spacer(Modifier.height(8.dp))
                        Text("Response Mode", color = Color.Gray, fontSize = 12.sp)
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            RadioButton(
                                selected = mode == GyroMode.ABSOLUTE,
                                onClick = { mode = GyroMode.ABSOLUTE },
                                modifier = Modifier.size(20.dp),
                            )
                            Spacer(Modifier.width(4.dp))
                            Text("Absolute (tilt angle)", fontSize = 12.sp, color = Color.White)
                        }
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            RadioButton(
                                selected = mode == GyroMode.RATE,
                                onClick = { mode = GyroMode.RATE },
                                modifier = Modifier.size(20.dp),
                            )
                            Spacer(Modifier.width(4.dp))
                            Text("Rate (angular velocity)", fontSize = 12.sp, color = Color.White)
                        }

                        // Per-axis binding (landscape: yaw=X, roll=Y, pitch=Z)
                        Spacer(Modifier.height(8.dp))
                        Text("Axis Bindings", color = Color.Gray, fontSize = 12.sp)
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            GyroAxisPicker("Yaw axis", axisX, Modifier.weight(1f)) { axisX = it }
                            GyroAxisPicker("Roll axis", axisY, Modifier.weight(1f)) { axisY = it }
                            GyroAxisPicker("Pitch axis", axisZ, Modifier.weight(1f)) { axisZ = it }
                        }

                        // Per-axis tilt range (absolute mode only)
                        if (mode == GyroMode.ABSOLUTE) {
                            Spacer(Modifier.height(8.dp))
                            if (axisX >= 0) {
                                val degX = "%.0f".format(Math.toDegrees(maxAngleX.toDouble()))
                                LabeledSlider("Tilt Range Yaw ($degX deg)", maxAngleX, 0.1f, 1.57f) { maxAngleX = it }
                            }
                            if (axisY >= 0) {
                                val degY = "%.0f".format(Math.toDegrees(maxAngleY.toDouble()))
                                LabeledSlider("Tilt Range Roll ($degY deg)", maxAngleY, 0.1f, 1.57f) { maxAngleY = it }
                            }
                            if (axisZ >= 0) {
                                val degZ = "%.0f".format(Math.toDegrees(maxAngleZ.toDouble()))
                                LabeledSlider("Tilt Range Pitch ($degZ deg)", maxAngleZ, 0.1f, 1.57f) { maxAngleZ = it }
                            }
                        }

                        if (axisX >= 0) {
                            val dzXPct = "%.0f".format(deadzoneX * 100f)
                            LabeledSlider("Deadzone Yaw ($dzXPct%)", deadzoneX, 0f, 0.6f) { deadzoneX = it }
                        }
                        if (axisY >= 0) {
                            val dzYPct = "%.0f".format(deadzoneY * 100f)
                            LabeledSlider("Deadzone Roll ($dzYPct%)", deadzoneY, 0f, 0.6f) { deadzoneY = it }
                        }
                        if (axisZ >= 0) {
                            val dzZPct = "%.0f".format(deadzoneZ * 100f)
                            LabeledSlider("Deadzone Pitch ($dzZPct%)", deadzoneZ, 0f, 0.6f) { deadzoneZ = it }
                        }

                        Row(horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                            if (axisX >= 0) LabeledToggle("Inv Yaw", invertX) { invertX = it }
                            if (axisY >= 0) LabeledToggle("Inv Roll", invertY) { invertY = it }
                            if (axisZ >= 0) LabeledToggle("Inv Pitch", invertZ) { invertZ = it }
                        }

                        Spacer(Modifier.height(8.dp))
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            TextButton(onClick = onRecenter) {
                                Text("Recenter Now", fontSize = 12.sp)
                            }
                            if (refAzimuth != null) {
                                TextButton(onClick = {
                                    refAzimuth = null
                                    refPitch = null
                                    refRoll = null
                                }) { Text("Reset Saved", fontSize = 12.sp) }
                            }
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
                        mode = mode,
                        invertX = invertX,
                        invertY = invertY,
                        invertZ = invertZ,
                        deadzone = deadzoneX, // legacy field tracks yaw for compat
                        deadzoneX = deadzoneX,
                        deadzoneY = deadzoneY,
                        deadzoneZ = deadzoneZ,
                        maxAngleX = maxAngleX,
                        maxAngleY = maxAngleY,
                        maxAngleZ = maxAngleZ,
                        axisX = axisX,
                        axisY = axisY,
                        axisZ = axisZ,
                        refAzimuth = refAzimuth,
                        refPitch = refPitch,
                        refRoll = refRoll,
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
