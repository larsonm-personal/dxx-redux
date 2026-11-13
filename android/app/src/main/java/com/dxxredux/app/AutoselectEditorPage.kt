package com.dxxredux.app

import androidx.activity.compose.BackHandler
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
import androidx.compose.runtime.rememberCoroutineScope
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
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.zIndex
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
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
private data class AutoselectShownOrdering(
    val primary: List<Int>,
    val secondary: List<Int>,
    val pilotCount: Int,
    val mismatchCount: Int,
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AutoselectEditorPage(
    gameVariant: String,
    filesDir: String,
    onBack: () -> Unit,
) {
    val draft = rememberSettingsEditorDraft("weapon.autoselect")
    val scope = rememberCoroutineScope()
    var activeGame by draft.state("activeGame") { mutableStateOf(gameVariant) }

    // Weapon index -> display name maps, built from paired JNI entries
    var primaryNameMap by draft.state("primaryNameMap") { mutableStateOf(emptyMap<Int, String>()) }
    var secondaryNameMap by draft.state("secondaryNameMap") { mutableStateOf(emptyMap<Int, String>()) }

    // Current ordering (mutable lists for drag reorder)
    val primaryOrder = draft.state("primaryOrder") { mutableStateListOf<Int>() }
    val secondaryOrder = draft.state("secondaryOrder") { mutableStateListOf<Int>() }

    var statusMessage by draft.state("statusMessage") { mutableStateOf("") }
    var hasChanges by draft.state("hasChanges") { mutableStateOf(false) }
    var hasMismatchedPilots by draft.state("hasMismatchedPilots") { mutableStateOf(false) }
    var d1Ordering by draft.state("d1Ordering") { mutableStateOf<AutoselectShownOrdering?>(null) }
    var d2Ordering by draft.state("d2Ordering") { mutableStateOf<AutoselectShownOrdering?>(null) }
    var mismatchGeneration by draft.state("mismatchGeneration") { mutableIntStateOf(0) }
    var saving by remember { mutableStateOf(false) }
    val savedOrderings =
        draft.state("savedOrderings") {
            androidx.compose.runtime.mutableStateMapOf<String, AutoselectShownOrdering>()
        }

    fun hasDraftEdits(): Boolean =
        savedOrderings.any { (game, saved) ->
            val shown = if (game == "d1") d1Ordering else d2Ordering
            shown != null && (shown.primary != saved.primary || shown.secondary != saved.secondary)
        }

    fun weaponName(
        index: Int,
        isPrimary: Boolean,
    ): String {
        val map = if (isPrimary) primaryNameMap else secondaryNameMap
        return map[index] ?: "Unknown"
    }

    fun orderingFor(game: String): AutoselectShownOrdering? = if (game == "d1") d1Ordering else d2Ordering

    fun setOrderingFor(
        game: String,
        ordering: AutoselectShownOrdering,
    ) {
        if (game == "d1") {
            d1Ordering = ordering
        } else {
            d2Ordering = ordering
        }
    }

    fun updateSaveState(
        nextD1Ordering: AutoselectShownOrdering? = d1Ordering,
        nextD2Ordering: AutoselectShownOrdering? = d2Ordering,
    ) {
        val d1MismatchCount = nextD1Ordering?.mismatchCount ?: 0
        val d2MismatchCount = nextD2Ordering?.mismatchCount ?: 0
        hasChanges =
            autoselectHasPendingSave(
                d1MismatchCount = d1MismatchCount,
                d2MismatchCount = d2MismatchCount,
            )
        hasMismatchedPilots = hasChanges
    }

    fun orderingWithMismatchCount(
        game: String,
        primary: List<Int>,
        secondary: List<Int>,
    ): AutoselectShownOrdering {
        val mismatch =
            NativeAutoselectPatcher.countAutoselectMismatches(
                game,
                filesDir,
                primary.toIntArray(),
                secondary.toIntArray(),
            )
        return AutoselectShownOrdering(
            primary = primary,
            secondary = secondary,
            pilotCount = mismatch.pilotCount,
            mismatchCount = mismatch.mismatchCount,
        )
    }

    fun loadGameOrdering(game: String): AutoselectShownOrdering {
        val primaryNames =
            NativeAutoselectPatcher.parseWeaponEntries(
                NativeAutoselectPatcher.getPrimaryWeaponEntries(game),
            )
        val secondaryNames =
            NativeAutoselectPatcher.parseWeaponEntries(
                NativeAutoselectPatcher.getSecondaryWeaponEntries(game),
            )
        val summary = NativeAutoselectPatcher.readAutoselectSummary(game, filesDir)
        val primary =
            summary?.primary?.toList()
                ?: primaryNames.keys.toList()
        val secondary =
            summary?.secondary?.toList()
                ?: secondaryNames.keys.toList()
        return orderingWithMismatchCount(game, primary, secondary)
    }

    fun syncActiveOrderingFromUi(): AutoselectShownOrdering {
        val previous = orderingFor(activeGame)
        val ordering =
            AutoselectShownOrdering(
                primary = primaryOrder.toList(),
                secondary = secondaryOrder.toList(),
                pilotCount = previous?.pilotCount ?: 0,
                mismatchCount = previous?.mismatchCount ?: 0,
            )
        setOrderingFor(
            activeGame,
            ordering,
        )
        statusMessage =
            if (ordering.pilotCount == 0) {
                "No pilot files found - showing defaults"
            } else {
                ""
            }
        return ordering
    }

    fun switchGame(game: String) {
        if (activeGame == game || saving) return
        syncActiveOrderingFromUi()
        activeGame = game
    }

    fun saveMismatchedGames(leaveAfterSave: Boolean = false) {
        if (saving) return
        val activeOrdering = syncActiveOrderingFromUi()
        val requestedD1 = if (activeGame == "d1") activeOrdering else d1Ordering
        val requestedD2 = if (activeGame == "d2") activeOrdering else d2Ordering
        saving = true
        scope.launch {
            try {
                val results =
                    withContext(Dispatchers.IO) {
                        listOf("d1" to requestedD1, "d2" to requestedD2).mapNotNull { (game, requested) ->
                            requested ?: return@mapNotNull null
                            // Recheck here: the debounced UI mismatch count may still describe an older order
                            val checked = orderingWithMismatchCount(game, requested.primary, requested.secondary)
                            val savedCount =
                                if (checked.mismatchCount > 0) {
                                    NativeAutoselectPatcher.writeAutoselect(
                                        game,
                                        filesDir,
                                        requested.primary.toIntArray(),
                                        requested.secondary.toIntArray(),
                                    )
                                } else {
                                    0
                                }
                            Triple(
                                game,
                                orderingWithMismatchCount(game, requested.primary, requested.secondary),
                                savedCount,
                            )
                        }
                    }
                var failed = false
                var savedCount = 0
                results.forEach { (game, ordering, count) ->
                    setOrderingFor(game, ordering)
                    savedCount += count.coerceAtLeast(0)
                    val baseline = savedOrderings[game]
                    val edited =
                        baseline != null &&
                            (ordering.primary != baseline.primary || ordering.secondary != baseline.secondary)
                    if (count < 0 || ordering.mismatchCount > 0 || (edited && ordering.pilotCount == 0)) {
                        failed = true
                    } else {
                        savedOrderings[game] = ordering
                    }
                }
                updateSaveState()
                statusMessage =
                    if (failed) {
                        "Some weapon ordering changes could not be saved. Check pilot files and retry"
                    } else {
                        "Saved to $savedCount pilot file(s)"
                    }
                if (!failed && leaveAfterSave) onBack()
            } catch (e: kotlinx.coroutines.CancellationException) {
                throw e
            } catch (_: Exception) {
                statusMessage = "Could not save weapon ordering. Please retry"
            } finally {
                saving = false
            }
        }
    }

    val requestExit =
        rememberSettingsEditorExit(
            dirty = hasDraftEdits(),
            onSaveAndLeave = { saveMismatchedGames(leaveAfterSave = true) },
            onLeave = onBack,
            saving = saving,
        )

    LaunchedEffect(activeGame) {
        val game = activeGame
        val cachedOrdering = orderingFor(game)
        val loaded =
            withContext(Dispatchers.IO) {
                val ordering = cachedOrdering ?: loadGameOrdering(game)
                val primaryNames =
                    NativeAutoselectPatcher.parseWeaponEntries(
                        NativeAutoselectPatcher.getPrimaryWeaponEntries(game),
                    )
                val secondaryNames =
                    NativeAutoselectPatcher.parseWeaponEntries(
                        NativeAutoselectPatcher.getSecondaryWeaponEntries(game),
                    )
                Triple(ordering, primaryNames, secondaryNames)
            }
        if (savedOrderings[game] == null) savedOrderings[game] = loaded.first
        setOrderingFor(game, loaded.first)
        primaryNameMap = loaded.second
        secondaryNameMap = loaded.third
        primaryOrder.clear()
        primaryOrder.addAll(loaded.first.primary)
        secondaryOrder.clear()
        secondaryOrder.addAll(loaded.first.secondary)
        updateSaveState()
    }

    // Check for changes whenever ordering updates
    fun checkChanges() {
        if (primaryOrder.isEmpty() || secondaryOrder.isEmpty()) {
            updateSaveState()
        } else {
            syncActiveOrderingFromUi()
            hasChanges = true
            hasMismatchedPilots = true
            mismatchGeneration++
        }
    }

    LaunchedEffect(mismatchGeneration) {
        if (mismatchGeneration == 0 || primaryOrder.isEmpty() || secondaryOrder.isEmpty()) {
            return@LaunchedEffect
        }
        delay(150)
        val game = activeGame
        val primary = primaryOrder.toList()
        val secondary = secondaryOrder.toList()
        val checked =
            withContext(Dispatchers.IO) {
                orderingWithMismatchCount(game, primary, secondary)
            }
        val current = orderingFor(game)
        if (current?.primary != primary || current.secondary != secondary) {
            return@LaunchedEffect
        }
        setOrderingFor(game, checked)
        updateSaveState()
    }

    val isLandscape =
        android.content.res.Configuration.ORIENTATION_LANDSCAPE ==
            androidx.compose.ui.platform.LocalConfiguration.current.orientation
    val initialFocus = remember { FocusRequester() }
    RequestLauncherControllerFocus(initialFocus, true, activeGame)

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
                        IconButton(onClick = requestExit, modifier = Modifier.tvFocusBorder()) {
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
                // Instructions (portrait only; landscape shows in TopAppBar)
                if (!isLandscape) {
                    Text(
                        "Long press + drag, or select + D-pad to reorder",
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        fontSize = 9.sp,
                        modifier = Modifier.padding(top = 2.dp, bottom = 4.dp),
                    )
                }

                // Game selector (only show if we might have both)
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.padding(bottom = 2.dp),
                ) {
                    OutlinedButton(
                        onClick = { switchGame("d1") },
                        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 1.dp),
                        modifier = Modifier.weight(1f).focusRequester(initialFocus).tvFocusBorder(),
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
                        onClick = { switchGame("d2") },
                        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 1.dp),
                        modifier = Modifier.weight(1f).tvFocusBorder(),
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
                if (hasMismatchedPilots) {
                    Text(
                        "Pilot files do not match the shown weapon ordering. Save to apply the shown ordering to all pilots.",
                        color = MaterialTheme.colorScheme.tertiary,
                        fontSize = 12.sp,
                        modifier = Modifier.padding(bottom = 4.dp),
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
                            onReorder = reorder@{ from, to ->
                                if (saving) return@reorder
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
                            onReorder = reorder@{ from, to ->
                                if (saving) return@reorder
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
                        enabled = !saving,
                        onClick = {
                            primaryOrder.clear()
                            primaryOrder.addAll(primaryNameMap.keys)
                            secondaryOrder.clear()
                            secondaryOrder.addAll(secondaryNameMap.keys)
                            checkChanges()
                        },
                        modifier = Modifier.weight(1f).height(28.dp).tvFocusBorder(),
                        contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
                    ) {
                        Text("Reset to Defaults", fontSize = 12.sp)
                    }

                    Button(
                        onClick = {
                            saveMismatchedGames()
                        },
                        enabled = (hasChanges || hasDraftEdits()) && !saving,
                        modifier = Modifier.weight(1f).height(28.dp).tvFocusBorder(),
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
    val dpadReorderScrollScope = rememberCoroutineScope()
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
    var grabbedOriginalIndex by remember { mutableIntStateOf(-1) }
    var grabbedOriginalValue by remember { mutableStateOf<Int?>(null) }
    // FocusRequesters so we can move focus after a reorder
    val focusRequesters = remember(items.size) { List(items.size) { FocusRequester() } }

    fun clearGrabbedItem() {
        grabbedIndex = -1
        grabbedOriginalIndex = -1
        grabbedOriginalValue = null
    }

    fun grabItem(index: Int) {
        grabbedIndex = index
        grabbedOriginalIndex = index
        grabbedOriginalValue = items.getOrNull(index)
    }

    fun cancelGrabbedMove() {
        val target = autoselectCancelMoveTarget(grabbedIndex, grabbedOriginalIndex, items.size)
        val value = grabbedOriginalValue
        if (target != null && value != null) {
            val current = items.indexOf(value)
            if (current >= 0 && current != target) {
                onReorder(current, target)
            }
            focusRequesters.getOrNull(target)?.requestFocusSafely()
        }
        clearGrabbedItem()
    }

    BackHandler(enabled = grabbedIndex >= 0) {
        cancelGrabbedMove()
    }

    fun scrollMovedItemIntoView(targetIndex: Int) {
        val anchor =
            autoselectMovedItemScrollAnchor(
                targetIndex = targetIndex,
                visibleIndices = listState.layoutInfo.visibleItemsInfo.map { it.index },
            ) ?: return
        dpadReorderScrollScope.launch {
            listState.scrollToItem(anchor)
        }
    }

    fun moveGrabbedItem(direction: Int): Boolean {
        if (grabbedIndex < 0) return false
        val target =
            autoselectGrabbedItemMoveTarget(
                grabbedIndex = grabbedIndex,
                itemCount = items.size,
                direction = direction,
            ) ?: return true
        if (target != grabbedIndex) {
            onReorder(grabbedIndex, target)
            val newReq = focusRequesters.getOrNull(target)
            grabbedIndex = target
            scrollMovedItemIntoView(target)
            newReq?.requestFocusSafely()
        }
        return true
    }

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
            modifier =
                Modifier
                    .fillMaxSize()
                    .onPreviewKeyEvent { event ->
                        if (grabbedIndex < 0) return@onPreviewKeyEvent false
                        when (event.key) {
                            Key.Back,
                            Key.Escape,
                            -> {
                                if (event.type == KeyEventType.KeyDown) {
                                    cancelGrabbedMove()
                                }
                                true
                            }

                            Key.DirectionLeft,
                            Key.DirectionRight,
                            -> {
                                true
                            }

                            else -> {
                                false
                            }
                        }
                    }.repeatVerticalDpadFocus(onMove = ::moveGrabbedItem),
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
                            ).tvFocusBorder()
                            .onKeyEvent { keyEvent ->
                                if (keyEvent.type != KeyEventType.KeyDown) return@onKeyEvent false
                                val k = keyEvent.key
                                if (k == Key.DirectionCenter || k == Key.Enter) {
                                    if (grabbedIndex < 0) {
                                        grabItem(index)
                                    } else {
                                        clearGrabbedItem()
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
                                                        center < edge -> {
                                                            -(1f - (center / edge).coerceIn(0f, 1f)) *
                                                                itemHeightPx / 4f
                                                        }

                                                        center > vpH - edge -> {
                                                            ((center - vpH + edge) / edge).coerceIn(0f, 1f) *
                                                                itemHeightPx / 4f
                                                        }

                                                        else -> {
                                                            0f
                                                        }
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

internal fun autoselectGrabbedItemMoveTarget(
    grabbedIndex: Int,
    itemCount: Int,
    direction: Int,
): Int? {
    if (grabbedIndex < 0 || itemCount <= 0) return null
    return (grabbedIndex + direction).coerceIn(0, itemCount - 1)
}

internal fun autoselectCancelMoveTarget(
    grabbedIndex: Int,
    originalIndex: Int,
    itemCount: Int,
): Int? {
    if (grabbedIndex < 0 || originalIndex < 0 || itemCount <= 0) return null
    return originalIndex.coerceIn(0, itemCount - 1)
}

internal fun autoselectMovedItemScrollAnchor(
    targetIndex: Int,
    visibleIndices: List<Int>,
): Int? {
    if (visibleIndices.isEmpty()) return targetIndex
    val firstVisible = visibleIndices.minOrNull() ?: return targetIndex
    val lastVisible = visibleIndices.maxOrNull() ?: return targetIndex
    val visibleCount = lastVisible - firstVisible + 1
    return when {
        targetIndex < firstVisible -> targetIndex
        targetIndex > lastVisible -> (targetIndex - visibleCount + 1).coerceAtLeast(0)
        else -> null
    }
}

internal fun autoselectHasPendingSave(
    d1MismatchCount: Int,
    d2MismatchCount: Int,
): Boolean = d1MismatchCount > 0 || d2MismatchCount > 0

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
