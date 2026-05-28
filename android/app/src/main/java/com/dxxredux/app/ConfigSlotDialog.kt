package com.dxxredux.app

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

private val slotButtonTextSize = 9.sp
private val slotDialogTextSize = 12.sp
private val slotButtonPadding = PaddingValues(horizontal = 4.dp, vertical = 0.dp)
private val slotTextButtonPadding = PaddingValues(horizontal = 6.dp, vertical = 2.dp)

private enum class SlotPromptMode {
    NEW,
    DUPLICATE,
}

@Composable
internal fun ConfigSlotDialog(
    title: String,
    slotNames: List<String>,
    activeIndex: Int,
    onSelectSlot: (Int) -> Unit,
    onRenameActiveSlot: (String) -> Unit,
    onNewSlot: (String) -> Unit,
    onDuplicateActiveSlot: (String) -> Unit,
    onDeleteActiveSlot: () -> Unit,
    onDismiss: () -> Unit,
) {
    val safeActiveIndex = activeIndex.coerceIn(0, (slotNames.size - 1).coerceAtLeast(0))
    val activeName = slotNames.getOrElse(safeActiveIndex) { DEFAULT_CONFIG_SLOT_NAME }
    val activeIsDefault = safeActiveIndex == 0
    val focusManager = LocalFocusManager.current
    var renameText by remember { mutableStateOf(activeName) }
    var lastSubmittedName by remember { mutableStateOf(activeName) }
    var renameFieldHadFocus by remember { mutableStateOf(false) }
    var promptMode by remember { mutableStateOf<SlotPromptMode?>(null) }
    var promptName by remember { mutableStateOf("") }
    var showDeleteConfirm by remember { mutableStateOf(false) }
    var slotDropdownExpanded by remember { mutableStateOf(false) }

    LaunchedEffect(safeActiveIndex, activeName) {
        renameText = activeName
        lastSubmittedName = activeName
        renameFieldHadFocus = false
    }

    fun submitRename() {
        if (activeIsDefault) return
        val normalizedName = normalizeConfigSlotName(renameText, activeName)
        renameText = normalizedName
        if (normalizedName != lastSubmittedName) {
            lastSubmittedName = normalizedName
            onRenameActiveSlot(normalizedName)
        }
    }

    val scrollState = rememberScrollState()

    AlertDialog(
        onDismissRequest = {
            submitRename()
            onDismiss()
        },
        title = { Text(title) },
        text = {
            Box(modifier = Modifier.heightIn(max = 360.dp)) {
                Column(
                    modifier =
                        Modifier
                            .verticalScroll(scrollState)
                            .padding(end = 8.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    Text("Active slot", fontSize = 11.sp)
                    Box {
                        OutlinedButton(
                            onClick = {
                                submitRename()
                                slotDropdownExpanded = true
                            },
                            modifier = Modifier.fillMaxWidth().height(32.dp),
                            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
                        ) {
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically,
                            ) {
                                Text("[$safeActiveIndex] $activeName", fontSize = 11.sp, maxLines = 1)
                                Icon(
                                    Icons.Default.KeyboardArrowDown,
                                    contentDescription = "Choose slot",
                                )
                            }
                        }
                        DropdownMenu(
                            expanded = slotDropdownExpanded,
                            onDismissRequest = { slotDropdownExpanded = false },
                        ) {
                            slotNames.forEachIndexed { slotIndex, slotName ->
                                DropdownMenuItem(
                                    text = {
                                        Text(
                                            "[$slotIndex] $slotName",
                                            fontSize = slotDialogTextSize,
                                            fontWeight =
                                                if (slotIndex == safeActiveIndex) {
                                                    FontWeight.Bold
                                                } else {
                                                    FontWeight.Normal
                                                },
                                        )
                                    },
                                    onClick = {
                                        submitRename()
                                        onSelectSlot(slotIndex)
                                        slotDropdownExpanded = false
                                    },
                                )
                            }
                        }
                    }

                    OutlinedTextField(
                        value = renameText,
                        onValueChange = { renameText = it.take(CONFIG_SLOT_NAME_MAX_LENGTH) },
                        enabled = !activeIsDefault,
                        singleLine = true,
                        keyboardOptions = KeyboardOptions(imeAction = ImeAction.Done),
                        keyboardActions =
                            KeyboardActions(
                                onDone = {
                                    submitRename()
                                    focusManager.clearFocus()
                                },
                            ),
                        label = {
                            Text(
                                if (activeIsDefault) "Slot name" else "Edit slot name",
                                fontSize = 11.sp,
                            )
                        },
                        modifier =
                            Modifier
                                .fillMaxWidth()
                                .onFocusChanged { focusState ->
                                    if (renameFieldHadFocus && !focusState.isFocused) {
                                        submitRename()
                                    }
                                    renameFieldHadFocus = focusState.isFocused
                                },
                    )

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(6.dp),
                    ) {
                        OutlinedButton(
                            onClick = {
                                submitRename()
                                promptName = ""
                                promptMode = SlotPromptMode.NEW
                            },
                            modifier = Modifier.weight(1f).height(30.dp),
                            contentPadding = slotButtonPadding,
                        ) {
                            Text("New", fontSize = slotButtonTextSize, maxLines = 1)
                        }
                        OutlinedButton(
                            onClick = {
                                submitRename()
                                promptName = ""
                                promptMode = SlotPromptMode.DUPLICATE
                            },
                            modifier = Modifier.weight(1f).height(30.dp),
                            contentPadding = slotButtonPadding,
                        ) {
                            Text("Duplicate", fontSize = slotButtonTextSize, maxLines = 1)
                        }
                        OutlinedButton(
                            onClick = {
                                submitRename()
                                showDeleteConfirm = true
                            },
                            enabled = !activeIsDefault,
                            modifier = Modifier.weight(1f).height(30.dp),
                            contentPadding = slotButtonPadding,
                        ) {
                            Text("Delete", fontSize = slotButtonTextSize, maxLines = 1)
                        }
                    }
                }
                SharedScrollArrows(scrollState)
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    submitRename()
                    onDismiss()
                },
                contentPadding = slotTextButtonPadding,
            ) {
                Text("Close", fontSize = slotButtonTextSize)
            }
        },
    )

    val pendingPromptMode = promptMode
    if (pendingPromptMode != null) {
        AlertDialog(
            onDismissRequest = { promptMode = null },
            title = {
                Text(if (pendingPromptMode == SlotPromptMode.NEW) "New Slot" else "copy to new slot")
            },
            text = {
                Column {
                    OutlinedTextField(
                        value = promptName,
                        onValueChange = { promptName = it.take(CONFIG_SLOT_NAME_MAX_LENGTH) },
                        singleLine = true,
                        label = { Text("Slot name", fontSize = 11.sp) },
                        modifier = Modifier.fillMaxWidth(),
                    )
                    Spacer(Modifier.height(4.dp))
                    Text("Maximum $CONFIG_SLOT_NAME_MAX_LENGTH characters", fontSize = 10.sp)
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        if (pendingPromptMode == SlotPromptMode.NEW) {
                            onNewSlot(promptName)
                        } else {
                            onDuplicateActiveSlot(promptName)
                        }
                        promptMode = null
                    },
                    enabled = promptName.trim().isNotEmpty(),
                    contentPadding = slotTextButtonPadding,
                ) {
                    Text("OK", fontSize = slotButtonTextSize)
                }
            },
            dismissButton = {
                TextButton(onClick = { promptMode = null }, contentPadding = slotTextButtonPadding) {
                    Text("Cancel", fontSize = slotButtonTextSize)
                }
            },
        )
    }

    if (showDeleteConfirm) {
        AlertDialog(
            onDismissRequest = { showDeleteConfirm = false },
            title = { Text("Delete Slot") },
            text = { Text("Delete '$activeName'?") },
            confirmButton = {
                Button(
                    onClick = {
                        onDeleteActiveSlot()
                        showDeleteConfirm = false
                    },
                    contentPadding = slotTextButtonPadding,
                ) {
                    Text("OK", fontSize = slotButtonTextSize)
                }
            },
            dismissButton = {
                TextButton(onClick = { showDeleteConfirm = false }, contentPadding = slotTextButtonPadding) {
                    Text("Cancel", fontSize = slotButtonTextSize)
                }
            },
        )
    }
}
