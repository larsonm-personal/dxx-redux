package com.dxxredux.app

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.focusable
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
import androidx.compose.foundation.gestures.scrollBy
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.zIndex
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlin.math.roundToInt

/**
 * Weapon autoselect ordering editor.
 * Matches the in-game "Reorder Primary" / "Reorder Secondary" menus exactly:
 * same weapon names, same separator text, same ordering.
 *
 * D1: 5 primary weapons + Quad Lasers + separator, 5 secondary + separator
 * D2: 10 primary weapons + separator, 10 secondary + separator
 *
 * Items above the separator are auto-selected (highest priority first).
 * Items below are never auto-selected.
 *
 * Long-press and drag to reorder.  Save writes to all pilot files.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AutoselectEditorPage(
    gameVariant: String,
    filesDir: String,
    onBack: () -> Unit,
) {
    var activeGame by remember { mutableStateOf(gameVariant) }

    // Weapon index -> display name maps, built from paired JNI entries
    var primaryNameMap by remember { mutableStateOf(emptyMap<Int, String>()) }
    var secondaryNameMap by remember { mutableStateOf(emptyMap<Int, String>()) }

    // Current ordering (mutable lists for drag reorder)
    val primaryOrder = remember { mutableStateListOf<Int>() }
    val secondaryOrder = remember { mutableStateListOf<Int>() }

    // Saved ordering (to detect changes)
    var savedPrimary by remember { mutableStateOf(listOf<Int>()) }
    var savedSecondary by remember { mutableStateOf(listOf<Int>()) }

    var statusMessage by remember { mutableStateOf("") }
    var hasChanges by remember { mutableStateOf(false) }

    fun weaponName(
        index: Int,
        isPrimary: Boolean,
    ): String {
        val map = if (isPrimary) primaryNameMap else secondaryNameMap
        return map[index] ?: "Unknown"
    }

    fun loadOrdering() {
        val primEntries = NativeAutoselectPatcher.getPrimaryWeaponEntries(activeGame)
        val secEntries = NativeAutoselectPatcher.getSecondaryWeaponEntries(activeGame)
        primaryNameMap = NativeAutoselectPatcher.parseWeaponEntries(primEntries)
        secondaryNameMap = NativeAutoselectPatcher.parseWeaponEntries(secEntries)
        val primLen = primaryNameMap.size
        val secLen = secondaryNameMap.size

        val data = NativeAutoselectPatcher.readAutoselect(activeGame, filesDir)
        if (data.isEmpty()) {
            // No pilot files -- use defaults (map keys are in default order)
            primaryOrder.clear()
            primaryOrder.addAll(primaryNameMap.keys)
            secondaryOrder.clear()
            secondaryOrder.addAll(secondaryNameMap.keys)
            statusMessage = "No pilot files found - showing defaults"
        } else {
            primaryOrder.clear()
            secondaryOrder.clear()
            for (i in 0 until primLen) primaryOrder.add(data[i])
            for (i in 0 until secLen) secondaryOrder.add(data[primLen + i])
            statusMessage = ""
        }

        savedPrimary = primaryOrder.toList()
        savedSecondary = secondaryOrder.toList()
        hasChanges = false
    }

    LaunchedEffect(activeGame) {
        loadOrdering()
    }

    // Check for changes whenever ordering updates
    fun checkChanges() {
        hasChanges = primaryOrder.toList() != savedPrimary ||
            secondaryOrder.toList() != savedSecondary
    }

    val isLandscape =
        android.content.res.Configuration.ORIENTATION_LANDSCAPE ==
            androidx.compose.ui.platform.LocalConfiguration.current.orientation
    val initialFocus = remember { FocusRequester() }
    LaunchedEffect(Unit) { initialFocus.requestFocus() }

    MaterialTheme(colorScheme = darkColorScheme()) {
        Scaffold(
            topBar = {
                TopAppBar(
                    title = {
                        if (isLandscape) {
                            Row(
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.spacedBy(12.dp),
                            ) {
                                Text("Weapon Autoselect")
                                Text(
                                    "Long press + drag, or select + D-pad to reorder",
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    fontSize = 9.sp,
                                )
                            }
                        } else {
                            Text("Weapon Autoselect")
                        }
                    },
                    navigationIcon = {
                        IconButton(onClick = onBack) {
                            Icon(Icons.AutoMirrored.Filled.ArrowBack, "Back")
                        }
                    },
                    colors =
                        TopAppBarDefaults.topAppBarColors(
                            containerColor = MaterialTheme.colorScheme.surface,
                        ),
                )
            },
        ) { padding ->
            Column(
                modifier =
                    Modifier
                        .fillMaxSize()
                        .padding(padding)
                        .padding(horizontal = 16.dp),
            ) {
                // Game selector (only show if we might have both)
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.padding(vertical = 2.dp),
                ) {
                    OutlinedButton(
                        onClick = { activeGame = "d1" },
                        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 1.dp),
                        modifier = Modifier.weight(1f).focusRequester(initialFocus),
                        border =
                            if (activeGame == "d1") {
                                androidx.compose.foundation.BorderStroke(
                                    2.dp,
                                    MaterialTheme.colorScheme.primary,
                                )
                            } else {
                                null
                            },
                    ) {
                        Text(
                            "Descent 1",
                            fontWeight = if (activeGame == "d1") FontWeight.Bold else FontWeight.Normal,
                        )
                    }
                    OutlinedButton(
                        onClick = { activeGame = "d2" },
                        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 1.dp),
                        modifier = Modifier.weight(1f),
                        border =
                            if (activeGame == "d2") {
                                androidx.compose.foundation.BorderStroke(
                                    2.dp,
                                    MaterialTheme.colorScheme.primary,
                                )
                            } else {
                                null
                            },
                    ) {
                        Text(
                            "Descent 2",
                            fontWeight = if (activeGame == "d2") FontWeight.Bold else FontWeight.Normal,
                        )
                    }
                }

                if (statusMessage.isNotEmpty()) {
                    Text(
                        statusMessage,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        fontSize = 12.sp,
                        modifier = Modifier.padding(bottom = 4.dp),
                    )
                }

                // Instructions (portrait only; landscape shows in TopAppBar)
                if (!isLandscape) {
                    Text(
                        "Long press + drag, or select + D-pad to reorder",
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        fontSize = 9.sp,
                        modifier = Modifier.padding(bottom = 8.dp),
                    )
                }

                // Two weapon lists side by side in landscape, stacked in portrait
                Row(
                    modifier = Modifier.weight(1f),
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                ) {
                    // Primary weapons
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            "Reorder Primary",
                            fontWeight = FontWeight.Bold,
                            fontSize = 14.sp,
                            modifier = Modifier.padding(bottom = 4.dp),
                        )
                        DragReorderList(
                            items = primaryOrder,
                            nameResolver = { weaponName(it, isPrimary = true) },
                            onReorder = { from, to ->
                                val item = primaryOrder.removeAt(from)
                                primaryOrder.add(to, item)
                                checkChanges()
                            },
                            modifier = Modifier.weight(1f),
                        )
                    }

                    // Secondary weapons
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            "Reorder Secondary",
                            fontWeight = FontWeight.Bold,
                            fontSize = 14.sp,
                            modifier = Modifier.padding(bottom = 4.dp),
                        )
                        DragReorderList(
                            items = secondaryOrder,
                            nameResolver = { weaponName(it, isPrimary = false) },
                            onReorder = { from, to ->
                                val item = secondaryOrder.removeAt(from)
                                secondaryOrder.add(to, item)
                                checkChanges()
                            },
                            modifier = Modifier.weight(1f),
                        )
                    }
                }

                // Save / Reset buttons
                Row(
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .padding(vertical = 4.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    OutlinedButton(
                        onClick = {
                            primaryOrder.clear()
                            primaryOrder.addAll(primaryNameMap.keys)
                            secondaryOrder.clear()
                            secondaryOrder.addAll(secondaryNameMap.keys)
                            checkChanges()
                        },
                        modifier = Modifier.weight(1f).height(28.dp),
                        contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
                    ) {
                        Text("Reset to Defaults", fontSize = 12.sp)
                    }

                    Button(
                        onClick = {
                            val count =
                                NativeAutoselectPatcher.writeAutoselect(
                                    activeGame,
                                    filesDir,
                                    primaryOrder.toIntArray(),
                                    secondaryOrder.toIntArray(),
                                )
                            savedPrimary = primaryOrder.toList()
                            savedSecondary = secondaryOrder.toList()
                            hasChanges = false
                            statusMessage = "Saved to $count pilot file(s)"
                        },
                        enabled = hasChanges,
                        modifier = Modifier.weight(1f).height(28.dp),
                        contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
                    ) {
                        Text("Save", fontSize = 12.sp)
                    }
                }
            }
        }
    }
}

/**
 * Drag-to-reorder list for weapon ordering.
 *
 * Long-press an item to begin dragging.  While dragging, other items
 * animate into their new positions around the held item.  Dragging
 * near the top or bottom edge auto-scrolls the list so items can be
 * moved beyond the visible viewport.
 *
 * D-pad / controller: press A on a focused item to grab it, then
 * D-pad up/down to move it, A again to drop it.
 *
 * Uses value-based keys so the dragged composable (and its gesture
 * handler) survives list mutations.
 */
@Composable
private fun DragReorderList(
    items: List<Int>,
    nameResolver: (Int) -> String,
    onReorder: (from: Int, to: Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    val listState = rememberLazyListState()
    val itemHeightDp = 40.dp
    val itemHeightPx =
        with(androidx.compose.ui.platform.LocalDensity.current) {
            itemHeightDp.toPx()
        }

    var draggedValue by remember { mutableStateOf<Int?>(null) }
    var dragOffsetY by remember { mutableStateOf(0f) }
    var overscrollSpeed by remember { mutableStateOf(0f) }

    // D-pad grab-and-move: index of the grabbed item, or -1
    var grabbedIndex by remember { mutableIntStateOf(-1) }
    // Track which index is focused for key handling
    var focusedIndex by remember { mutableIntStateOf(-1) }
    // FocusRequesters so we can move focus after a reorder
    val focusRequesters = remember(items.size) { List(items.size) { FocusRequester() } }

    // Auto-scroll while dragging near viewport edges.
    LaunchedEffect(draggedValue) {
        val value = draggedValue ?: return@LaunchedEffect
        while (isActive) {
            if (overscrollSpeed != 0f) {
                val scrolled = listState.scrollBy(overscrollSpeed)
                if (scrolled != 0f) {
                    dragOffsetY += scrolled
                    val idx = items.indexOf(value)
                    if (idx >= 0) {
                        val target =
                            (idx + (dragOffsetY / itemHeightPx).roundToInt())
                                .coerceIn(0, items.size - 1)
                        if (target != idx) {
                            onReorder(idx, target)
                            dragOffsetY -= (target - idx) * itemHeightPx
                        }
                    }
                }
            }
            delay(16)
        }
    }

    Box(modifier = modifier) {
        LazyColumn(
            state = listState,
            modifier = Modifier.fillMaxSize(),
            userScrollEnabled = draggedValue == null,
        ) {
            itemsIndexed(items, key = { _, value -> value }) { index, value ->
                val isDragging = value == draggedValue
                val isGrabbed = index == grabbedIndex
                val isSeparator = value == NativeAutoselectPatcher.SEPARATOR
                val name = nameResolver(value)
                val focusReq = focusRequesters.getOrNull(index)

                Box(
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .height(itemHeightDp)
                            .then(
                                if (isDragging) {
                                    Modifier
                                        .zIndex(1f)
                                        .offset { IntOffset(0, dragOffsetY.roundToInt()) }
                                        .graphicsLayer { alpha = 0.8f }
                                } else {
                                    Modifier.animateItem()
                                },
                            ).background(
                                when {
                                    isDragging || isGrabbed -> MaterialTheme.colorScheme.primaryContainer
                                    isSeparator -> MaterialTheme.colorScheme.surfaceVariant
                                    else -> Color.Transparent
                                },
                            ).then(
                                if (isGrabbed) {
                                    Modifier.border(2.dp, MaterialTheme.colorScheme.primary)
                                } else {
                                    Modifier
                                },
                            ).onFocusChanged { state ->
                                if (state.isFocused) focusedIndex = index
                            }.onKeyEvent { keyEvent ->
                                if (keyEvent.type != KeyEventType.KeyDown) return@onKeyEvent false
                                val k = keyEvent.key
                                if (k == Key.DirectionCenter || k == Key.Enter) {
                                    if (grabbedIndex < 0) {
                                        // Grab this item
                                        grabbedIndex = index
                                    } else {
                                        // Drop
                                        grabbedIndex = -1
                                    }
                                    return@onKeyEvent true
                                }
                                if (grabbedIndex >= 0) {
                                    val target =
                                        when (k) {
                                            Key.DirectionUp -> (grabbedIndex - 1).coerceAtLeast(0)
                                            Key.DirectionDown -> (grabbedIndex + 1).coerceAtMost(items.size - 1)
                                            else -> return@onKeyEvent false
                                        }
                                    if (target != grabbedIndex) {
                                        onReorder(grabbedIndex, target)
                                        val newReq = focusRequesters.getOrNull(target)
                                        grabbedIndex = target
                                        // Move focus to follow the item
                                        newReq?.requestFocus()
                                    }
                                    return@onKeyEvent true
                                }
                                false
                            }.then(
                                if (focusReq != null) {
                                    Modifier.focusRequester(focusReq)
                                } else {
                                    Modifier
                                },
                            ).focusable()
                            .pointerInput(value) {
                                detectDragGesturesAfterLongPress(
                                    onDragStart = {
                                        draggedValue = value
                                        dragOffsetY = 0f
                                        overscrollSpeed = 0f
                                    },
                                    onDrag = { change, dragAmount ->
                                        change.consume()
                                        dragOffsetY += dragAmount.y

                                        val currentIdx = items.indexOf(value)
                                        if (currentIdx >= 0) {
                                            val target =
                                                (currentIdx + (dragOffsetY / itemHeightPx).roundToInt())
                                                    .coerceIn(0, items.size - 1)
                                            if (target != currentIdx) {
                                                onReorder(currentIdx, target)
                                                dragOffsetY -= (target - currentIdx) * itemHeightPx
                                            }

                                            // Look up by key (value) for correct position after swaps.
                                            val info =
                                                listState.layoutInfo.visibleItemsInfo
                                                    .find { it.key == value }
                                            if (info != null) {
                                                val center = info.offset + info.size / 2 + dragOffsetY
                                                val vpH = listState.layoutInfo.viewportEndOffset.toFloat()
                                                val edge = vpH * 0.15f
                                                overscrollSpeed =
                                                    when {
                                                        center < edge ->
                                                            -(1f - (center / edge).coerceIn(0f, 1f)) *
                                                                itemHeightPx / 4f
                                                        center > vpH - edge ->
                                                            ((center - vpH + edge) / edge).coerceIn(0f, 1f) *
                                                                itemHeightPx / 4f
                                                        else -> 0f
                                                    }
                                            } else {
                                                // Item scrolled off-screen -- scroll toward it.
                                                val firstVisible = listState.firstVisibleItemIndex
                                                overscrollSpeed =
                                                    if (currentIdx < firstVisible) {
                                                        -itemHeightPx / 3f
                                                    } else {
                                                        itemHeightPx / 3f
                                                    }
                                            }
                                        }
                                    },
                                    onDragEnd = {
                                        draggedValue = null
                                        dragOffsetY = 0f
                                        overscrollSpeed = 0f
                                    },
                                    onDragCancel = {
                                        draggedValue = null
                                        dragOffsetY = 0f
                                        overscrollSpeed = 0f
                                    },
                                )
                            },
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        modifier =
                            Modifier
                                .fillMaxSize()
                                .padding(horizontal = 8.dp),
                    ) {
                        Icon(
                            Icons.Filled.Menu,
                            contentDescription = "Drag to reorder",
                            modifier = Modifier.size(20.dp),
                            tint = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(
                            text = name,
                            fontSize = if (isSeparator) 11.sp else 13.sp,
                            lineHeight = if (isSeparator) 12.sp else 16.sp,
                            fontStyle = if (isSeparator) FontStyle.Italic else FontStyle.Normal,
                            color =
                                if (isSeparator) {
                                    MaterialTheme.colorScheme.onSurfaceVariant
                                } else {
                                    MaterialTheme.colorScheme.onSurface
                                },
                            textAlign = if (isSeparator) TextAlign.Center else TextAlign.Start,
                            modifier = if (isSeparator) Modifier.fillMaxWidth() else Modifier,
                        )
                    }
                }
            }
        }
        LazyListScrollArrows(listState)
    }
}

@Composable
private fun BoxScope.LazyListScrollArrows(listState: LazyListState) {
    if (listState.canScrollBackward) {
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
    if (listState.canScrollForward) {
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
