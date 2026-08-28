package com.dxxredux.app

import android.graphics.Bitmap
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.PagerState
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.abs
import kotlin.math.roundToInt

private enum class SaveExplorerMode(
    val label: String,
) {
    Choose("Choose Save"),
    Recent("Most Recent"),
    SaveSet("Save Set"),
    All("All Slots"),
}

private val saveExplorerModes = SaveExplorerMode.values().toList()

internal fun saveExplorerModeLabels(): List<String> = saveExplorerModes.map { it.label }

internal fun saveExplorerDefaultModeLabel(): String = SaveExplorerMode.Choose.label

private fun saveExplorerModeAfter(
    mode: SaveExplorerMode,
    step: Int,
): SaveExplorerMode = saveExplorerModes[saveExplorerPageIndexAfter(mode.ordinal, step)]

internal fun saveExplorerModeLabelAfter(
    currentLabel: String,
    step: Int,
): String {
    val current = saveExplorerModes.first { it.label == currentLabel }
    return saveExplorerModeAfter(current, step).label
}

internal fun saveExplorerLivePageIndex(
    currentPage: Int,
    offsetFraction: Float,
): Int = (currentPage + offsetFraction).roundToInt().coerceIn(saveExplorerModes.indices)

internal fun saveExplorerPageIndexAfter(
    currentPage: Int,
    step: Int,
): Int = (currentPage + step).coerceIn(saveExplorerModes.indices)

internal data class SaveExplorerRow(
    val slotIndex: Int,
    val slot: SaveExplorerBridge.SaveExplorerSlot?,
)

internal data class SaveExplorerDetailRow(
    val label: String,
    val value: String,
)

@Composable
internal fun SaveExplorerDialog(
    filesDir: File,
    resumeOptions: ResumeSaveBridge.ResumeSaveOptions? = null,
    refreshTrigger: Int = 0,
    canLaunchGame: (String) -> Boolean,
    onLoadCandidate: (ResumeSaveBridge.ResumeSaveCandidate) -> Unit,
    onChanged: () -> Unit,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    var refreshKey by remember { mutableIntStateOf(0) }
    val pagerState = rememberPagerState(pageCount = { saveExplorerModes.size })
    var selectedGame by remember { mutableStateOf("") }
    var selectedScope by remember { mutableStateOf("single") }
    var selectedPilot by remember { mutableStateOf("") }
    var selectedMission by remember { mutableStateOf("") }
    var orphanOnly by remember { mutableStateOf(false) }
    var pendingDelete by remember { mutableStateOf<SaveExplorerBridge.SaveExplorerSlot?>(null) }
    var pendingDetails by remember { mutableStateOf<SaveExplorerBridge.SaveExplorerSlot?>(null) }
    var deleteError by remember { mutableStateOf<String?>(null) }

    var slots by remember(refreshKey, refreshTrigger) {
        mutableStateOf<List<SaveExplorerBridge.SaveExplorerSlot>?>(null)
    }
    var slotsProgress by remember(refreshKey, refreshTrigger) {
        mutableStateOf(MetadataLoadProgress("Scanning saves", 0, 2))
    }
    LaunchedEffect(refreshKey, refreshTrigger, filesDir.absolutePath) {
        slots = null
        slotsProgress = MetadataLoadProgress("Scanning saves", 0, 2)
        val loadedSlots = withContext(Dispatchers.IO) { SaveExplorerBridge.listSlots(filesDir) }
        slotsProgress = MetadataLoadProgress("Preparing save list", 1, 2)
        slots = loadedSlots
        slotsProgress = MetadataLoadProgress("Save list ready", 2, 2)
    }
    val gameOptions = remember(slots) { slots?.map { it.game }?.filter { it.isNotBlank() }?.distinct() ?: emptyList() }
    val scopeOptions =
        remember(slots, selectedGame) {
            slots
                ?.filter { selectedGame.isBlank() || it.game == selectedGame }
                ?.map { it.scope.ifBlank { "single" } }
                ?.distinct()
                ?: emptyList()
        }
    val pilotOptions =
        remember(slots, selectedGame, selectedScope) {
            slots
                ?.filter { it.game == selectedGame && it.scope == selectedScope }
                ?.map { it.pilot.ifBlank { it.callsign } }
                ?.filter { it.isNotBlank() }
                ?.distinct()
                ?: emptyList()
        }
    val missionOptions =
        remember(slots, selectedGame, selectedScope, selectedPilot) {
            slots
                ?.filter {
                    it.game == selectedGame &&
                        it.scope == selectedScope &&
                        (selectedPilot.isBlank() || it.pilot == selectedPilot || it.callsign == selectedPilot)
                }?.map { it.missionKey.ifBlank { it.missionName } }
                ?.filter { it.isNotBlank() }
                ?.distinct()
                ?: emptyList()
        }

    LaunchedEffect(gameOptions) {
        if (selectedGame !in gameOptions) selectedGame = gameOptions.firstOrNull().orEmpty()
    }
    LaunchedEffect(scopeOptions) {
        if (selectedScope !in scopeOptions) selectedScope = scopeOptions.firstOrNull() ?: "single"
    }
    LaunchedEffect(pilotOptions) {
        if (selectedPilot !in pilotOptions) selectedPilot = pilotOptions.firstOrNull().orEmpty()
    }
    LaunchedEffect(missionOptions) {
        if (selectedMission !in missionOptions) selectedMission = missionOptions.firstOrNull().orEmpty()
    }

    fun movePage(step: Int) {
        val nextPage = saveExplorerPageIndexAfter(pagerState.currentPage, step)
        if (nextPage != pagerState.currentPage) {
            scope.launch {
                pagerState.animateScrollToPage(nextPage)
            }
        }
    }

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false),
    ) {
        Surface(
            modifier =
                Modifier
                    .fillMaxSize()
                    .onPreviewKeyEvent { event ->
                        if (event.type != KeyEventType.KeyDown) return@onPreviewKeyEvent false
                        when (event.key) {
                            Key.DirectionLeft -> {
                                movePage(-1)
                                true
                            }

                            Key.DirectionRight -> {
                                movePage(1)
                                true
                            }

                            else -> {
                                false
                            }
                        }
                    }.padding(12.dp),
            shape = RoundedCornerShape(8.dp),
            color = MaterialTheme.colorScheme.surface,
        ) {
            Column(
                modifier = Modifier.fillMaxSize().padding(12.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        "Save Explorer",
                        modifier = Modifier.weight(1f),
                        fontSize = 18.sp,
                        fontWeight = FontWeight.SemiBold,
                    )
                    TextButton(onClick = onDismiss) {
                        Text("Close")
                    }
                }

                SaveExplorerModeTabRow(
                    pagerState = pagerState,
                    onModeStep = ::movePage,
                    onModeSelected = { page ->
                        scope.launch {
                            pagerState.animateScrollToPage(page)
                        }
                    },
                )

                HorizontalPager(
                    state = pagerState,
                    modifier = Modifier.fillMaxWidth().weight(1f),
                ) { page ->
                    SaveExplorerPage(
                        mode = saveExplorerModes[page],
                        slots = slots,
                        slotsProgress = slotsProgress,
                        resumeOptions = resumeOptions,
                        selectedGame = selectedGame,
                        gameOptions = gameOptions,
                        onGameSelected = { selectedGame = it },
                        selectedScope = selectedScope,
                        scopeOptions = scopeOptions,
                        onScopeSelected = { selectedScope = it },
                        selectedPilot = selectedPilot,
                        pilotOptions = pilotOptions,
                        onPilotSelected = { selectedPilot = it },
                        selectedMission = selectedMission,
                        missionOptions = missionOptions,
                        onMissionSelected = { selectedMission = it },
                        orphanOnly = orphanOnly,
                        onOrphanOnlyChanged = { orphanOnly = it },
                        canLaunchGame = canLaunchGame,
                        onOpenDetails = { pendingDetails = it },
                        onLoadCandidate = onLoadCandidate,
                        onDelete = { pendingDelete = it },
                    )
                }

                deleteError?.let {
                    Text(
                        text = "Delete failed: $it",
                        color = MaterialTheme.colorScheme.error,
                        fontSize = 12.sp,
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
            }
        }
    }

    pendingDelete?.let { slot ->
        AlertDialog(
            onDismissRequest = {
                pendingDelete = null
                deleteError = null
            },
            title = { Text("Delete Save") },
            text = {
                Text(
                    buildString {
                        append(saveExplorerIdentityLine(slot))
                        append("\n")
                        append(slot.relativePath.ifBlank { slot.path })
                    },
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        scope.launch {
                            val result = withContext(Dispatchers.IO) { SaveExplorerBridge.deleteSlot(filesDir, slot) }
                            if (result.deleted) {
                                pendingDelete = null
                                deleteError = null
                                refreshKey++
                                onChanged()
                            } else {
                                deleteError = result.reason.ifBlank { "unknown" }
                            }
                        }
                    },
                ) {
                    Text("Delete")
                }
            },
            dismissButton = {
                TextButton(
                    onClick = {
                        pendingDelete = null
                        deleteError = null
                    },
                ) {
                    Text("Cancel")
                }
            },
        )
    }

    pendingDetails?.let { slot ->
        SaveExplorerDetailsDialog(
            slot = slot,
            onDismiss = { pendingDetails = null },
        )
    }
}

@Composable
private fun SaveExplorerModeTabRow(
    pagerState: PagerState,
    onModeStep: (Int) -> Unit,
    onModeSelected: (Int) -> Unit,
) {
    val pagePosition = pagerState.currentPage + pagerState.currentPageOffsetFraction
    val activePage = saveExplorerLivePageIndex(pagerState.currentPage, pagerState.currentPageOffsetFraction)
    var tabDragX by remember { mutableFloatStateOf(0f) }
    val density = LocalDensity.current
    val dragOffset = with(density) { tabDragX.toDp() }
    BoxWithConstraints(
        modifier =
            Modifier
                .fillMaxWidth()
                .height(40.dp)
                .clipToBounds()
                .pointerInput(Unit) {
                    detectHorizontalDragGestures(
                        onDragStart = { tabDragX = 0f },
                        onHorizontalDrag = { _, dragAmount -> tabDragX += dragAmount },
                        onDragCancel = { tabDragX = 0f },
                        onDragEnd = {
                            if (abs(tabDragX) >= 36f) {
                                onModeStep(if (tabDragX < 0f) 1 else -1)
                            }
                            tabDragX = 0f
                        },
                    )
                },
    ) {
        val arrowGutter = 30.dp
        val tabCellWidth = 124.dp
        val visibleWidth = maxWidth - arrowGutter * 2f
        val rowOffset = arrowGutter + visibleWidth * 0.5f - tabCellWidth * (pagePosition + 0.5f) + dragOffset
        Row(
            modifier =
                Modifier
                    .offset(x = rowOffset)
                    .width(tabCellWidth * saveExplorerModes.size.toFloat())
                    .height(40.dp),
            horizontalArrangement = Arrangement.spacedBy(4.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            saveExplorerModes.forEachIndexed { page, chipMode ->
                SaveExplorerTabButton(
                    label = chipMode.label,
                    selected = activePage == page,
                    onClick = { onModeSelected(page) },
                    modifier = Modifier.width(tabCellWidth - 4.dp),
                )
            }
        }
        SharedHorizontalScrollArrows(
            canScrollBackward = pagePosition > 0.01f,
            canScrollForward = pagePosition < saveExplorerModes.lastIndex - 0.01f,
            onScrollBackward = { onModeStep(-1) },
            onScrollForward = { onModeStep(1) },
        )
    }
}

@Composable
private fun SaveExplorerTabButton(
    label: String,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier,
) {
    val colorScheme = MaterialTheme.colorScheme
    Surface(
        modifier = modifier.height(34.dp).clickable(onClick = onClick),
        shape = RoundedCornerShape(percent = 50),
        color = if (selected) colorScheme.primaryContainer else colorScheme.surfaceVariant.copy(alpha = 0.72f),
        border = BorderStroke(1.dp, if (selected) colorScheme.primary else colorScheme.outline),
    ) {
        Box(
            modifier = Modifier.fillMaxSize().padding(horizontal = 3.dp),
            contentAlignment = Alignment.Center,
        ) {
            Text(
                label,
                fontSize = 11.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                textAlign = TextAlign.Center,
                color = if (selected) colorScheme.onPrimaryContainer else colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
private fun SaveExplorerPage(
    mode: SaveExplorerMode,
    slots: List<SaveExplorerBridge.SaveExplorerSlot>?,
    slotsProgress: MetadataLoadProgress,
    resumeOptions: ResumeSaveBridge.ResumeSaveOptions?,
    selectedGame: String,
    gameOptions: List<String>,
    onGameSelected: (String) -> Unit,
    selectedScope: String,
    scopeOptions: List<String>,
    onScopeSelected: (String) -> Unit,
    selectedPilot: String,
    pilotOptions: List<String>,
    onPilotSelected: (String) -> Unit,
    selectedMission: String,
    missionOptions: List<String>,
    onMissionSelected: (String) -> Unit,
    orphanOnly: Boolean,
    onOrphanOnlyChanged: (Boolean) -> Unit,
    canLaunchGame: (String) -> Boolean,
    onOpenDetails: (SaveExplorerBridge.SaveExplorerSlot) -> Unit,
    onLoadCandidate: (ResumeSaveBridge.ResumeSaveCandidate) -> Unit,
    onDelete: (SaveExplorerBridge.SaveExplorerSlot) -> Unit,
) {
    when (mode) {
        SaveExplorerMode.Choose -> {
            SaveExplorerChooseSaveTab(
                options = resumeOptions,
                onLoadCandidate = onLoadCandidate,
                modifier = Modifier.fillMaxSize(),
            )
        }

        SaveExplorerMode.Recent -> {
            SaveExplorerSlotPageBody(
                slots = slots,
                slotsProgress = slotsProgress,
                rows =
                    remember(slots) {
                        saveExplorerRecentSlots(slots.orEmpty()).map { SaveExplorerRow(it.slot, it) }
                    },
                canLaunchGame = canLaunchGame,
                onOpenDetails = onOpenDetails,
                onLoadCandidate = onLoadCandidate,
                onDelete = onDelete,
            )
        }

        SaveExplorerMode.SaveSet -> {
            Column(modifier = Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                val filterScrollState = rememberScrollState()
                Box(modifier = Modifier.fillMaxWidth()) {
                    Row(
                        modifier = Modifier.fillMaxWidth().horizontalScroll(filterScrollState),
                        horizontalArrangement = Arrangement.spacedBy(6.dp),
                    ) {
                        SaveExplorerDropdown("Game", selectedGame, gameOptions, onGameSelected)
                        SaveExplorerDropdown("Level Set", selectedMission, missionOptions, onMissionSelected)
                        SaveExplorerDropdown("Scope", selectedScope, scopeOptions, onScopeSelected)
                        SaveExplorerDropdown("Pilot", selectedPilot, pilotOptions, onPilotSelected)
                    }
                    SharedHorizontalScrollArrows(filterScrollState)
                }
                SaveExplorerSlotPageBody(
                    slots = slots,
                    slotsProgress = slotsProgress,
                    rows =
                        remember(slots, selectedGame, selectedScope, selectedPilot, selectedMission) {
                            saveExplorerSaveSetRows(
                                slots.orEmpty(),
                                selectedGame,
                                selectedScope,
                                selectedPilot,
                                selectedMission,
                            )
                        },
                    canLaunchGame = canLaunchGame,
                    onOpenDetails = onOpenDetails,
                    onLoadCandidate = onLoadCandidate,
                    onDelete = onDelete,
                    modifier = Modifier.weight(1f),
                )
            }
        }

        SaveExplorerMode.All -> {
            Column(modifier = Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Checkbox(checked = orphanOnly, onCheckedChange = onOrphanOnlyChanged)
                    Text("Orphans only", fontSize = 12.sp)
                }
                SaveExplorerSlotPageBody(
                    slots = slots,
                    slotsProgress = slotsProgress,
                    rows =
                        remember(slots, orphanOnly) {
                            slots
                                .orEmpty()
                                .filter { !orphanOnly || it.orphan }
                                .map { SaveExplorerRow(it.slot, it) }
                        },
                    canLaunchGame = canLaunchGame,
                    onOpenDetails = onOpenDetails,
                    onLoadCandidate = onLoadCandidate,
                    onDelete = onDelete,
                    modifier = Modifier.weight(1f),
                )
            }
        }
    }
}

@Composable
private fun SaveExplorerSlotPageBody(
    slots: List<SaveExplorerBridge.SaveExplorerSlot>?,
    slotsProgress: MetadataLoadProgress,
    rows: List<SaveExplorerRow>,
    canLaunchGame: (String) -> Boolean,
    onOpenDetails: (SaveExplorerBridge.SaveExplorerSlot) -> Unit,
    onLoadCandidate: (ResumeSaveBridge.ResumeSaveCandidate) -> Unit,
    onDelete: (SaveExplorerBridge.SaveExplorerSlot) -> Unit,
    modifier: Modifier = Modifier.fillMaxSize(),
) {
    if (slots == null) {
        Box(modifier = modifier, contentAlignment = Alignment.Center) {
            MetadataLoadProgressView(slotsProgress)
        }
        return
    }

    val slotListState = rememberLazyListState()
    Box(modifier = modifier) {
        LazyColumn(
            state = slotListState,
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(5.dp),
        ) {
            items(rows, key = { row ->
                "${row.slotIndex}:${row.slot?.path.orEmpty()}"
            }) { row ->
                SaveExplorerSlotRow(
                    row = row,
                    canLaunchGame = canLaunchGame,
                    onOpenDetails = onOpenDetails,
                    onLoadCandidate = onLoadCandidate,
                    onDelete = onDelete,
                )
            }
        }
        SharedLazyListScrollArrows(slotListState)
    }
}

@Composable
private fun SaveExplorerChooseSaveTab(
    options: ResumeSaveBridge.ResumeSaveOptions?,
    onLoadCandidate: (ResumeSaveBridge.ResumeSaveCandidate) -> Unit,
    modifier: Modifier,
) {
    val choices = remember(options) { options?.let(::resumeSaveChoiceRows).orEmpty() }
    if (choices.isEmpty()) {
        Box(modifier = modifier, contentAlignment = Alignment.Center) {
            Text(
                "No alternate resume saves found",
                fontSize = 13.sp,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        return
    }
    val listState = rememberLazyListState()
    Box(modifier = modifier) {
        LazyColumn(
            state = listState,
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            items(choices, key = { it.candidate.path + ":" + it.label }) { choiceRow ->
                SaveExplorerChooseSaveRow(choiceRow, onLoadCandidate)
            }
        }
        SharedLazyListScrollArrows(listState)
    }
}

@Composable
private fun SaveExplorerChooseSaveRow(
    choiceRow: ResumeSaveChoiceRow,
    onLoadCandidate: (ResumeSaveBridge.ResumeSaveCandidate) -> Unit,
) {
    val choice = choiceRow.candidate
    val thumbnail =
        remember(choice.path, choice.saveTimeUnixSeconds, choice.thumbnailRgb6) {
            decodeResumeSaveThumbnail(choice)
        }
    ElevatedCard(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(6.dp),
        colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
    ) {
        Column(
            modifier = Modifier.fillMaxWidth().padding(8.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            SaveExplorerLargeThumbnail(thumbnail)
            Button(
                onClick = { onLoadCandidate(choice) },
                modifier = Modifier.fillMaxWidth(),
                contentPadding = PaddingValues(horizontal = 12.dp, vertical = 10.dp),
            ) {
                Column(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalAlignment = Alignment.Start,
                ) {
                    Text(choiceRow.label, fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
                    Text(
                        resumeChoiceLine(choice),
                        fontSize = 9.sp,
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
            }
        }
    }
}

internal fun saveExplorerSaveSetRows(
    slots: List<SaveExplorerBridge.SaveExplorerSlot>,
    selectedGame: String,
    selectedScope: String,
    selectedPilot: String,
    selectedMission: String,
): List<SaveExplorerRow> {
    val bySlot =
        slots
            .filter {
                it.slot in 0..9 &&
                    it.game == selectedGame &&
                    it.scope == selectedScope &&
                    (
                        selectedPilot.isBlank() || it.pilot == selectedPilot ||
                            it.callsign == selectedPilot
                    ) &&
                    (
                        selectedMission.isBlank() ||
                            it.missionKey == selectedMission ||
                            it.missionName == selectedMission
                    )
            }.groupBy { it.slot }
            .mapValues { (_, slotSaves) ->
                slotSaves.maxWithOrNull(
                    compareBy<SaveExplorerBridge.SaveExplorerSlot> { saveExplorerSortTimestamp(it) }
                        .thenBy { saveExplorerKindPriority(it.saveKind) }
                        .thenByDescending { it.modifiedUnixSeconds },
                )
            }

    val occupied =
        bySlot
            .values
            .filterNotNull()
            .sortedWith(
                compareByDescending<SaveExplorerBridge.SaveExplorerSlot> { saveExplorerSortTimestamp(it) }
                    .thenByDescending { saveExplorerKindPriority(it.saveKind) }
                    .thenByDescending { it.modifiedUnixSeconds }
                    .thenBy { it.slot },
            ).map { SaveExplorerRow(it.slot, it) }
    val empty =
        (0..9)
            .filter { it !in bySlot }
            .map { SaveExplorerRow(it, null) }
    return occupied + empty
}

internal fun saveExplorerRecentSlots(
    slots: List<SaveExplorerBridge.SaveExplorerSlot>,
): List<SaveExplorerBridge.SaveExplorerSlot> =
    slots
        .sortedWith(
            compareByDescending<SaveExplorerBridge.SaveExplorerSlot> { it.saveTimeUnixSeconds }
                .thenByDescending { saveExplorerKindPriority(it.saveKind) }
                .thenBy { it.relativePath.ifBlank { it.path } },
        ).distinctBy { saveExplorerRecentDedupKey(it) }
        .take(10)

private fun saveExplorerSortTimestamp(slot: SaveExplorerBridge.SaveExplorerSlot): Long =
    slot.saveTimeUnixSeconds.takeIf { it > 0L } ?: slot.modifiedUnixSeconds

// Keep in sync with android_save_meta_kind_priority in android_save_meta.c.
private fun saveExplorerKindPriority(kind: String): Int =
    when (kind) {
        "auto_abort" -> 6
        "auto_exit" -> 5
        "auto_minimize" -> 4
        "auto_progress" -> 3
        "auto_periodic" -> 2
        else -> 1
    }

private fun saveExplorerRecentDedupKey(slot: SaveExplorerBridge.SaveExplorerSlot): String {
    if (!slot.saveKind.startsWith("auto_")) {
        return "file:" + slot.path.ifBlank { slot.relativePath }
    }
    return listOf(
        slot.game,
        slot.scope.ifBlank { "single" },
        slot.pilot.ifBlank { slot.callsign }.lowercase(Locale.US),
        slot.missionKey.ifBlank { slot.missionName }.lowercase(Locale.US),
        slot.saveTimeUnixSeconds.toString(),
        slot.levelNum.toString(),
        slot.levelName.lowercase(Locale.US),
        slot.levelSeconds.toString(),
        slot.totalSeconds.toString(),
        slot.sizeBytes.toString(),
    ).joinToString("|")
}

@Composable
private fun SaveExplorerSlotRow(
    row: SaveExplorerRow,
    canLaunchGame: (String) -> Boolean,
    onOpenDetails: (SaveExplorerBridge.SaveExplorerSlot) -> Unit,
    onLoadCandidate: (ResumeSaveBridge.ResumeSaveCandidate) -> Unit,
    onDelete: (SaveExplorerBridge.SaveExplorerSlot) -> Unit,
) {
    val slot = row.slot
    val candidateState =
        produceState<ResumeSaveBridge.ResumeSaveCandidate?>(
            initialValue = null,
            slot?.path,
            slot?.saveTimeUnixSeconds,
        ) {
            value =
                if (slot != null) {
                    withContext(Dispatchers.IO) { slot.toResumeCandidate() }
                } else {
                    null
                }
        }
    val candidate = candidateState.value
    val thumbnail =
        remember(candidate?.path, candidate?.saveTimeUnixSeconds, candidate?.thumbnailRgb6) {
            candidate?.let { decodeResumeSaveThumbnail(it) }
        }

    ElevatedCard(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(6.dp),
        colors = CardDefaults.elevatedCardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp).padding(6.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Row(
                modifier =
                    Modifier
                        .weight(1f)
                        .heightIn(min = 36.dp)
                        .clickable(enabled = slot != null) { slot?.let(onOpenDetails) },
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                SaveExplorerThumbnail(thumbnail)
                Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(1.dp)) {
                    slot?.let(::saveExplorerMissionHeading)?.let { missionName ->
                        Text(
                            text = missionName,
                            fontSize = 12.sp,
                            fontWeight = FontWeight.Bold,
                            color = MaterialTheme.colorScheme.primary,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                    Text(
                        text = if (slot == null) "Slot ${row.slotIndex}: Empty" else saveExplorerPrimaryLine(slot),
                        fontSize = 11.sp,
                        fontWeight = FontWeight.SemiBold,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                    Text(
                        text = slot?.let { saveExplorerSecondaryLine(it) }.orEmpty(),
                        fontSize = 9.sp,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                    if (slot?.orphan == true) {
                        Text(
                            text = saveExplorerStatusMessage(slot),
                            fontSize = 8.sp,
                            color = MaterialTheme.colorScheme.error,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                        )
                    }
                }
            }
            Button(
                onClick = { candidate?.let(onLoadCandidate) },
                enabled = candidate != null && slot != null && canLaunchGame(slot.game),
                modifier = Modifier.height(32.dp),
                contentPadding = PaddingValues(horizontal = 10.dp, vertical = 4.dp),
            ) {
                Text("Load", fontSize = 10.sp)
            }
            TextButton(
                onClick = { slot?.let(onDelete) },
                enabled = slot != null,
                modifier = Modifier.height(32.dp),
                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp),
            ) {
                Text("Delete", fontSize = 10.sp)
            }
        }
    }
}

@Composable
private fun SaveExplorerDetailsDialog(
    slot: SaveExplorerBridge.SaveExplorerSlot,
    onDismiss: () -> Unit,
) {
    val candidateState =
        produceState<ResumeSaveBridge.ResumeSaveCandidate?>(
            initialValue = null,
            slot.path,
            slot.saveTimeUnixSeconds,
        ) {
            value = withContext(Dispatchers.IO) { slot.toResumeCandidate() }
        }
    val candidate = candidateState.value
    val thumbnail =
        remember(candidate?.path, candidate?.saveTimeUnixSeconds, candidate?.thumbnailRgb6) {
            candidate?.let { decodeResumeSaveThumbnail(it) }
        }
    val detailRows = remember(slot) { saveExplorerDetailRows(slot) }

    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false),
    ) {
        Surface(
            modifier =
                Modifier
                    .fillMaxWidth()
                    .widthIn(max = 620.dp)
                    .padding(14.dp),
            shape = RoundedCornerShape(8.dp),
            color = MaterialTheme.colorScheme.surface,
        ) {
            Column(
                modifier =
                    Modifier
                        .padding(14.dp)
                        .verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        "Save Details",
                        modifier = Modifier.weight(1f),
                        fontSize = 18.sp,
                        fontWeight = FontWeight.SemiBold,
                    )
                    TextButton(onClick = onDismiss) {
                        Text("Close")
                    }
                }
                SaveExplorerLargeThumbnail(thumbnail)
                Text(
                    saveExplorerDetailTitle(slot),
                    fontSize = 13.sp,
                    fontWeight = FontWeight.SemiBold,
                    maxLines = 2,
                    overflow = TextOverflow.Ellipsis,
                )
                Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    detailRows.forEach { row ->
                        SaveExplorerDetailLine(row)
                    }
                }
            }
        }
    }
}

@Composable
private fun SaveExplorerLargeThumbnail(thumbnail: Bitmap?) {
    Surface(
        modifier = Modifier.fillMaxWidth().aspectRatio(2f),
        shape = RoundedCornerShape(6.dp),
        color = MaterialTheme.colorScheme.surfaceVariant,
        border = BorderStroke(1.dp, Color.Black),
    ) {
        if (thumbnail != null) {
            Image(
                bitmap = thumbnail.asImageBitmap(),
                contentDescription = "Save screenshot",
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.FillBounds,
            )
        } else {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text("No screenshot", fontSize = 13.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}

@Composable
private fun SaveExplorerDetailLine(row: SaveExplorerDetailRow) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Text(
            row.label,
            modifier = Modifier.width(112.dp),
            fontSize = 11.sp,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
        Text(
            row.value,
            modifier = Modifier.weight(1f),
            fontSize = 11.sp,
        )
    }
}

@Composable
private fun SaveExplorerThumbnail(thumbnail: Bitmap?) {
    Surface(
        modifier = Modifier.size(width = 54.dp, height = 27.dp),
        shape = RoundedCornerShape(4.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, Color.Black),
    ) {
        if (thumbnail != null) {
            Image(
                bitmap = thumbnail.asImageBitmap(),
                contentDescription = "Save thumbnail",
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.FillBounds,
            )
        } else {
            Box(
                modifier = Modifier.fillMaxSize().background(MaterialTheme.colorScheme.surface),
                contentAlignment = Alignment.Center,
            ) {
                Text("No thumbnail", fontSize = 6.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}

@Composable
private fun SaveExplorerDropdown(
    label: String,
    selected: String,
    options: List<String>,
    onSelected: (String) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    Box {
        OutlinedButton(
            onClick = { expanded = true },
            enabled = options.isNotEmpty(),
            contentPadding = PaddingValues(horizontal = 10.dp, vertical = 4.dp),
            modifier = Modifier.height(34.dp),
        ) {
            Text(
                "$label: ${selected.ifBlank { "-" }}",
                fontSize = 11.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
        DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
            options.forEach { option ->
                DropdownMenuItem(
                    text = {
                        Text(option, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    },
                    onClick = {
                        expanded = false
                        onSelected(option)
                    },
                )
            }
        }
    }
}

private fun saveExplorerPrimaryLine(slot: SaveExplorerBridge.SaveExplorerSlot): String {
    val displayGame =
        if (slot.game == "d1") {
            "D1"
        } else if (slot.game == "d2") {
            "D2"
        } else {
            "?"
        }
    val scope = if (slot.scope == "coop") "Coop" else "Single"
    val description = slot.description.ifBlank { slot.callsign.ifBlank { "Save" } }
    return "Slot ${slot.slot}: $description | $displayGame | $scope | ${slot.pilot.ifBlank { slot.callsign }}"
}

internal fun saveExplorerSecondaryLine(slot: SaveExplorerBridge.SaveExplorerSlot): String {
    val level = ("${slot.levelNum} ${slot.levelName}").trim().ifBlank { slot.missionKey.ifBlank { "Unknown level" } }
    return "$level | ${saveMetadataKindLabel(slot.saveKind)} | ${saveExplorerTime(slot.saveTimeUnixSeconds)} | " +
        saveExplorerSize(slot.sizeBytes)
}

private fun saveExplorerIdentityLine(slot: SaveExplorerBridge.SaveExplorerSlot): String {
    val identity =
        "${slot.game.ifBlank { "unknown" }} / ${slot.scope.ifBlank { "single" }} / " +
            "${slot.pilot.ifBlank { slot.callsign.ifBlank { "unknown" } }} / " +
            "${slot.missionKey.ifBlank { slot.missionName.ifBlank { "unknown" } }} / slot ${slot.slot}"
    return listOfNotNull(saveExplorerMissionHeading(slot), identity).joinToString(" / ")
}

internal fun saveExplorerDetailRows(slot: SaveExplorerBridge.SaveExplorerSlot): List<SaveExplorerDetailRow> =
    buildList {
        if (saveExplorerMissionHeading(slot) != null) {
            add(SaveExplorerDetailRow("Level Set", saveExplorerMissionSetLabel(slot)))
        }
        add(SaveExplorerDetailRow("Game", "${resumeGameDisplayName(slot.game)} (${slot.game.ifBlank { "unknown" }})"))
        if (saveExplorerMissionHeading(slot) == null) {
            add(SaveExplorerDetailRow("Level Set", saveExplorerMissionSetLabel(slot)))
        }
        add(SaveExplorerDetailRow("Scope", if (slot.scope == "coop") "Coop" else "Single-player"))
        add(SaveExplorerDetailRow("Pilot", slot.pilot.ifBlank { slot.callsign.ifBlank { "Unknown" } }))
        add(SaveExplorerDetailRow("Description", slot.description.ifBlank { "Save" }))
        add(SaveExplorerDetailRow("Save Kind", saveMetadataKindLabel(slot.saveKind)))
        add(SaveExplorerDetailRow("Saved At", formatResumeSaveTime(slot.saveTimeUnixSeconds)))
        add(SaveExplorerDetailRow("Level", saveExplorerLevelLabel(slot)))
        add(SaveExplorerDetailRow("Level Time", formatResumeDuration(slot.levelSeconds)))
        add(SaveExplorerDetailRow("Total Time", formatResumeDuration(slot.totalSeconds)))
        add(SaveExplorerDetailRow("Difficulty", saveExplorerDifficultyLabel(slot)))
        add(SaveExplorerDetailRow("Music", saveMetadataMusicTypeLabel(slot.musicType)))
        add(SaveExplorerDetailRow("Slot", slot.slot.takeIf { it >= 0 }?.toString() ?: "Unknown"))
        add(SaveExplorerDetailRow("Size", saveExplorerSize(slot.sizeBytes)))
        add(SaveExplorerDetailRow("Metadata", if (slot.metadataBacked) "Present" else "Missing or incompatible"))
        if (slot.orphan) {
            add(SaveExplorerDetailRow("Status", saveExplorerStatusMessage(slot)))
        }
        add(SaveExplorerDetailRow("Path", slot.relativePath.ifBlank { slot.path }))
    }

internal fun saveExplorerStatusMessage(slot: SaveExplorerBridge.SaveExplorerSlot): String {
    val reason = slot.orphanReason.ifBlank { "orphaned" }
    return when (reason) {
        "not_loadable_from_launcher" -> {
            if (slot.scope == "coop") {
                "Co-op save: use Multiplayer > Host LAN Game > Restore from save"
            } else {
                "Not loadable directly from Save Explorer"
            }
        }

        "metadata_footer_missing" -> {
            "Metadata missing or incompatible"
        }

        else -> {
            reason
        }
    }
}

private fun saveExplorerDetailTitle(slot: SaveExplorerBridge.SaveExplorerSlot): String {
    val description = slot.description.ifBlank { "Save" }
    return saveExplorerMissionHeading(slot)?.let { "$it | $description" }
        ?: "$description | ${saveExplorerMissionSetLabel(slot)}"
}

internal fun saveExplorerMissionHeading(slot: SaveExplorerBridge.SaveExplorerSlot): String? {
    val key = slot.missionKey.trim()
    if (key.isBlank() || key.equals("d1", ignoreCase = true) || key.equals("d2", ignoreCase = true)) return null
    return slot.missionName.trim().ifBlank { key }
}

private fun saveExplorerMissionSetLabel(slot: SaveExplorerBridge.SaveExplorerSlot): String {
    val key = slot.missionKey.ifBlank { "unknown" }
    val name = slot.missionName
    return if (name.isBlank() || name == key) key else "$name ($key)"
}

private fun saveExplorerLevelLabel(slot: SaveExplorerBridge.SaveExplorerSlot): String {
    val levelName = slot.levelName.ifBlank { slot.missionKey.ifBlank { "Unknown level" } }
    return if (slot.levelNum != 0) "${slot.levelNum} $levelName" else levelName
}

private fun saveExplorerDifficultyLabel(slot: SaveExplorerBridge.SaveExplorerSlot): String {
    if (!slot.difficultyChanged) return "Unchanged"
    return "${saveExplorerDifficultyName(slot.difficultyMin)} to ${saveExplorerDifficultyName(slot.difficultyMax)}"
}

private fun saveExplorerDifficultyName(value: Int): String =
    when (value) {
        0 -> "Trainee"
        1 -> "Rookie"
        2 -> "Hotshot"
        3 -> "Ace"
        4 -> "Insane"
        else -> "Unknown"
    }

private fun saveExplorerTime(unixSeconds: Long): String {
    if (unixSeconds <= 0L) return "Unknown"
    return SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US).format(Date(unixSeconds * 1000L))
}

private fun saveExplorerSize(bytes: Long): String =
    when {
        bytes >= 1024L * 1024L -> String.format(Locale.US, "%.1f MB", bytes / (1024.0 * 1024.0))
        bytes >= 1024L -> String.format(Locale.US, "%.1f KB", bytes / 1024.0)
        bytes > 0L -> "$bytes B"
        else -> "-"
    }
