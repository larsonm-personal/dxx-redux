package com.dxxredux.app.multiplayer

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.dxxredux.app.FileSetManager
import com.dxxredux.app.GameFileFormats
import com.dxxredux.app.dpadTextFieldNavigation
import java.io.File

/**
 * Scans game data directories for available missions.
 *
 * Duplicated constants from C (keep in sync):
 *   d2/main/mission.h: FULL_MISSION_FILENAME="d2", D1_MISSION_FILENAME="descent",
 *                       SHAREWARE_MISSION_FILENAME="d2demo", MISSION_DIR="missions/"
 *   d1/main/mission.h: D1_MISSION_FILENAME="" (empty for D1 engine), MISSION_DIR="missions/"
 * Display names mirror D1_MISSION_NAME, OEM_MISSION_NAME, etc. from those headers.
 */
object MissionScanner {
    data class MissionInfo(
        val filename: String, // passed to nativeSetAutoHost (e.g. "d2", "descent", "mymod")
        val displayName: String,
        val levelCount: Int = 0, // 0 = unknown
        val anarchyOnly: Boolean = false,
        val isBuiltin: Boolean = false,
    )

    // -- mirrors d2/main/mission.h --
    private val D2_BUILTINS =
        listOf(
            MissionInfo("d2", "Descent 2: Counterstrike!", levelCount = 30, isBuiltin = true),
            MissionInfo("descent", "Descent: First Strike", levelCount = 27, isBuiltin = true),
        )

    // -- mirrors d1/main/mission.h (D1_MISSION_FILENAME is "") --
    private val D1_BUILTINS =
        listOf(
            MissionInfo("", "Descent: First Strike", levelCount = 27, isBuiltin = true),
        )

    fun scan(
        setDir: File,
        game: String,
    ): List<MissionInfo> {
        val builtins = if (game == "d2") D2_BUILTINS else D1_BUILTINS
        val missions = builtins.toMutableList()
        val seen = builtins.map { it.filename.lowercase() }.toMutableSet()

        val dirs = listOf(setDir, File(setDir, "missions"))

        for (dir in dirs) {
            val files = dir.listFiles() ?: continue
            for (file in files) {
                if (!file.isFile) continue
                val descriptorGame = GameFileFormats.gameForDescriptor(file.name) ?: continue
                if (game != "d2" && descriptorGame != game) continue
                val basename = file.nameWithoutExtension
                if (basename.lowercase() in seen) continue
                seen.add(basename.lowercase())
                val info = parseMissionFile(file, basename)
                if (info != null) missions.add(info)
            }
        }

        return missions.sortedWith(compareBy({ !it.isBuiltin }, { it.displayName.lowercase() }))
    }

    private fun parseMissionFile(
        file: File,
        basename: String,
    ): MissionInfo? =
        try {
            val descriptor = GameFileFormats.parseMissionDescriptor(file.name, file.readText())
            MissionInfo(
                filename = basename,
                displayName = descriptor.displayName,
                levelCount = descriptor.declaredLevelCount ?: descriptor.levelNames.size,
                anarchyOnly = descriptor.type.equals("anarchy", ignoreCase = true),
            )
        } catch (_: Exception) {
            MissionInfo(filename = basename, displayName = basename)
        }
}

/**
 * A composable that shows the currently-selected mission and opens a picker dialog on tap.
 * Drop-in replacement for the OutlinedTextField that both host dialogs previously used.
 */
@Composable
fun MissionPickerField(
    selectedFilename: String?,
    game: String,
    onSelect: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    val context = LocalContext.current
    val fsm = remember { FileSetManager(context.filesDir) }
    val setDir = remember(fsm) { fsm.getSetDir(fsm.getActive()) }
    val missions = remember(game, setDir) { MissionScanner.scan(setDir, game) }
    val displayText =
        remember(selectedFilename, missions) {
            if (selectedFilename == null) {
                ""
            } else {
                missions.find { it.filename == selectedFilename }?.displayName ?: selectedFilename
            }
        }

    var showPicker by remember { mutableStateOf(false) }

    OutlinedButton(
        onClick = { showPicker = true },
        modifier = modifier.fillMaxWidth(),
    ) {
        Column(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(2.dp),
        ) {
            Text("Mission", style = MaterialTheme.typography.labelSmall)
            Text(displayText.ifBlank { "Tap to select" }, style = MaterialTheme.typography.bodyMedium)
        }
    }

    if (showPicker) {
        MissionPickerDialog(
            missions = missions,
            onSelect = {
                onSelect(it)
                showPicker = false
            },
            onDismiss = { showPicker = false },
        )
    }
}

@Composable
private fun MissionPickerDialog(
    missions: List<MissionScanner.MissionInfo>,
    onSelect: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    var filter by remember { mutableStateOf("") }
    var textEntryActive by remember { mutableStateOf(false) }
    val dismissFocus = remember { FocusRequester() }
    val dismissOrEndTextEntry =
        rememberControllerTextEntryDismiss(textEntryActive, dismissFocus, { textEntryActive = it }, onDismiss)

    RequestControllerInitialFocus(dismissFocus)

    val filtered =
        remember(filter, missions) {
            if (filter.isBlank()) {
                missions
            } else {
                missions.filter {
                    it.displayName.contains(filter, ignoreCase = true) ||
                        it.filename.contains(filter, ignoreCase = true)
                }
            }
        }

    AlertDialog(
        onDismissRequest = dismissOrEndTextEntry,
        title = { Text("Select Mission") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                if (missions.size > 5) {
                    OutlinedTextField(
                        value = filter,
                        onValueChange = { filter = it },
                        placeholder = { Text("Search...") },
                        singleLine = true,
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .dpadTextFieldNavigation(up = dismissFocus)
                                .controllerTextEntryFocus { textEntryActive = it },
                    )
                }
                LazyColumn(modifier = Modifier.heightIn(max = 320.dp)) {
                    items(filtered) { m ->
                        Row(
                            modifier =
                                Modifier
                                    .fillMaxWidth()
                                    .clickable { onSelect(m.filename) }
                                    .padding(vertical = 8.dp, horizontal = 4.dp),
                        ) {
                            Column {
                                Text(m.displayName, style = MaterialTheme.typography.bodyLarge)
                                if (m.filename.isNotEmpty() && m.filename != m.displayName) {
                                    Text(
                                        m.filename,
                                        style = MaterialTheme.typography.bodySmall,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                                    )
                                }
                                if (m.anarchyOnly) {
                                    Text(
                                        "Anarchy only",
                                        style = MaterialTheme.typography.labelSmall,
                                        color = MaterialTheme.colorScheme.error,
                                    )
                                }
                            }
                        }
                        HorizontalDivider()
                    }
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss, modifier = Modifier.focusRequester(dismissFocus)) { Text("Cancel") }
        },
    )
}
