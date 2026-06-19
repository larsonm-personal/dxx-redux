package com.dxxredux.app.multiplayer

import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusProperties
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.dxxredux.app.dpadTextFieldNavigation
import com.dxxredux.app.tvFocusBorder

/**
 * Shared dialog for creating a game/lobby, used by both LAN hosting and
 * matchmaking lobby creation. Includes all settings: game, mission, mode,
 * difficulty, level, max players, and coop save restore.
 */
@Composable
internal fun CreateGameDialog(
    title: String = "Create Game",
    confirmLabel: String = "Create",
    onCreate: (
        game: String,
        mission: String?,
        mode: String,
        maxPlayers: Int,
        difficulty: Int,
        levelNum: Int,
        coopQol: Boolean,
        fullDeathSpew: Boolean,
        playerSpewNoExpire: Boolean,
        clientsCanRequestRewind: Boolean,
        restrictNonCoopFovToBase: Boolean,
    ) -> Unit,
    onDismiss: () -> Unit,
) {
    val context = LocalContext.current
    val defaults = remember { HostGameDefaults.load(context) }
    var game by remember { mutableStateOf(defaults.game) }
    var mission by remember { mutableStateOf(defaults.mission) }
    var mode by remember { mutableStateOf(defaults.mode) }
    var maxPlayersText by remember { mutableStateOf(defaults.maxPlayers.toString()) }
    var difficulty by remember { mutableStateOf(defaults.difficulty) }
    var levelNumText by remember { mutableStateOf(defaults.levelNum.toString()) }
    var coopQol by remember { mutableStateOf(defaults.coopQol) }
    var fullDeathSpew by remember { mutableStateOf(defaults.fullDeathSpew) }
    var playerSpewNoExpire by remember { mutableStateOf(defaults.playerSpewNoExpire) }
    var clientsCanRequestRewind by remember { mutableStateOf(defaults.clientsCanRequestRewind) }
    var restrictNonCoopFovToBase by remember { mutableStateOf(defaults.restrictNonCoopFovToBase) }
    var textEntryActive by remember { mutableStateOf(false) }
    val dialogFocus = remember { FocusRequester() }
    val missionFocus = remember { FocusRequester() }
    val difficultyFocus = remember { FocusRequester() }
    val levelFocus = remember { FocusRequester() }
    val maxPlayersFocus = remember { FocusRequester() }
    val createFocus = remember { FocusRequester() }
    val dismissOrEndTextEntry =
        rememberControllerTextEntryDismiss(textEntryActive, dialogFocus, { textEntryActive = it }, onDismiss)

    RequestControllerInitialFocus(dialogFocus, revealFocusOnRequest = false)

    // Coop auto-saves and progress
    val coopSaves =
        if (mode == "coop") {
            val saves = readCoopAutosaveHistory(context.filesDir, game, mission, context)
            val checkpoint = readCoopProgressAsEntry(context.filesDir, game, mission, context)
            if (checkpoint != null) saves + checkpoint else saves
        } else {
            emptyList()
        }
    val coopRestoreSelection =
        if (mode == "coop") {
            readCoopRestoreSelection(context.filesDir, game)
        } else {
            null
        }
    var selectedSave by remember(coopSaves, coopRestoreSelection) {
        mutableStateOf(initialCoopSaveSelection(coopSaves, coopRestoreSelection))
    }

    val coopResumeLevel =
        if (mode == "coop" && coopSaves.none { it.type == "full_save" }) {
            readCoopProgress(context.filesDir, game, mission)
        } else {
            null
        }

    // When a save is selected, auto-set the level to match
    LaunchedEffect(selectedSave) {
        val save = selectedSave
        if (save != null && save.level > 0) {
            levelNumText = save.level.toString()
        } else if (save == null && coopSaves.isNotEmpty()) {
            val fallback = coopResumeLevel?.let { it + 1 } ?: defaults.levelNum
            levelNumText = fallback.toString()
        }
    }

    LaunchedEffect(coopResumeLevel) {
        if (coopResumeLevel != null && coopResumeLevel > 0) {
            levelNumText = (coopResumeLevel + 1).toString()
        }
    }

    val difficultyNames = listOf("Trainee", "Rookie", "Hotshot", "Ace", "Insane")

    AlertDialog(
        onDismissRequest = dismissOrEndTextEntry,
        title = {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.fillMaxWidth(),
            ) {
                Text(title, modifier = Modifier.weight(1f))
                TextButton(
                    onClick = onDismiss,
                    modifier =
                        Modifier
                            .focusRequester(dialogFocus)
                            .focusProperties { down = missionFocus },
                ) {
                    Text("Cancel")
                }
            }
        },
        text = {
            val scrollState = rememberScrollState()
            Box {
                Column(
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                    modifier =
                        Modifier
                            .showControllerFocusOnDpad(dialogFocus)
                            .verticalScroll(scrollState),
                ) {
                    // Game selector
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        listOf("d1", "d2").forEach { g ->
                            if (g == game) {
                                Button(onClick = {}) { Text(g.uppercase()) }
                            } else {
                                OutlinedButton(onClick = {
                                    game = g
                                    val saved = HostGameDefaults.load(context)
                                    mission =
                                        if (saved.game == g) {
                                            saved.mission
                                        } else {
                                            HostGameDefaults.defaultMissionForGame(g)
                                        }
                                }) { Text(g.uppercase()) }
                            }
                        }
                    }
                    MissionPickerField(
                        selectedFilename = mission,
                        game = game,
                        onSelect = { mission = it },
                        modifier = Modifier.focusRequester(missionFocus),
                    )
                    // Mode selector
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        listOf("anarchy", "coop").forEach { m ->
                            if (m == mode) {
                                Button(onClick = {}) { Text(m.replaceFirstChar { it.uppercase() }) }
                            } else {
                                OutlinedButton(onClick = { mode = m }) {
                                    Text(m.replaceFirstChar { it.uppercase() })
                                }
                            }
                        }
                    }
                    if (mode == "coop") {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            Switch(
                                checked = coopQol,
                                onCheckedChange = { coopQol = it },
                                modifier = Modifier.tvFocusBorder(),
                            )
                            Column {
                                Text(
                                    "Coop QoL (guidebot, arrows, warp)",
                                    style = MaterialTheme.typography.labelMedium,
                                )
                                Text(
                                    "Host-side coop helper features for this session",
                                    fontSize = 11.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                        }
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            Switch(
                                checked = clientsCanRequestRewind,
                                onCheckedChange = { clientsCanRequestRewind = it },
                                modifier = Modifier.tvFocusBorder(),
                            )
                            Column {
                                Text(
                                    "Allow client rewind requests",
                                    style = MaterialTheme.typography.labelMedium,
                                )
                                Text(
                                    "Clients can ask this host to use its rewind buffer",
                                    fontSize = 11.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                        }
                    }
                    if (mode != "coop") {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            Switch(
                                checked = restrictNonCoopFovToBase,
                                onCheckedChange = { restrictNonCoopFovToBase = it },
                                modifier = Modifier.tvFocusBorder(),
                            )
                            Column {
                                Text(
                                    "Restrict client FOV to base",
                                    style = MaterialTheme.typography.labelMedium,
                                )
                                Text(
                                    "Clients use base FOV for this non-coop session",
                                    fontSize = 11.sp,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                                )
                            }
                        }
                    }
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Switch(
                            checked = fullDeathSpew,
                            onCheckedChange = { fullDeathSpew = it },
                            modifier = Modifier.tvFocusBorder(),
                        )
                        Text(
                            "100% death spew",
                            style = MaterialTheme.typography.labelMedium,
                        )
                    }
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Switch(
                            checked = playerSpewNoExpire,
                            onCheckedChange = { playerSpewNoExpire = it },
                            modifier = Modifier.tvFocusBorder(),
                        )
                        Text(
                            "Player spew does not expire",
                            style = MaterialTheme.typography.labelMedium,
                        )
                    }
                    // Difficulty dropdown
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Text("Difficulty", style = MaterialTheme.typography.labelMedium)
                        Box {
                            var diffExpanded by remember { mutableStateOf(false) }
                            OutlinedButton(
                                onClick = { diffExpanded = true },
                                modifier =
                                    Modifier
                                        .focusRequester(difficultyFocus)
                                        .focusProperties { down = levelFocus },
                            ) {
                                Text(difficultyNames[difficulty])
                            }
                            DropdownMenu(
                                expanded = diffExpanded,
                                onDismissRequest = { diffExpanded = false },
                            ) {
                                difficultyNames.forEachIndexed { idx, name ->
                                    DropdownMenuItem(
                                        text = { Text(name) },
                                        onClick = {
                                            difficulty = idx
                                            diffExpanded = false
                                        },
                                    )
                                }
                            }
                        }
                    }
                    // Level number + Max players on same row
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        OutlinedTextField(
                            value = levelNumText,
                            onValueChange = { levelNumText = it.filter { c -> c.isDigit() } },
                            label = { Text("Level") },
                            singleLine = true,
                            modifier =
                                Modifier
                                    .weight(1f)
                                    .focusRequester(levelFocus)
                                    .focusProperties {
                                        up = difficultyFocus
                                        right = maxPlayersFocus
                                        down = createFocus
                                    }.dpadTextFieldNavigation(up = difficultyFocus, down = createFocus)
                                    .controllerTextEntryFocus { textEntryActive = it },
                        )
                        OutlinedTextField(
                            value = maxPlayersText,
                            onValueChange = { maxPlayersText = it.filter { c -> c.isDigit() } },
                            label = { Text("Max Players") },
                            singleLine = true,
                            modifier =
                                Modifier
                                    .weight(1f)
                                    .focusRequester(maxPlayersFocus)
                                    .focusProperties {
                                        up = difficultyFocus
                                        left = levelFocus
                                        down = createFocus
                                    }.dpadTextFieldNavigation(up = difficultyFocus, down = createFocus)
                                    .controllerTextEntryFocus { textEntryActive = it },
                        )
                    }
                    if (coopSaves.isNotEmpty()) {
                        Text("Restore from save:", style = MaterialTheme.typography.labelMedium)
                        Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                            val noSaveSelected = selectedSave == null
                            if (noSaveSelected) {
                                Button(
                                    onClick = { selectedSave = null },
                                    modifier = Modifier.fillMaxWidth(),
                                ) { Text("Start fresh (no restore)") }
                            } else {
                                OutlinedButton(
                                    onClick = { selectedSave = null },
                                    modifier = Modifier.fillMaxWidth(),
                                ) { Text("Start fresh (no restore)") }
                            }
                            coopSaves.forEach { save ->
                                val typeTag = if (save.type == "checkpoint") "[Chk]" else "[Save]"
                                val scoreStr = if (save.totalScore > 0) " ${save.totalScore}pts" else ""
                                val label =
                                    "$typeTag L${save.level} - ${save.numPlayers}p" +
                                        " - ${save.callsigns.joinToString()}" +
                                        "$scoreStr - ${formatTimeAgo(save.timestamp)}"
                                if (selectedSave == save) {
                                    Button(
                                        onClick = {},
                                        modifier = Modifier.fillMaxWidth(),
                                    ) { Text(label, fontSize = 11.sp, maxLines = 2) }
                                } else {
                                    OutlinedButton(
                                        onClick = { selectedSave = save },
                                        modifier = Modifier.fillMaxWidth(),
                                    ) { Text(label, fontSize = 11.sp, maxLines = 2) }
                                }
                            }
                        }
                    } else if (coopResumeLevel != null) {
                        Text(
                            "Last completed: Level $coopResumeLevel",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.primary,
                        )
                    }
                }
                ScrollArrows(scrollState)
            }
        },
        confirmButton = {
            val maxPlayers = maxPlayersText.toIntOrNull() ?: 0
            val levelNum = levelNumText.toIntOrNull() ?: 1
            Button(
                onClick = {
                    HostGameDefaults.save(
                        context,
                        HostGameDefaults.Defaults(
                            game = game,
                            mission = mission,
                            mode = mode,
                            difficulty = difficulty,
                            levelNum = levelNum,
                            maxPlayers = maxPlayers,
                            coopQol = coopQol,
                            fullDeathSpew = fullDeathSpew,
                            playerSpewNoExpire = playerSpewNoExpire,
                            clientsCanRequestRewind = clientsCanRequestRewind,
                            restrictNonCoopFovToBase = restrictNonCoopFovToBase,
                        ),
                    )
                    // slot = -1 for checkpoint entries (no save file to load)
                    val restoreSlot = selectedSave?.slot?.takeIf { it >= 0 }
                    writeCoopRestoreSlot(context.filesDir, game, restoreSlot)
                    MultiplayerResumePrefs.saveRestoreSelection(context, game, selectedSave)
                    onCreate(
                        game,
                        mission,
                        mode,
                        maxPlayers,
                        difficulty,
                        levelNum,
                        coopQol,
                        fullDeathSpew,
                        playerSpewNoExpire,
                        clientsCanRequestRewind,
                        restrictNonCoopFovToBase,
                    )
                },
                enabled = mission != null && maxPlayers in 2..8 && levelNum >= 1,
                modifier = Modifier.focusRequester(createFocus),
            ) {
                Text(confirmLabel)
            }
        },
    )
}

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
