package com.dxxredux.app.multiplayer

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
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
import androidx.compose.ui.unit.dp
import com.dxxredux.app.dpadTextFieldNavigation
import java.io.File
import java.util.Locale

internal object MultiplayerCallsigns {
    private const val COOP_AUTOSAVE_CALLSIGN = "coopsave"
    private val allowedChars = Regex("^[A-Za-z0-9_-]{1,${CallsignPrefs.MAX_LEN}}$")

    fun scan(
        filesDir: File,
        activeSetDir: File? = null,
    ): List<String> {
        val roots = listOfNotNull(activeSetDir, filesDir).distinctBy { it.absolutePath }
        val seen = linkedMapOf<String, String>()
        roots.flatMap(::pilotDirs).forEach { dir ->
            val files = dir.listFiles() ?: return@forEach
            files.mapNotNull(::callsignFromPilotFile).forEach { callsign ->
                seen.putIfAbsent(callsign.lowercase(Locale.US), callsign)
            }
        }
        return seen.values.sortedBy { it.lowercase(Locale.US) }
    }

    fun sanitizeNewCallsign(input: String): String =
        input
            .filter(::isAllowedCallsignChar)
            .take(CallsignPrefs.MAX_LEN)

    fun isValidNewCallsign(callsign: String): Boolean = allowedChars.matches(callsign)

    fun pickInitialCallsign(
        savedCallsign: String,
        options: List<String>,
    ): String =
        options.firstOrNull { it.equals(savedCallsign, ignoreCase = true) }
            ?: options.firstOrNull()
            ?: ""

    fun mergePendingCallsign(
        options: List<String>,
        pendingCallsign: String?,
    ): List<String> {
        val pending = pendingCallsign?.takeIf(::isValidNewCallsign) ?: return options
        if (options.any { it.equals(pending, ignoreCase = true) }) return options
        return (options + pending).sortedBy { it.lowercase(Locale.US) }
    }

    private fun pilotDirs(root: File): List<File> =
        listOf(
            File(root, "d2x-redux/Players"),
            File(root, "d1x-redux/Players"),
            File(root, "Players"),
            File(root, "d2x-redux"),
            File(root, "d1x-redux"),
            root,
        )

    private fun callsignFromPilotFile(file: File): String? {
        if (!file.isFile || !file.name.endsWith(".plr", ignoreCase = true)) return null
        val callsign = file.nameWithoutExtension
        if (!isValidNewCallsign(callsign)) return null
        if (callsign.equals(COOP_AUTOSAVE_CALLSIGN, ignoreCase = true)) return null
        return callsign
    }

    private fun isAllowedCallsignChar(ch: Char): Boolean =
        ch in 'a'..'z' || ch in 'A'..'Z' || ch in '0'..'9' || ch == '_' || ch == '-'
}

@Composable
internal fun CallsignPickerField(
    selectedCallsign: String,
    callsigns: List<String>,
    onSelect: (String) -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
) {
    var showPicker by remember { mutableStateOf(false) }
    OutlinedButton(
        onClick = { showPicker = true },
        enabled = enabled && callsigns.isNotEmpty(),
        modifier = modifier.fillMaxWidth(),
    ) {
        Column(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(2.dp),
        ) {
            Text("Callsign", style = MaterialTheme.typography.labelSmall)
            Text(
                selectedCallsign.ifBlank {
                    if (callsigns.isEmpty()) "No pilots yet" else "Select callsign"
                },
                style = MaterialTheme.typography.bodyMedium,
            )
        }
    }

    if (showPicker) {
        CallsignPickerDialog(
            callsigns = callsigns,
            onSelect = {
                onSelect(it)
                showPicker = false
            },
            onDismiss = { showPicker = false },
        )
    }
}

@Composable
internal fun NewCallsignButton(
    existingCallsigns: List<String>,
    onCreate: (String) -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
) {
    var showDialog by remember { mutableStateOf(false) }
    Button(onClick = { showDialog = true }, enabled = enabled, modifier = modifier) {
        Icon(Icons.Default.Add, contentDescription = "Create callsign")
        Spacer(Modifier.width(4.dp))
        Text("New")
    }
    if (showDialog) {
        NewCallsignDialog(
            existingCallsigns = existingCallsigns,
            onCreate = {
                onCreate(it)
                showDialog = false
            },
            onDismiss = { showDialog = false },
        )
    }
}

@Composable
private fun CallsignPickerDialog(
    callsigns: List<String>,
    onSelect: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    val dismissFocus = remember { FocusRequester() }
    RequestControllerInitialFocus(dismissFocus)
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Select Callsign") },
        text = {
            LazyColumn(modifier = Modifier.heightIn(max = 320.dp)) {
                items(callsigns) { callsign ->
                    Row(
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .clickable { onSelect(callsign) }
                                .padding(vertical = 8.dp, horizontal = 4.dp),
                    ) {
                        Text(callsign, style = MaterialTheme.typography.bodyLarge)
                    }
                    HorizontalDivider()
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss, modifier = Modifier.focusRequester(dismissFocus)) { Text("Cancel") }
        },
    )
}

@Composable
private fun NewCallsignDialog(
    existingCallsigns: List<String>,
    onCreate: (String) -> Unit,
    onDismiss: () -> Unit,
) {
    var callsign by remember { mutableStateOf("") }
    var textEntryActive by remember { mutableStateOf(false) }
    val dismissFocus = remember { FocusRequester() }
    val dismissOrEndTextEntry =
        rememberControllerTextEntryDismiss(textEntryActive, dismissFocus, { textEntryActive = it }, onDismiss)
    val duplicate = existingCallsigns.any { it.equals(callsign, ignoreCase = true) }
    val valid = MultiplayerCallsigns.isValidNewCallsign(callsign) && !duplicate

    RequestControllerInitialFocus(dismissFocus)
    AlertDialog(
        onDismissRequest = dismissOrEndTextEntry,
        title = { Text("Create Callsign") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                OutlinedTextField(
                    value = callsign,
                    onValueChange = { callsign = MultiplayerCallsigns.sanitizeNewCallsign(it) },
                    label = { Text("Callsign") },
                    singleLine = true,
                    isError = duplicate,
                    supportingText = {
                        if (duplicate) Text("That callsign already exists")
                    },
                    modifier =
                        Modifier
                            .fillMaxWidth()
                            .dpadTextFieldNavigation(up = dismissFocus)
                            .controllerTextEntryFocus { textEntryActive = it },
                )
            }
        },
        confirmButton = {
            TextButton(onClick = { onCreate(callsign) }, enabled = valid) {
                Text("Create")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss, modifier = Modifier.focusRequester(dismissFocus)) { Text("Cancel") }
        },
    )
}
