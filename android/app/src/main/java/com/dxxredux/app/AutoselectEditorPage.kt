package com.dxxredux.app

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGesturesAfterLongPress
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
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
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material3.Button
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.zIndex
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
    val context = LocalContext.current

    // Use d2 native lib (d1 functions are compiled into d2 lib with different defines,
    // but NativeAutoselectPatcher is compiled per-game -- we need both libs)
    // Actually: both d1 and d2 are separate .so files, loaded by game variant.
    // The NativeAutoselectPatcher calls are dispatched to whichever lib is loaded.
    var activeGame by remember { mutableStateOf(gameVariant) }
    var libLoaded by remember { mutableStateOf(false) }

    // Weapon names from C (indexed by weapon number)
    var weaponNames by remember { mutableStateOf(emptyArray<String>()) }
    var primaryLen by remember { mutableIntStateOf(0) }
    var secondaryLen by remember { mutableIntStateOf(0) }

    // Current ordering (mutable lists for drag reorder)
    val primaryOrder = remember { mutableStateListOf<Int>() }
    val secondaryOrder = remember { mutableStateListOf<Int>() }

    // Saved ordering (to detect changes)
    var savedPrimary by remember { mutableStateOf(listOf<Int>()) }
    var savedSecondary by remember { mutableStateOf(listOf<Int>()) }

    var statusMessage by remember { mutableStateOf("") }
    var hasChanges by remember { mutableStateOf(false) }

    // D1 primary names: indices 0-4 = weapons, index 5 = "Quad Lasers"
    // D1 secondary names: indices 0-4 = weapons
    // D2 primary names: indices 0-9 = weapons
    // D2 secondary names: indices 0-9 = weapons (at offset 10 in names array)

    fun weaponName(
        index: Int,
        isPrimary: Boolean,
    ): String {
        if (index == NativeAutoselectPatcher.SEPARATOR) {
            return "--- Never Autoselect below ---"
        }
        return if (isPrimary) {
            if (activeGame == "d1" && index == NativeAutoselectPatcher.D1_QUAD_LASERS_INDEX) {
                // Quad Lasers is the last primary name entry for D1
                weaponNames.getOrElse(5) { "Quad Lasers" }
            } else if (index in 0 until (if (activeGame == "d1") 5 else 10)) {
                weaponNames.getOrElse(index) { "Unknown" }
            } else {
                "Unknown"
            }
        } else {
            val offset = if (activeGame == "d1") 6 else 10
            weaponNames.getOrElse(offset + index) { "Unknown" }
        }
    }

    fun loadOrdering() {
        try {
            val libName = if (activeGame == "d1") "dxx-redux-d1" else "dxx-redux-d2"
            System.loadLibrary(libName)
            libLoaded = true
        } catch (e: UnsatisfiedLinkError) {
            statusMessage = "Failed to load native library"
            return
        }

        weaponNames = NativeAutoselectPatcher.nativeGetWeaponNames()
        val lengths = NativeAutoselectPatcher.nativeGetOrderLengths()
        primaryLen = lengths[0]
        secondaryLen = lengths[1]

        val data = NativeAutoselectPatcher.nativeReadAutoselect(filesDir)
        if (data.isEmpty()) {
            // No pilot files -- use defaults
            val defaults = NativeAutoselectPatcher.nativeGetDefaultAutoselect()
            primaryOrder.clear()
            secondaryOrder.clear()
            for (i in 0 until primaryLen) primaryOrder.add(defaults[i])
            for (i in 0 until secondaryLen) secondaryOrder.add(defaults[primaryLen + i])
            statusMessage = "No pilot files found - showing defaults"
        } else {
            primaryOrder.clear()
            secondaryOrder.clear()
            for (i in 0 until primaryLen) primaryOrder.add(data[i])
            for (i in 0 until secondaryLen) secondaryOrder.add(data[primaryLen + i])
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

    MaterialTheme(colorScheme = darkColorScheme()) {
        Scaffold(
            topBar = {
                TopAppBar(
                    title = { Text("Weapon Autoselect") },
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
                    modifier = Modifier.padding(vertical = 8.dp),
                ) {
                    OutlinedButton(
                        onClick = { activeGame = "d1" },
                        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 4.dp),
                        modifier = Modifier.weight(1f),
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
                        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 4.dp),
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

                // Instructions
                Text(
                    "Long press + drag to reorder",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    fontSize = 12.sp,
                    modifier = Modifier.padding(bottom = 8.dp),
                )

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
                            .padding(vertical = 8.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    OutlinedButton(
                        onClick = {
                            val defaults = NativeAutoselectPatcher.nativeGetDefaultAutoselect()
                            primaryOrder.clear()
                            secondaryOrder.clear()
                            for (i in 0 until primaryLen) primaryOrder.add(defaults[i])
                            for (i in 0 until secondaryLen) secondaryOrder.add(defaults[primaryLen + i])
                            checkChanges()
                        },
                        modifier = Modifier.weight(1f),
                    ) {
                        Text("Reset to Defaults")
                    }

                    Button(
                        onClick = {
                            val count =
                                NativeAutoselectPatcher.nativeWriteAutoselect(
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
                        modifier = Modifier.weight(1f),
                    ) {
                        Text("Save")
                    }
                }
            }
        }
    }
}

/**
 * A drag-to-reorder list for weapon ordering.
 * Long-press an item to start dragging, release to drop.
 */
@Composable
private fun DragReorderList(
    items: List<Int>,
    nameResolver: (Int) -> String,
    onReorder: (from: Int, to: Int) -> Unit,
    modifier: Modifier = Modifier,
) {
    val itemHeightDp = 40.dp
    val itemHeightPx =
        with(androidx.compose.ui.platform.LocalDensity.current) {
            itemHeightDp.toPx()
        }

    var dragIndex by remember { mutableStateOf(-1) }
    var dragOffsetY by remember { mutableStateOf(0f) }

    Box(modifier = modifier) {
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            userScrollEnabled = dragIndex < 0,
        ) {
            itemsIndexed(items, key = { idx, value -> "$idx-$value" }) { index, value ->
                val isDragging = index == dragIndex
                val name = nameResolver(value)
                val isSeparator = value == NativeAutoselectPatcher.SEPARATOR

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
                                    Modifier
                                },
                            ).background(
                                when {
                                    isDragging -> MaterialTheme.colorScheme.primaryContainer
                                    isSeparator -> MaterialTheme.colorScheme.surfaceVariant
                                    else -> Color.Transparent
                                },
                            ).pointerInput(index, items.size) {
                                detectDragGesturesAfterLongPress(
                                    onDragStart = {
                                        dragIndex = index
                                        dragOffsetY = 0f
                                    },
                                    onDrag = { change, dragAmount ->
                                        change.consume()
                                        dragOffsetY += dragAmount.y
                                        // Determine target index from drag offset
                                        val targetIndex =
                                            (
                                                index +
                                                    (dragOffsetY / itemHeightPx)
                                                        .roundToInt()
                                            ).coerceIn(0, items.size - 1)
                                        if (targetIndex != index) {
                                            onReorder(index, targetIndex)
                                            dragIndex = targetIndex
                                            dragOffsetY -= (targetIndex - index) * itemHeightPx
                                        }
                                    },
                                    onDragEnd = {
                                        dragIndex = -1
                                        dragOffsetY = 0f
                                    },
                                    onDragCancel = {
                                        dragIndex = -1
                                        dragOffsetY = 0f
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
                            fontSize = 13.sp,
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
    }
}
