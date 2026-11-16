package com.dxxredux.app

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.Column
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.dxxredux.app.NavigationAlertDialog as AlertDialog

/** Shares the same dirty-exit policy between visible Back and system/controller Back */
@Composable
internal fun rememberSettingsEditorExit(
    dirty: Boolean,
    onSaveAndLeave: () -> Unit,
    onLeave: () -> Unit,
    saving: Boolean = false,
    mixedImmediateSaves: Boolean = false,
    onPromptOpenChanged: (Boolean) -> Unit = {},
): () -> Unit {
    val draft = rememberSettingsEditorDraft("settings.exit")
    var showPrompt by draft.state("showPrompt") { mutableStateOf(false) }
    DisposableEffect(showPrompt) {
        onPromptOpenChanged(showPrompt)
        onDispose { onPromptOpenChanged(false) }
    }
    val requestExit: () -> Unit = {
        if (!saving) {
            if (dirty) showPrompt = true else onLeave()
        }
    }
    BackHandler(onBack = requestExit)
    if (showPrompt) {
        AlertDialog(
            onDismissRequest = { showPrompt = false },
            title = { Text("Unsaved changes") },
            text = {
                Text(
                    if (mixedImmediateSaves) {
                        "Save your pending changes? Discard only affects unsaved changes; settings already saved stay applied."
                    } else {
                        "Save your changes before leaving?"
                    },
                )
            },
            confirmButton = {
                TextButton(onClick = {
                    showPrompt = false
                    onSaveAndLeave()
                }, enabled = !saving) { Text("Save") }
            },
            dismissButton = {
                Column {
                    TextButton(onClick = {
                        showPrompt = false
                        onLeave()
                    }, enabled = !saving) { Text("Discard") }
                    TextButton(onClick = { showPrompt = false }) { Text("Keep editing") }
                }
            },
        )
    }
    return requestExit
}
