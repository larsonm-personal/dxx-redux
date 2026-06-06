package com.dxxredux.app

import android.graphics.Bitmap
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.FilterChip
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
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

private enum class SaveExplorerMode(
    val label: String,
) {
    SaveSet("Save Set"),
    Recent("Ten Recent"),
    All("All Slots"),
}

private data class SaveExplorerRow(
    val slotIndex: Int,
    val slot: SaveExplorerBridge.SaveExplorerSlot?,
)

@Composable
internal fun SaveExplorerDialog(
    filesDir: File,
    canLaunchGame: (String) -> Boolean,
    onLoadCandidate: (ResumeSaveBridge.ResumeSaveCandidate) -> Unit,
    onChanged: () -> Unit,
    onDismiss: () -> Unit,
) {
    val scope = rememberCoroutineScope()
    var refreshKey by remember { mutableIntStateOf(0) }
    var mode by remember { mutableStateOf(SaveExplorerMode.SaveSet) }
    var selectedGame by remember { mutableStateOf("") }
    var selectedScope by remember { mutableStateOf("single") }
    var selectedPilot by remember { mutableStateOf("") }
    var selectedMission by remember { mutableStateOf("") }
    var orphanOnly by remember { mutableStateOf(false) }
    var pendingDelete by remember { mutableStateOf<SaveExplorerBridge.SaveExplorerSlot?>(null) }
    var deleteError by remember { mutableStateOf<String?>(null) }

    val slotsState =
        produceState<List<SaveExplorerBridge.SaveExplorerSlot>?>(initialValue = null, refreshKey) {
            value = withContext(Dispatchers.IO) { SaveExplorerBridge.listSlots(filesDir) }
        }
    val slots = slotsState.value
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

    val displayedRows =
        remember(slots, mode, selectedGame, selectedScope, selectedPilot, selectedMission, orphanOnly) {
            when (mode) {
                SaveExplorerMode.SaveSet -> {
                    val bySlot =
                        slots
                            .orEmpty()
                            .filter {
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
                            }.associateBy { it.slot }
                    (0..9).map { SaveExplorerRow(it, bySlot[it]) }
                }

                SaveExplorerMode.Recent -> {
                    saveExplorerRecentSlots(slots.orEmpty())
                        .map { SaveExplorerRow(it.slot, it) }
                }

                SaveExplorerMode.All -> {
                    slots
                        .orEmpty()
                        .filter { !orphanOnly || it.orphan }
                        .map { SaveExplorerRow(it.slot, it) }
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
                    .padding(12.dp),
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

                Row(
                    modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(6.dp),
                ) {
                    SaveExplorerMode.values().forEach { chipMode ->
                        FilterChip(
                            selected = mode == chipMode,
                            onClick = { mode = chipMode },
                            label = { Text(chipMode.label, fontSize = 12.sp) },
                        )
                    }
                }

                if (mode == SaveExplorerMode.SaveSet) {
                    Row(
                        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(6.dp),
                    ) {
                        SaveExplorerDropdown("Game", selectedGame, gameOptions) { selectedGame = it }
                        SaveExplorerDropdown("Scope", selectedScope, scopeOptions) { selectedScope = it }
                        SaveExplorerDropdown("Pilot", selectedPilot, pilotOptions) { selectedPilot = it }
                        SaveExplorerDropdown("Level Set", selectedMission, missionOptions) { selectedMission = it }
                    }
                } else if (mode == SaveExplorerMode.All) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Checkbox(checked = orphanOnly, onCheckedChange = { orphanOnly = it })
                        Text("Orphans only", fontSize = 12.sp)
                    }
                }

                if (slots == null) {
                    Box(modifier = Modifier.fillMaxWidth().weight(1f), contentAlignment = Alignment.Center) {
                        Text("Scanning saves...")
                    }
                } else {
                    LazyColumn(
                        modifier = Modifier.fillMaxWidth().weight(1f),
                        verticalArrangement = Arrangement.spacedBy(5.dp),
                    ) {
                        items(displayedRows, key = { row -> "${row.slotIndex}:${row.slot?.path.orEmpty()}" }) { row ->
                            SaveExplorerSlotRow(
                                row = row,
                                canLaunchGame = canLaunchGame,
                                onLoadCandidate = onLoadCandidate,
                                onDelete = { pendingDelete = it },
                            )
                        }
                    }
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
            SaveExplorerThumbnail(thumbnail)
            Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(1.dp)) {
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
                        text = slot.orphanReason.ifBlank { "orphaned" },
                        fontSize = 8.sp,
                        color = MaterialTheme.colorScheme.error,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
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

private fun saveExplorerSecondaryLine(slot: SaveExplorerBridge.SaveExplorerSlot): String {
    val level = ("${slot.levelNum} ${slot.levelName}").trim().ifBlank { slot.missionKey.ifBlank { "Unknown level" } }
    return "$level | ${saveExplorerKindLabel(slot.saveKind)} | ${saveExplorerTime(slot.saveTimeUnixSeconds)} | " +
        saveExplorerSize(slot.sizeBytes)
}

private fun saveExplorerIdentityLine(slot: SaveExplorerBridge.SaveExplorerSlot): String =
    "${slot.game.ifBlank { "unknown" }} / ${slot.scope.ifBlank { "single" }} / " +
        "${slot.pilot.ifBlank { slot.callsign.ifBlank { "unknown" } }} / " +
        "${slot.missionKey.ifBlank { slot.missionName.ifBlank { "unknown" } }} / slot ${slot.slot}"

private fun saveExplorerKindLabel(kind: String): String =
    when (kind) {
        "auto_minimize" -> "Minimize"
        "auto_exit" -> "Exit"
        "auto_progress" -> "Progress"
        "auto_abort" -> "Abort"
        "auto_periodic" -> "Periodic"
        "manual" -> "Manual"
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
